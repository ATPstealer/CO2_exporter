#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_SGP30.h>
#include <SensirionI2cScd4x.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ssid = "GULDAN-R";
const char* password = "123";

IPAddress local_IP(192, 168, 178, 73);  
IPAddress gateway(192, 168, 178, 1);    
IPAddress subnet(255, 255, 255, 0);   
IPAddress dns(192, 168, 178, 1);        

unsigned long lastSensorRead = 0;
#define SENSOR_READ_TIMEOUT 10000

Adafruit_SGP30 sgp;
bool sgp30_ok;
uint16_t sgp30_co2, sgp30_tvoc;

SensirionI2cScd4x scd4x;
bool scd41_ok;
uint16_t scd41_co2;
float scd41_temperature, scd41_humidity;

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

  scd4x.begin(Wire, SCD41_I2C_ADDR_62);
  scd4x.stopPeriodicMeasurement();
  uint16_t error;
  char errorMessage[256];
  error = scd4x.startPeriodicMeasurement();
  if (error) {
    Serial.println("Error SCD41 init: " + String(error));
    displayText("Error SCD41 init: " + String(error), 1, 0, 0);
  }
  Serial.println("SCD41 sensor initialized!");
  displayText("SCD41 sensor initialized!", 1, 0, 0);
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


  if (lastSensorRead + SENSOR_READ_TIMEOUT < millis()) {
    lastSensorRead = millis();
    String text = "";

    if (readSCD41(scd41_co2, scd41_temperature, scd41_humidity)) {
      scd41_ok = 1;
      Serial.println("CO2:  " + String(scd41_co2) + " ppm\tTemperature: " + String(scd41_temperature) + "C\tHumidity: " + String(scd41_humidity) + "%");
      text += "CO2: " + String(scd41_co2) + " ppm\nTemp: " + String(scd41_temperature) + "C\nHum:  " + String(scd41_humidity) + "%\n\n";

    } else {
      scd41_ok = 0;
      Serial.println("❌ Failed to read SCD41");
      displayText("Failed to read SCD41", 1, 0, 0);
      delay(1000);
    }

    if (readCO2(sgp30_co2, sgp30_tvoc)) {
      sgp30_ok = 1;
      Serial.println("eCO2: " + String(sgp30_co2) + " ppm\tTVOC: " + String(sgp30_tvoc) + " ppb");
      text += "eCO2: " + String(sgp30_co2) + " ppm\nTVOC: " + String(sgp30_tvoc) + " ppb";
    } else {
      sgp30_ok = 0;
      Serial.println("❌ Failed to read SGP30");
      displayText("Failed to read SGP30", 1, 0, 0);
      delay(1000);
    }
  
    displayText(text, 0, 0, 0);
  }

  webServerWork();
  delay(100);
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

bool readSCD41(uint16_t &co2, float &temperature, float &humidity) {
  uint16_t error = scd4x.readMeasurement(co2, temperature, humidity);
  if (error) {
    return false;
  }

  if (co2 == 0) {
    return false;
  }

  return true;
}

void webServerWork() {
  WiFiClient client = server.available();
  if (!client) return;

  String request = client.readStringUntil('\n');  // read the entire request line
  request.trim();  // remove \r and \n
  
  Serial.println("📡 RAW REQUEST: [" + request + "]");  // debug
  
  if (request.startsWith("GET /metrics")) {
    // System metrics
    unsigned long uptime_sec = millis() / 1000;
    uint32_t free_heap = ESP.getFreeHeap();
    int wifi_rssi = WiFi.RSSI();
  
    String body;
  
    // === SGP30 CO₂ и TVOC ===
    body += "# HELP sgp30_co2_ppm Equivalent CO2 concentration from SGP30 in ppm\n";
    body += "# TYPE sgp30_co2_ppm gauge\n";
    body += "sgp30_co2_ppm ";
    body += String(sgp30_ok ? sgp30_co2 : 0);
    body += "\n";

    body += "# HELP sgp30_tvoc_ppb Total Volatile Organic Compounds from SGP30 in ppb\n";
    body += "# TYPE sgp30_tvoc_ppb gauge\n";
    body += "sgp30_tvoc_ppb ";
    body += String(sgp30_ok ? sgp30_tvoc : 0);
    body += "\n";

    body += "# HELP sgp30_sensor_status 1 if last SGP30 read was successful, 0 otherwise\n";
    body += "# TYPE sgp30_sensor_status gauge\n";
    body += "sgp30_sensor_status ";
    body += String(sgp30_ok ? 1 : 0);
    body += "\n";

    // === SCD41 CO₂, Temperature, Humidity ===
    body += "# HELP scd41_co2_ppm CO2 concentration from SCD41 in ppm\n";
    body += "# TYPE scd41_co2_ppm gauge\n";
    body += "scd41_co2_ppm ";
    body += String(scd41_ok ? scd41_co2 : 0);
    body += "\n";

    body += "# HELP scd41_temperature_celsius Temperature from SCD41 in °C\n";
    body += "# TYPE scd41_temperature_celsius gauge\n";
    body += "scd41_temperature_celsius ";
    body += String(scd41_ok ? scd41_temperature : 0);
    body += "\n";

    body += "# HELP scd41_humidity_percent Relative humidity from SCD41 in %\n";
    body += "# TYPE scd41_humidity_percent gauge\n";
    body += "scd41_humidity_percent ";
    body += String(scd41_ok ? scd41_humidity : 0);
    body += "\n";

    body += "# HELP scd41_sensor_status 1 if last SCD41 read was successful, 0 otherwise\n";
    body += "# TYPE scd41_sensor_status gauge\n";
    body += "scd41_sensor_status ";
    body += String(scd41_ok ? 1 : 0);
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
