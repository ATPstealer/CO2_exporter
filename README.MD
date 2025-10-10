```
docker run -d --rm  --name prometheus  -p 9090:9090  -v ${PWD}/prometheus.yml:/etc/prometheus/prometheus.yml prom/prometheus
```
