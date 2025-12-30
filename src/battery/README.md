# Battery Module

This module manages individual battery modules and orchestrates multi-battery systems (1-5 batteries). It aggregates data from both CAN bus messages and direct sensor readings.

## Files

- `battery_module.h/cpp` - Individual battery data structure and state management
- `battery_manager.h/cpp` - Multi-battery orchestration and aggregate calculations

## Architecture

```
BatteryManager
├── BatteryModule[0] (Front Battery)
│   ├── Voltage (from sensor or CAN)
│   ├── Current (from ACS712 sensor)
│   ├── SOC (from CAN)
│   ├── Temperatures (from CAN)
│   └── Status flags (from CAN)
├── BatteryModule[1] (Rear Battery)
└── ... (up to 5 batteries)
```

## BatteryModule API

Each `BatteryModule` represents a single battery pack with its telemetry data.

### Initialization

```cpp
BatteryModule battery;
battery.begin(0, "Front Battery");
```

### Updating Data

**From Sensors:**
```cpp
// Update voltage from ADC reading
battery.updateVoltage(52.4f);  // 52.4V

// Update current from ACS712 sensor
battery.updateCurrent(3.5f);   // 3.5A
```

**From CAN Bus:**
```cpp
CANBatteryData canData;
// ... parse CAN message into canData

battery.updateFromCAN(canData);
// This updates: voltage, current, SOC, temps, status flags
```

### Reading Data

```cpp
// Basic telemetry
float voltage = battery.getVoltage();      // Volts
float current = battery.getCurrent();      // Amps (signed, negative = charging)
float power = battery.getPower();          // Watts (V × A)

// State of charge
uint8_t soc = battery.getSOC();            // 0-100%

// Temperatures
float temp1 = battery.getTemp1();          // °C
float temp2 = battery.getTemp2();          // °C

// Status
uint8_t flags = battery.getStatusFlags();  // See CANStatusFlags
bool charging = flags & CANStatusFlags::CHARGING;
bool error = flags & CANStatusFlags::ERROR;
```

### Status Checks

```cpp
// Check if battery is enabled
if (battery.isEnabled()) {
    // Process battery data
}

// Check if data is fresh (not stale)
if (battery.isDataFresh(5000)) {  // 5 second timeout
    // Data is recent
}

// Check if we have CAN data
if (battery.hasCANData()) {
    // Battery is communicating via CAN
}

// Check for errors
if (battery.hasError()) {
    // Error flag set
}
```

### Configuration

```cpp
// Enable/disable battery
battery.setEnabled(true);

// Set custom name
battery.setName("Front Battery");

// Get battery info
uint8_t id = battery.getId();
const char* name = battery.getName();
uint32_t lastUpdate = battery.getLastUpdate();  // millis()
```

## BatteryManager API

The `BatteryManager` orchestrates multiple battery modules.

### Initialization

```cpp
BatteryManager batteryManager;

// Initialize with 2 batteries
batteryManager.begin(2);

// Batteries are auto-named "Battery 1", "Battery 2", etc.
```

### Accessing Batteries

```cpp
// Get specific battery
BatteryModule* battery = batteryManager.getBattery(0);
if (battery != nullptr) {
    float voltage = battery->getVoltage();
}

// Get active battery count
uint8_t count = batteryManager.getActiveBatteryCount();  // Returns 2
```

### Aggregate Calculations

```cpp
// Total power across all enabled batteries
float totalPower = batteryManager.getTotalPower();

// Total current (sum of all battery currents)
float totalCurrent = batteryManager.getTotalCurrent();

// Average voltage
float avgVoltage = batteryManager.getAverageVoltage();
```

### Configuration

```cpp
// Enable/disable specific battery
batteryManager.enableBattery(1, false);  // Disable battery 1

// Set custom name
batteryManager.setBatteryName(0, "Front Battery");

// Calibrate current sensor (placeholder for sensor integration)
batteryManager.calibrateCurrent(0);
```

### Health Monitoring

```cpp
// Check if all batteries are healthy
if (batteryManager.allBatteriesHealthy()) {
    Serial.println("All batteries OK");
} else {
    // Get error count
    uint8_t errors = batteryManager.getErrorCount();
    Serial.printf("%d battery error(s) detected\n", errors);
}
```

A battery is considered unhealthy if:
- Error flag is set
- Data is stale (>10 seconds old)
- Over/under voltage
- Over current
- Temperature warning

## Data Flow

### Sensor-based Updates

```
ADC Reading → Sensor Driver → BatteryModule.updateVoltage()
                             → BatteryModule.updateCurrent()
```

### CAN-based Updates

```
CAN Message → Parser → CANBatteryData → BatteryModule.updateFromCAN()
```

### Hybrid System

Most eBike monitoring systems use a hybrid approach:
- **Current**: Measured locally via ACS712 sensors
- **Voltage**: Can be measured locally or via CAN
- **SOC, Temperatures, Status**: Received via CAN from BMS

## Example: Complete Battery Monitoring

