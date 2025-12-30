#ifndef CURRENT_SENSOR_H
#define CURRENT_SENSOR_H

#include <Arduino.h>

// ACS712 Hall-effect current sensor interface
class CurrentSensor {
public:
    // ACS712 variants
    enum Variant {
        ACS712_05A,     // ±5A, 185mV/A
        ACS712_20A,     // ±20A, 100mV/A
        ACS712_30A      // ±30A, 66mV/A
    };

    CurrentSensor();

    // Initialization
    void begin(uint8_t adc_pin, Variant variant = ACS712_30A);

    // Configuration
    void setCalibration(float offset_mv, float scale_mv_per_amp);
    void calibrateZero();  // Auto-calibrate zero point

    // Reading
    float readCurrent();   // Returns current in Amps
    float readRaw();       // Returns raw voltage in mV

    // Getters
    float getOffset() const { return zero_offset_mv; }
    float getScale() const { return sensitivity_mv_per_amp; }

private:
    uint8_t pin;
    Variant variant;
    float sensitivity_mv_per_amp;
    float zero_offset_mv;

    // ADC reading with averaging
    float readADCVoltage();
};

#endif // CURRENT_SENSOR_H
