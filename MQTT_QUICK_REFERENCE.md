# MQTT Quick Reference Card

## Connection Settings

```
Broker: 508641aa1f684bb2b2f1170750e8c5ac.s1.eu.hivemq.cloud
Port: 8883 (TLS/SSL)
Username: [Your HiveMQ Cloud Username]
Password: [Your HiveMQ Cloud Password]
Client ID: ebike-[MAC Address]
```

## Topic Structure

```
ebike/
├── battery/
│   ├── 0/status          # Battery 0 telemetry (1s interval)
│   ├── 1/status          # Battery 1 telemetry (1s interval)
│   ├── 2/status          # Battery 2 telemetry (1s interval)
│   └── all/status        # Combined battery data (1s interval)
├── system/
│   ├── status            # System health (5s interval)
│   └── config            # Device configuration (retained)
└── can/
    └── raw               # Raw CAN messages (optional)
```

## Message Formats

### Battery Status (`ebike/battery/0/status`)
```json
{
  "id": 0,
  "name": "Battery 1",
  "voltage": 52.4,
  "current": 3.2,
  "power": 167.68,
  "soc": 85,
  "temp1": 28.5,
  "temp2": 27.8,
  "enabled": true,
  "has_can_data": true,
  "data_fresh": true,
  "timestamp": 12345
}
```

### All Batteries (`ebike/battery/all/status`)
```json
{
  "batteries": [
    {"id": 0, "name": "Front", "voltage": 52.4, "current": 3.2, "power": 167.68, "soc": 85},
    {"id": 1, "name": "Rear", "voltage": 51.8, "current": 2.1, "power": 108.78, "soc": 82}
  ],
  "total_power": 276.46,
  "total_current": 5.3,
  "avg_voltage": 52.1,
  "timestamp": 12345
}
```

### System Status (`ebike/system/status`)
```json
{
  "uptime": 12345,
  "free_heap": 145632,
  "wifi_rssi": -45,
  "wifi_ssid": "MyNetwork",
  "ip_address": "192.168.1.100",
  "mqtt_publishes": 1234,
  "mqtt_failures": 5,
  "timestamp": 12345
}
```

## Command Line Testing

### Subscribe to All Topics
```bash
mosquitto_sub -h YOUR_BROKER.hivemq.cloud -p 8883 \
  -u YOUR_USERNAME -P YOUR_PASSWORD \
  -t "ebike/#" --capath /etc/ssl/certs
```

### Subscribe to Specific Battery
```bash
mosquitto_sub -h YOUR_BROKER.hivemq.cloud -p 8883 \
  -u YOUR_USERNAME -P YOUR_PASSWORD \
  -t "ebike/battery/0/status"
```

### Watch JSON Formatted
```bash
mosquitto_sub -h YOUR_BROKER.hivemq.cloud -p 8883 \
  -u YOUR_USERNAME -P YOUR_PASSWORD \
  -t "ebike/battery/all/status" | jq .
```

## Python Example

```python
import paho.mqtt.client as mqtt
import json

def on_message(client, userdata, msg):
    data = json.loads(msg.payload)
    print(f"{msg.topic}: {data}")

client = mqtt.Client()
client.username_pw_set("YOUR_USERNAME", "YOUR_PASSWORD")
client.tls_set()
client.on_message = on_message
client.connect("YOUR_BROKER.hivemq.cloud", 8883)
client.subscribe("ebike/#")
client.loop_forever()
```

## Home Assistant Sensors

```yaml
sensor:
  - platform: mqtt
    name: "eBike Battery Voltage"
    state_topic: "ebike/battery/0/status"
    value_template: "{{ value_json.voltage }}"
    unit_of_measurement: "V"

  - platform: mqtt
    name: "eBike Battery Power"
    state_topic: "ebike/battery/0/status"
    value_template: "{{ value_json.power }}"
    unit_of_measurement: "W"

  - platform: mqtt
    name: "eBike Total Power"
    state_topic: "ebike/battery/all/status"
    value_template: "{{ value_json.total_power }}"
    unit_of_measurement: "W"
```

## Troubleshooting

| Issue | Check |
|-------|-------|
| Not connecting | WiFi status, broker URL, credentials |
| Messages not appearing | Connection status, publish interval |
| Connection drops | WiFi signal, broker stability |
| Authentication failed | Username/password in HiveMQ Cloud |

## Serial Monitor Messages

```
[MQTT] Connected successfully!           ✅ Working
[MQTT] Connection failed: Bad credentials  ❌ Fix credentials
[MQTT] Connection timeout                ❌ Network/broker issue
[MQTT] Published to ebike/battery/0/status  ✅ Publishing OK
[MQTT] Publish failed to ebike/...       ❌ Connection issue
```

## Data Rates

- **Battery Status**: Every 1 second (configurable)
- **System Status**: Every 5 seconds
- **Configuration**: On connect (retained)
- **Bandwidth**: ~1 KB/second (2 batteries)
