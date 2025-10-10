#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_SGP30.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ssid = "GULDAN-R";
const char* password = "SlozhniyParol1";

IPAddress local_IP(192, 168, 178, 73);  
IPAddress gateway(192, 168, 178, 1);    
IPAddress subnet(255, 255, 255, 0);   
IPAddress dns(192, 168, 178, 1);        

Adafruit_SGP30 sgp;

WiFiServer server(80);

ADC_MODE(ADC_VCC);

void setup() {
  Serial.begin(115200);
  Wire.begin(D1, D2);  // SDA = D1 (GPIO5), SCL = D2 (GPIO4) — IC2 for Display and CO2 sensor
  i2cScan();

  if (!display.begin(0x3C, true)) {  
    Serial.println("SH1106 display not found at 0x3C");
    for (;;);
  }

  displayText("Booting...", 2, 5, 30);
  Serial.println("Booting...");
  delay(300);

  wifiConnect();

  if (!sgp.begin()) {
    Serial.println("❌ SGP30 not found! Check wiring.");
    displayText("SGP30 not found! Check wiring.", 1, 0, 0);
    delay(3000);
  }
  Serial.println("✅ SGP30 sensor initialized!");
  displayText("SGP30 sensor initialized!", 1, 0, 0);
  delay(300);

  server.begin();
  Serial.println("✅ Web server started");
  displayText("Web server started", 1, 0, 0);
  delay(300);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Lost Wi-Fi connection!");
    displayText("Lost Wi-Fi connection!", 1, 0, 0);
  }

  uint16_t co2, tvoc;
  if (readCO2(co2, tvoc)) {
      Serial.println("eCO2: " + String(co2) + " ppm\tTVOC: " + String(tvoc) + " ppb");
      displayText("eCO2: " + String(co2) + "\nTVOC: " + String(tvoc) + "", 2, 0, 0);
  } else {
    Serial.println("❌ Failed to read SGP30");
    displayText("Failed to read SGP30", 1, 0, 0);
  }

  webServerWork();
  delay(1000);
}

void displayText(String text, int size, int left, int top) {
  display.clearDisplay();
  display.setTextSize(size);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(left, top);
  display.println(text);
  display.display();
}

void wifiConnect(){
  displayText("Connecting to "+ String(ssid) + "...", 1, 5, 10);
  Serial.println("Connecting to "+ String(ssid) + "...");

  if (!WiFi.config(local_IP, gateway, subnet, dns)) {
    Serial.println("❌ Failed to configure static IP");
    displayText("Failed to configure static IP", 1, 0, 0);
    for (;;);
  }

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ Wi-Fi connected!\nIP address: " + WiFi.localIP().toString());
  displayText("Wi-Fi connected!\nIP address:\n" + WiFi.localIP().toString(), 1, 0, 0);
  delay(1000);
}

void i2cScan() {
  Serial.println("\n🔍 Scanning I2C bus...");
  
  byte count = 0;
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.print("✅ Found I2C device at 0x");
      Serial.println(address, HEX);
      count++;
    }
  }

  if (count == 0) {
    Serial.println("❌ No I2C devices found!");
  } else {
    Serial.println("✅ Scan complete.");
  }
}

bool readCO2(uint16_t &eco2, uint16_t &tvoc) {
  if (sgp.IAQmeasure()) {
    eco2 = sgp.eCO2;   // CO₂ в ppm
    tvoc = sgp.TVOC;   // TVOC в ppb
    return true;
  } else {
    return false;
  }
}

