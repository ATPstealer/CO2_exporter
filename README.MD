NodeMcu ESP8266 OLED + Sensor Sgp30 shield D1 mini

Collect CO2 and TVOC metrics in Promethues via Wi-Fi. 

YouTube video:
https://www.youtube.com/watch?v=mJeS49Id614


Prometheus run windows:
```
docker run -d --rm  --name prometheus  -p 9090:9090  -v ${PWD}/prometheus.yml:/etc/prometheus/prometheus.yml prom/prometheus
```