```cpp
#include "battery/battery_manager.h"
#include "can/can_driver.h"
#include "can/can_parser.h"

BatteryManager batteryManager;
CANParser canParser;

void setup() {
    // Initialize with 2 batteries
    batteryManager.begin(2);

    // Configure batteries
    batteryManager.setBatteryName(0, "Front");
    batteryManager.setBatteryName(1, "Rear");

    // Set up CAN callback
    canDriver.setMessageCallback([](const CANMessage& msg) {
        CANBatteryData canData;
        if (canParser.parseMessage(msg, canData)) {
            // Update corresponding battery
            if (canData.battery_id < 2) {
                BatteryModule* batt = batteryManager.getBattery(canData.battery_id);
                if (batt) {
                    batt->updateFromCAN(canData);
                }
            }
        }
    });
}

void loop() {
    // Read current sensors and update batteries
    float current0 = readCurrentSensor(0);
    float current1 = readCurrentSensor(1);

    batteryManager.getBattery(0)->updateCurrent(current0);
    batteryManager.getBattery(1)->updateCurrent(current1);

    // Print status
    Serial.printf("Total Power: %.2f W\n", batteryManager.getTotalPower());

    // Check health
    if (!batteryManager.allBatteriesHealthy()) {
        Serial.println("WARNING: Battery issues detected!");
    }

    delay(1000);
}
```

## Integration with Settings

Battery configuration is stored in NVS via SettingsManager:

```cpp
SettingsManager settingsManager;
BatteryManager batteryManager;

void setup() {
    settingsManager.begin();

    const Settings& settings = settingsManager.getSettings();
    batteryManager.begin(settings.num_batteries);

    // Apply settings to each battery
    for (uint8_t i = 0; i < settings.num_batteries; i++) {
        BatteryModule* battery = batteryManager.getBattery(i);
        battery->setEnabled(settings.batteries[i].enabled);
        battery->setName(settings.batteries[i].name);

        // Calibration values would be used by sensor drivers
        // float offset = settings.batteries[i].current_cal_offset;
        // float scale = settings.batteries[i].current_cal_scale;
    }
}
```

## Status Flags

From `can_message.h`:

```cpp
namespace CANStatusFlags {
    constexpr uint8_t CHARGING       = 0x01;  // Battery is charging
    constexpr uint8_t DISCHARGING    = 0x02;  // Battery is discharging
    constexpr uint8_t BALANCING      = 0x04;  // Cell balancing active
    constexpr uint8_t TEMP_WARNING   = 0x08;  // Temperature outside normal
    constexpr uint8_t OVER_VOLTAGE   = 0x10;  // Pack/cell overvoltage
    constexpr uint8_t UNDER_VOLTAGE  = 0x20;  // Pack/cell undervoltage
    constexpr uint8_t OVER_CURRENT   = 0x40;  // Current limit exceeded
    constexpr uint8_t ERROR          = 0x80;  // General fault condition
}
```

### Checking Status Flags

```cpp
uint8_t flags = battery.getStatusFlags();

if (flags & CANStatusFlags::CHARGING) {
    Serial.println("Battery is charging");
}

if (flags & CANStatusFlags::ERROR) {
    Serial.println("Battery error detected!");
}

if (flags & (CANStatusFlags::OVER_VOLTAGE | CANStatusFlags::UNDER_VOLTAGE)) {
    Serial.println("Voltage out of range!");
}
```

## Data Freshness

Data is considered "fresh" if it was updated recently:

```cpp
// Check if data is less than 5 seconds old
if (battery.isDataFresh(5000)) {
    // Safe to use data
    float voltage = battery.getVoltage();
} else {
    Serial.println("Warning: Stale battery data");
}
```

The default timeout in health checks is 10 seconds.

## Memory Usage

- **BatteryModule**: ~80 bytes per instance
- **BatteryManager**: ~400 bytes (5 batteries × 80 bytes)
- **Total**: <500 bytes for full 5-battery system

## Thread Safety

⚠️ **Important**:
- The battery module is NOT thread-safe by default
- Updates from CAN task and sensor task should be coordinated
- In practice, this is safe because:
  - CAN updates come from CAN task
  - Sensor updates come from sensor task
  - Different batteries are typically updated from different tasks
  - Reads are atomic for simple types

For critical applications, add mutex protection:

```cpp
SemaphoreHandle_t batteryMutex;

// Before updating
xSemaphoreTake(batteryMutex, portMAX_DELAY);
battery->updateVoltage(voltage);
xSemaphoreGive(batteryMutex);
```

## Future Enhancements

- [ ] Historical data tracking (moving averages)
- [ ] Energy consumption calculation (Wh)
- [ ] Cell-level voltage monitoring
- [ ] Predictive SOC estimation
- [ ] Battery chemistry profiles (Li-ion, LiFePO4, etc.)
- [ ] Alerts and notifications
- [ ] Data logging to SD card

## See Also

- `config/settings.h` - Battery configuration storage
- `can/can_message.h` - CAN battery data structures
- `sensors/current_sensor.h` - ACS712 current sensing
- `sensors/voltage_sensor.h` - Voltage measurement
