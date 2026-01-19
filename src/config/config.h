#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Hardware Configuration
#define MAX_BATTERY_MODULES 5

// Pin Definitions - ESP32
#define PIN_CAN_TX          5
#define PIN_CAN_RX          4

// ACS712 Current Sensor Pins (ADC1 - safe to use with WiFi)
#define PIN_ACS712_BATT1    34  // ADC1_CH6
#define PIN_ACS712_BATT2    35  // ADC1_CH7
#define PIN_ACS712_BATT3    32  // ADC1_CH4
#define PIN_ACS712_BATT4    33  // ADC1_CH5
#define PIN_ACS712_BATT5    36  // ADC1_CH0 (VP)

// Voltage Sense Pins
#define PIN_VOLTAGE_BATT1   39  // ADC1_CH3 (VN)
#define PIN_VOLTAGE_COMMON  25  // ADC2_CH8 (or use multiplexer)

// Status LED
#define PIN_STATUS_LED      2

// CAN Configuration
#define CAN_BITRATE         500000  // 500 kbps
#define CAN_RX_QUEUE_SIZE   100
#define CAN_TX_QUEUE_SIZE   20

// Timing Configuration (milliseconds)
#define DEFAULT_SAMPLE_INTERVAL_MS      100
#define DEFAULT_PUBLISH_INTERVAL_MS     1000
#define DEFAULT_WEB_REFRESH_MS          500
#define CAN_LOG_FLUSH_INTERVAL_MS       5000

// WiFi Configuration
#define WIFI_AP_SSID_PREFIX     "eBikeMonitor-"
#define WIFI_AP_PASSWORD        "ebike123"  // Must be 8+ chars for WPA2
#define WIFI_AP_IP              "192.168.4.1"
#define WIFI_CONNECTION_TIMEOUT 20000

// MQTT Configuration
// HiveMQ Cloud broker with TLS
#define MQTT_DEFAULT_BROKER     "508641aa1f684bb2b2f1170750e8c5ac.s1.eu.hivemq.cloud"
#define MQTT_DEFAULT_PORT       8883  // TLS MQTT port
#define MQTT_WEBSOCKET_PORT     8884  // TLS WebSocket port
#define MQTT_RECONNECT_DELAY    5000
#define MQTT_TOPIC_PREFIX       "ebike"
// Note: TLS support requires WiFiClientSecure and proper certificate handling

// Web Server
#define WEB_SERVER_PORT         80

// Memory Management
#define HEAP_WARNING_THRESHOLD  20000   // Warn if free heap below 20KB
#define CAN_LOG_MAX_ENTRIES     1000    // Ring buffer size for CAN log
#define SPIFFS_ROTATION_PERCENT 80      // Rotate log at 80% full

// ADC Configuration
#define ADC_SAMPLES_FOR_AVERAGE 10
#define ADC_VREF                3.3f
#define ADC_RESOLUTION          4095    // 12-bit ADC

// ACS712 Calibration Defaults
#define ACS712_ZERO_CURRENT_MV  2500.0f // Center voltage at 0A
#define ACS712_05A_SENSITIVITY  185.0f  // mV/A for 5A variant
#define ACS712_20A_SENSITIVITY  100.0f  // mV/A for 20A variant
#define ACS712_30A_SENSITIVITY  66.0f   // mV/A for 30A variant

// Voltage Divider Defaults (adjust based on hardware)
#define VOLTAGE_DIVIDER_RATIO   20.0f   // (R1+R2)/R2

// NVS Configuration
#define NVS_NAMESPACE           "ebike_config"

#endif // CONFIG_H
