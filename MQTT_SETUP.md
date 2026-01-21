# MQTT Setup Guide - Quick Start

## Prerequisites

1. **ESP32 with eBike Monitor firmware** (with MQTT support)
2. **HiveMQ Cloud account** (free tier available)
3. **WiFi connection** configured on the device

## Step-by-Step Setup

### 1. Create HiveMQ Cloud Cluster

1. Go to https://console.hivemq.cloud/
2. Sign up (free tier includes 100 connections)
3. Click "Create Cluster"
4. Wait for cluster provisioning (~2 minutes)
5. Note your cluster URL (e.g., `xxxxxxxx.s1.eu.hivemq.cloud`)

### 2. Create MQTT Credentials

1. In HiveMQ Cloud console, open your cluster
2. Click "Access Management"
3. Click "Add Credentials"
4. Enter:
   - **Username**: `ebike_monitor` (or any name)
   - **Password**: Create a strong password
5. Click "Add" and save these credentials

### 3. Configure Device

#### Option A: Via Web Interface

1. Connect to device web interface (http://192.168.4.1 or device IP)
2. Go to "Configuration" page
3. Scroll to MQTT section
4. Enter:
   - **MQTT Broker**: `[your-cluster].s1.eu.hivemq.cloud`
   - **MQTT Port**: `8883`
   - **MQTT Username**: `ebike_monitor`
   - **MQTT Password**: [your password]
   - **Topic Prefix**: `ebike` (default)
5. Click "Save Configuration"

#### Option B: Via API

```bash
curl -X POST http://[device-ip]/api/config \
  -H "Content-Type: application/json" \
  -d '{
    "mqtt_broker": "[your-cluster].s1.eu.hivemq.cloud",
    "mqtt_port": 8883,
    "mqtt_username": "ebike_monitor",
    "mqtt_password": "your-password",
    "mqtt_topic_prefix": "ebike"
  }'
```

### 4. Verify Connection

#### Check Serial Monitor

Connect to serial port (115200 baud) and look for:

```
[MQTT] Initializing MQTT client...
[MQTT] Broker: [your-cluster].s1.eu.hivemq.cloud:8883
[MQTT] TLS configured with root CA certificate
[MQTT] Attempting to connect...
[MQTT] Connected successfully!
[MQTT] Published to ebike/system/config
```

✅ If you see "Connected successfully!" - you're done!

❌ If you see errors:
- `Bad credentials` - Check username/password
- `Connection timeout` - Check WiFi and broker URL
- `Unauthorized` - Verify credentials in HiveMQ Cloud

### 5. Test with MQTT Explorer

1. Download MQTT Explorer: http://mqtt-explorer.com/
2. Create new connection:
   - **Host**: `[your-cluster].s1.eu.hivemq.cloud`
   - **Port**: `8883`
   - **Username**: `ebike_monitor`
   - **Password**: [your password]
   - **Encryption**: TLS
3. Click "Connect"
4. You should see topics appearing under `ebike/`

### 6. View Messages

In MQTT Explorer, expand the topics:

```
ebike/
├── battery/
│   ├── 0/status ← Click to see battery data
│   └── all/status ← Click to see combined data
└── system/
    ├── status ← System health
    └── config ← Device configuration
```

## What Gets Published?

### Every 1 Second:
- Individual battery status (voltage, current, power, SOC, temperature)
- Combined battery status (total power, average voltage)

### Every 5 Seconds:
- System status (uptime, memory, WiFi signal)

### On Connection:
- Device configuration (number of batteries, settings)

## Common Issues

### "WiFi not connected"
1. Fix WiFi connection first
2. Check WiFi credentials in device settings
3. Verify device can access internet

### "No broker configured"
1. Enter MQTT broker URL in settings
2. Make sure you save the configuration
3. Restart device if needed

### "Bad credentials"
1. Verify username/password in HiveMQ Cloud
2. Re-enter credentials in device settings
3. Ensure no extra spaces in credentials

### Messages Not Appearing
1. Verify device shows "Connected" in serial monitor
2. Check publish interval (default 1000ms)
3. Ensure battery data is being received from CAN bus
4. Check MQTT Explorer subscription to `ebike/#`

## Next Steps

### Home Assistant Integration
See `MQTT_IMPLEMENTATION.md` for Home Assistant configuration examples.

### Node-RED Integration
See `MQTT_IMPLEMENTATION.md` for Node-RED flow examples.

### Custom Processing
Subscribe to `ebike/#` and process data in your application.

## Data Format Reference

### Battery Status Message
```json
{
  "voltage": 52.4,      // Volts
  "current": 3.2,       // Amperes
  "power": 167.68,      // Watts
  "soc": 85,            // Percent (0-100)
  "temp1": 28.5,        // Celsius
  "temp2": 27.8,        // Celsius
  "timestamp": 12345    // Seconds since boot
}
```

## Documentation

- **Full Guide**: See `MQTT_IMPLEMENTATION.md`
- **Quick Reference**: See `MQTT_QUICK_REFERENCE.md`
- **Main Documentation**: See `CLAUDE.md`

## Support

For issues:
1. Check serial monitor for error messages
2. Verify HiveMQ Cloud credentials are correct
3. Test broker connection with MQTT Explorer
4. Check WiFi signal strength and internet connectivity

## Alternative Brokers

While pre-configured for HiveMQ Cloud, you can use any MQTT broker that supports TLS:

- **Mosquitto** (self-hosted)
- **CloudMQTT**
- **EMQ X Cloud**
- **AWS IoT Core** (requires code changes)

See `MQTT_IMPLEMENTATION.md` for details on using alternative brokers.