void webServerWork() {
  WiFiClient client = server.available();
  if (!client) return;

  String request = client.readStringUntil('\n');  // read the entire request line
  request.trim();  // remove \r and \n
  
  Serial.println("📡 RAW REQUEST: [" + request + "]");  // debug
  
  if (request.startsWith("GET /metrics")) {
    uint16_t eco2 = 0;
    uint16_t tvoc = 0;
    bool co2_ok = readCO2(eco2, tvoc);
  
    // System metrics
    unsigned long uptime_sec = millis() / 1000;
    uint32_t free_heap = ESP.getFreeHeap();
    int wifi_rssi = WiFi.RSSI();
  
    String body;
  
    // === CO₂ и TVOC ===
    body += "# HELP air_co2_ppm Equivalent CO2 concentration in ppm\n";
    body += "# TYPE air_co2_ppm gauge\n";
    body += "air_co2_ppm ";
    body += String(co2_ok ? eco2 : 0);
    body += "\n";
  
    body += "# HELP air_tvoc_ppb Total Volatile Organic Compounds in ppb\n";
    body += "# TYPE air_tvoc_ppb gauge\n";
    body += "air_tvoc_ppb ";
    body += String(co2_ok ? tvoc : 0);
    body += "\n";
  
    body += "# HELP air_sensor_status 1 if last CO2 read was successful, 0 otherwise\n";
    body += "# TYPE air_sensor_status gauge\n";
    body += "air_sensor_status ";
    body += String(co2_ok ? 1 : 0);
    body += "\n";
  
    // === ESP8266 ===
    body += "# HELP system_uptime_seconds System uptime in seconds\n";
    body += "# TYPE system_uptime_seconds counter\n";
    body += "system_uptime_seconds ";
    body += String(uptime_sec);
    body += "\n";
  
    body += "# HELP system_free_heap_bytes Free heap memory in bytes\n";
    body += "# TYPE system_free_heap_bytes gauge\n";
    body += "system_free_heap_bytes ";
    body += String(free_heap);
    body += "\n";

    body += "# HELP system_sketch_size_bytes Sketch size (flash)\n";
    body += "# TYPE system_sketch_size_bytes gauge\n";
    body += "system_sketch_size_bytes " + String(ESP.getSketchSize()) + "\n";

    body += "# HELP system_free_sketch_space_bytes Free flash space for OTA\n";
    body += "# TYPE system_free_sketch_space_bytes gauge\n";
    body += "system_free_sketch_space_bytes " + String(ESP.getFreeSketchSpace()) + "\n";

    body += "# HELP system_heap_fragmentation_percent Heap fragmentation percent\n";
    body += "# TYPE system_heap_fragmentation_percent gauge\n";
    body += "system_heap_fragmentation_percent " + String(ESP.getHeapFragmentation()) + "\n";

    body += "# HELP system_max_free_block_size_bytes Largest free RAM block\n";
    body += "# TYPE system_max_free_block_size_bytes gauge\n";
    body += "system_max_free_block_size_bytes " + String(ESP.getMaxFreeBlockSize()) + "\n";
  
    body += "# HELP wifi_rssi_dbm WiFi signal strength (dBm)\n";
    body += "# TYPE wifi_rssi_dbm gauge\n";
    body += "wifi_rssi_dbm ";
    body += String(wifi_rssi);
    body += "\n";

    body += "# HELP wifi_channel Current WiFi channel\n";
    body += "# TYPE wifi_channel gauge\n";
    body += "wifi_channel " + String(WiFi.channel()) + "\n";
  
    body += "# HELP wifi_connected 1 if connected to WiFi, 0 otherwise\n";
    body += "# TYPE wifi_connected gauge\n";
    body += "wifi_connected ";
    body += String(WiFi.isConnected() ? 1 : 0);
    body += "\n";

    uint32_t vcc = ESP.getVcc(); // mV
    body += "# HELP system_vcc_mv Supply voltage in millivolts\n";
    body += "# TYPE system_vcc_mv gauge\n";
    body += "system_vcc_mv " + String(vcc) + "\n";
  
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain; charset=utf-8");
    client.print("Content-Length: ");
    client.println(body.length());
    client.println("Connection: close");
    client.println();                
    client.print(body);              
  
    client.flush();
    delay(1);
    client.stop();
    Serial.println("✅ Sent /metrics");
    return;
  }

  // Default HTML page for non-/metrics requests
  String html =
    "<!DOCTYPE html><html><head><meta charset='utf-8'><title>ESP8266</title></head>"
    "<body><h1>🌿 ESP8266 CO₂ Monitor</h1>"
    "<p>Visit <a href='/metrics'>/metrics</a> to get Prometheus metrics</p>"
    "</body></html>";

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.print("Content-Length: ");
  client.println(html.length());
  client.println();
  client.print(html);
  client.stop();

  Serial.println("✅ Sent HTML response");
}
