# MQTT Implementation Guide

## Overview

The eBike Battery CANBUS Monitor now includes full MQTT support for publishing battery telemetry and system status to an MQTT broker. The implementation uses TLS/SSL encryption and is pre-configured for HiveMQ Cloud.

## Features

✅ **TLS/SSL Encryption** - Secure connection to MQTT brokers
✅ **Auto-Reconnect** - Automatic reconnection with exponential backoff
✅ **Configurable** - All settings stored in NVS and configurable via web interface
✅ **HiveMQ Cloud Ready** - Pre-configured with root CA certificate
✅ **JSON Payloads** - Structured data in JSON format
✅ **Multiple Topics** - Separate topics for batteries, system, and CAN data
✅ **QoS Support** - Configurable Quality of Service levels

## Default Configuration

The system is pre-configured with HiveMQ Cloud settings:

```cpp
Broker: 508641aa1f684bb2b2f1170750e8c5ac.s1.eu.hivemq.cloud
Port: 8883 (TLS/SSL)
Topic Prefix: ebike
```

**You MUST configure your HiveMQ Cloud username and password** via the web interface or settings.

## MQTT Topics

### Battery Status Topics

Individual battery status (published every 1 second by default):
```
ebike/battery/0/status
ebike/battery/1/status
ebike/battery/2/status
...
```

**Payload Example:**
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

### Combined Battery Status

All batteries combined (published every 1 second by default):
```
ebike/battery/all/status
```

**Payload Example:**
```json
{
  "batteries": [
    {
      "id": 0,
      "name": "Front Pack",
      "voltage": 52.4,
      "current": 3.2,
      "power": 167.68,
      "soc": 85
    },
    {
      "id": 1,
      "name": "Rear Pack",
      "voltage": 51.8,
      "current": 2.1,
      "power": 108.78,
      "soc": 82
    }
  ],
  "total_power": 276.46,
  "total_current": 5.3,
  "avg_voltage": 52.1,
  "timestamp": 12345
}
```

### System Status

System health and statistics (published every 5 seconds):
```
ebike/system/status
```

**Payload Example:**
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

### System Configuration

Device configuration (published on connect, retained):
```
ebike/system/config
```

**Payload Example:**
```json
{
  "num_batteries": 2,
  "can_bitrate": 500000,
  "sample_interval_ms": 100,
  "publish_interval_ms": 1000,
  "batteries": [
    {
      "id": 0,
      "name": "Front Pack",
      "enabled": true
    },
    {
      "id": 1,
      "name": "Rear Pack",
      "enabled": true
    }
  ]
}
```

### Raw CAN Data (Optional)

Raw CAN messages (if enabled):
```
ebike/can/raw
```

**Payload Example:**
```json
{
  "id": "0x202",
  "dlc": 8,
  "data": "CF3E00000000",
  "timestamp": 12345678
}
```

## Configuration

### Via Web Interface

1. Connect to the device's web interface
2. Navigate to Configuration page
3. Enter MQTT settings:
   - **Broker**: Your HiveMQ Cloud cluster URL (or other MQTT broker)
   - **Port**: 8883 for TLS (default), or 1883 for non-TLS
   - **Username**: Your HiveMQ Cloud username
   - **Password**: Your HiveMQ Cloud password
   - **Topic Prefix**: Base topic (default: "ebike")

4. Click "Save Configuration"
5. Device will automatically connect to MQTT broker

### Via API

```bash
# Update MQTT settings
curl -X POST http://device-ip/api/config \
  -H "Content-Type: application/json" \
  -d '{
    "mqtt_broker": "your-broker.hivemq.cloud",
    "mqtt_port": 8883,
    "mqtt_username": "your-username",
    "mqtt_password": "your-password",
    "mqtt_topic_prefix": "ebike"
  }'
```

### Direct NVS Configuration

You can also configure via serial commands (future feature).

## HiveMQ Cloud Setup

### 1. Create HiveMQ Cloud Account

1. Go to https://console.hivemq.cloud/
2. Sign up for a free account
3. Create a new cluster (free tier available)

### 2. Create Credentials

1. In your cluster dashboard, go to "Access Management"
2. Click "Add Credentials"
3. Enter a username and password
4. Note these credentials - you'll need them

### 3. Get Cluster URL

1. In cluster overview, find your cluster URL
2. It will look like: `XXXXXXXX.s1.eu.hivemq.cloud`
3. This is your MQTT broker hostname

### 4. Configure Device

1. Enter the cluster URL as the broker
2. Port: 8883 (TLS is mandatory for HiveMQ Cloud)
3. Enter your username and password
4. Save configuration

## Using Other MQTT Brokers

