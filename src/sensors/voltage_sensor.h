#ifndef VOLTAGE_SENSOR_H
#define VOLTAGE_SENSOR_H

#include <Arduino.h>

// Voltage divider sensor interface
class VoltageSensor {
public:
    VoltageSensor();

    // Initialization
    void begin(uint8_t adc_pin, float divider_ratio = 20.0f);

    // Configuration
    void setDividerRatio(float ratio) { divider_ratio = ratio; }
    void calibrate(float known_voltage); // Calibrate against known voltage

    // Reading
    float readVoltage();   // Returns voltage in Volts
    float readRaw();       // Returns raw ADC voltage

    // Getters
    float getDividerRatio() const { return divider_ratio; }

private:
    uint8_t pin;
    float divider_ratio;

    // ADC reading with averaging
    float readADCVoltage();
};

#endif // VOLTAGE_SENSOR_H
