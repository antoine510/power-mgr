# power-mgr

This project allows the use of one or more home power meter of type PZEM-004T to track power consumption or production.
The produced data is sent to an influxdb instance.

## Configuration
Configuration is provided by placing a file `config.yaml` in the execution directory.
The configuration file takes the following form:
```yaml
influx:
  ip: 127.0.0.1
  org: MyOrg
  bucket: MyBucket
  token: <INFLUXDB_API_TOKEN>
power-meters:
  Mains:
    dev: /dev/ttyUSB0
```