The implementation supports any MQTT broker with TLS/SSL. Common options:

### Mosquitto (Self-Hosted)

```
Broker: your-server.com
Port: 8883 (if TLS enabled) or 1883 (no TLS)
Username: (optional)
Password: (optional)
```

**Note:** For TLS with self-hosted Mosquitto, you may need to update the root CA certificate in `mqtt_client.cpp`.

### AWS IoT Core

Requires custom certificate configuration (not currently supported - needs code modification).

### Azure IoT Hub

Requires SAS token authentication (not currently supported).

### CloudMQTT / Other Cloud Providers

Should work with TLS on port 8883. Use provider's root CA certificate.

## Connection Status

### Serial Monitor Output

The MQTT client logs connection status to the serial monitor:

```
[MQTT] Initializing MQTT client...
[MQTT] Broker: 508641aa1f684bb2b2f1170750e8c5ac.s1.eu.hivemq.cloud:8883
[MQTT] Topic prefix: ebike
[MQTT] TLS configured with root CA certificate
[MQTT] MQTT client initialized
[MQTT] Attempting to connect...
[MQTT] Connecting to 508641aa1f684bb2b2f1170750e8c5ac.s1.eu.hivemq.cloud:8883 as ebike-AABBCCDDEEFF
[MQTT] Using authentication (username: your-username)
[MQTT] Connected successfully!
[MQTT] Published to ebike/system/config (156 bytes)
[MQTT] Published to ebike/battery/0/status (234 bytes)
```

### Connection Errors

Common error messages and solutions:

| Error | Cause | Solution |
|-------|-------|----------|
| `Bad credentials` | Wrong username/password | Check HiveMQ Cloud credentials |
| `Connection timeout` | Network issue or wrong broker | Verify broker URL and WiFi connection |
| `Unauthorized` | Insufficient permissions | Check HiveMQ Cloud access rights |
| `WiFi not connected` | No WiFi | Fix WiFi connection first |
| `No broker configured` | MQTT not set up | Configure MQTT broker in settings |

### Auto-Reconnect

The client automatically reconnects with exponential backoff:
- First retry: 5 seconds
- Second retry: 10 seconds
- Third retry: 20 seconds
- Max retry delay: 60 seconds

Connection status is logged on state changes.

## Testing MQTT Connection

### Using MQTT Explorer

1. Download MQTT Explorer: http://mqtt-explorer.com/
2. Create new connection:
   - **Name**: eBike Monitor
   - **Host**: your-hivemq-cluster.hivemq.cloud
   - **Port**: 8883
   - **Protocol**: mqtt/tls
   - **Username**: your-username
   - **Password**: your-password
   - **Validate certificate**: Yes
3. Connect
4. Subscribe to `ebike/#` to see all topics

### Using mosquitto_sub

```bash
# Subscribe to all topics
mosquitto_sub -h your-broker.hivemq.cloud -p 8883 \
  -u your-username -P your-password \
  -t "ebike/#" \
  --capath /etc/ssl/certs

# Subscribe to specific battery
mosquitto_sub -h your-broker.hivemq.cloud -p 8883 \
  -u your-username -P your-password \
  -t "ebike/battery/0/status"
```

### Using Python (paho-mqtt)

```python
import paho.mqtt.client as mqtt
import json

def on_connect(client, userdata, flags, rc):
    print(f"Connected with result code {rc}")
    client.subscribe("ebike/#")

def on_message(client, userdata, msg):
    print(f"Topic: {msg.topic}")
    data = json.loads(msg.payload)
    print(json.dumps(data, indent=2))

client = mqtt.Client()
client.username_pw_set("your-username", "your-password")
client.tls_set()  # Use system CA certificates
client.on_connect = on_connect
client.on_message = on_message

client.connect("your-broker.hivemq.cloud", 8883, 60)
client.loop_forever()
```

## Home Assistant Integration

### Configuration

Add to `configuration.yaml`:

```yaml
mqtt:
  broker: your-broker.hivemq.cloud
  port: 8883
  username: your-username
  password: your-password

sensor:
  - platform: mqtt
    name: "eBike Front Battery Voltage"
    state_topic: "ebike/battery/0/status"
    unit_of_measurement: "V"
    value_template: "{{ value_json.voltage }}"

  - platform: mqtt
    name: "eBike Front Battery Current"
    state_topic: "ebike/battery/0/status"
    unit_of_measurement: "A"
    value_template: "{{ value_json.current }}"

  - platform: mqtt
    name: "eBike Front Battery Power"
    state_topic: "ebike/battery/0/status"
    unit_of_measurement: "W"
    value_template: "{{ value_json.power }}"

  - platform: mqtt
    name: "eBike Front Battery SOC"
    state_topic: "ebike/battery/0/status"
    unit_of_measurement: "%"
    value_template: "{{ value_json.soc }}"

  - platform: mqtt
    name: "eBike Total Power"
    state_topic: "ebike/battery/all/status"
    unit_of_measurement: "W"
    value_template: "{{ value_json.total_power }}"
```

