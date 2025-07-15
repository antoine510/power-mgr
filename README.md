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

## Systemd

### `/etc/systemd/system/power-mgrd.service`
```bash
[Unit]
Description=Power manager interface software

[Service]
ExecStart=/home/orangepi/power-mgr/Install/bin/power-mgr
WorkingDirectory=/home/orangepi/power-mgr/Install/bin
Restart=on-failure
RestartSec=30

[Install]
WantedBy=multi-user.target
```