### Lovelace Dashboard

```yaml
type: entities
title: eBike Battery Monitor
entities:
  - entity: sensor.ebike_front_battery_voltage
    name: Voltage
  - entity: sensor.ebike_front_battery_current
    name: Current
  - entity: sensor.ebike_front_battery_power
    name: Power
  - entity: sensor.ebike_front_battery_soc
    name: State of Charge
  - entity: sensor.ebike_total_power
    name: Total Power
```

## Node-RED Integration

### Flow Example

1. Add MQTT In node:
   - Server: your-broker.hivemq.cloud:8883
   - Topic: `ebike/battery/+/status`
   - QoS: 0
   - Output: JSON object

2. Add Function node to process data:
```javascript
msg.payload = {
    battery_id: msg.payload.id,
    voltage: msg.payload.voltage,
    current: msg.payload.current,
    power: msg.payload.power,
    timestamp: new Date()
};
return msg;
```

3. Connect to InfluxDB, debug, or other outputs

## Performance

### Bandwidth Usage

At default settings (1 second publish interval):
- Per battery status: ~250 bytes/message
- All batteries combined: ~400 bytes/message
- System status: ~200 bytes/message (every 5s)

**Total bandwidth**: ~1 KB/second for 2 batteries

### Memory Usage

- WiFiClientSecure: ~40 KB heap
- PubSubClient buffer: 512 bytes (configurable)
- Total MQTT overhead: ~45 KB

### CPU Usage

MQTT operations run in the network task with minimal CPU impact:
- Publishing: <1ms per message
- Keep-alive: Handled by PubSubClient (60s interval)

## Troubleshooting

### MQTT Not Connecting

1. **Check WiFi**: Ensure WiFi is connected first
2. **Verify credentials**: Double-check username/password
3. **Test broker**: Use MQTT Explorer to verify broker is accessible
4. **Check serial output**: Look for specific error messages
5. **Certificate issues**: Ensure system time is correct (TLS validation)

### Messages Not Publishing

1. **Check connection**: Verify MQTT shows "Connected"
2. **Check publish interval**: Default is 1000ms
3. **Monitor failures**: Check `mqtt_failures` in system/status
4. **Buffer size**: Increase buffer if messages are large

### High Reconnect Count

Indicates unstable connection:
- Check WiFi signal strength (RSSI)
- Verify broker is stable
- Check for firewall blocking port 8883
- Consider increasing keep-alive interval

## Advanced Configuration

### Changing Buffer Size

In `mqtt_client.cpp`, modify:
```cpp
mqtt_client_.setBufferSize(1024);  // Increase to 1024 bytes
```

### Changing Keep-Alive

```cpp
mqtt_client_.setKeepAlive(120);  // Increase to 120 seconds
```

### Custom Topics

Modify topic structure in settings or code:
```cpp
// In mqtt_client.cpp publishBatteryStatus()
snprintf(topic, sizeof(topic), "%s/batteries/%d",
         config.mqtt_topic_prefix, battery_id);
```

### Adding Custom Messages

Add new publishing method in `mqtt_client.cpp`:
```cpp
bool MQTTClient::publishCustomData(const char* data) {
    char topic[64];
    snprintf(topic, sizeof(topic), "%s/custom/data",
             settings_->getSettings().mqtt_topic_prefix);
    return publish(topic, data, false);
}
```

## Security Considerations

1. **TLS is mandatory** for HiveMQ Cloud - credentials sent encrypted
2. **Store passwords securely** - kept in NVS flash (not plaintext in code)
3. **Use strong passwords** - for MQTT authentication
4. **Firewall rules** - Only allow outbound port 8883
5. **Certificate validation** - Root CA certificate embedded in firmware

## Future Enhancements

Potential additions:
- [ ] MQTT command support (subscriptions)
- [ ] Custom certificate upload via web interface
- [ ] MQTT over WebSocket support
- [ ] Compression for large payloads
- [ ] Batch publishing for efficiency
- [ ] Last Will and Testament (LWT) support
- [ ] QoS 1/2 support for critical messages

## References

- HiveMQ Cloud: https://www.hivemq.com/mqtt-cloud-broker/
- MQTT Specification: https://mqtt.org/
- PubSubClient Library: https://github.com/knolleary/pubsubclient
- MQTT Explorer: http://mqtt-explorer.com/
