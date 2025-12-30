#ifndef MOVING_AVERAGE_H
#define MOVING_AVERAGE_H

#include <Arduino.h>

// Moving average filter with configurable sample window
template<size_t WINDOW_SIZE>
class MovingAverage {
public:
    MovingAverage() : index(0), count(0), sum(0.0f) {
        for (size_t i = 0; i < WINDOW_SIZE; i++) {
            samples[i] = 0.0f;
        }
    }

    // Add new sample and return current average
    float addSample(float value) {
        // Subtract old value from sum
        sum -= samples[index];

        // Add new value
        samples[index] = value;
        sum += value;

        // Move to next position
        index = (index + 1) % WINDOW_SIZE;

        // Track how many samples we have
        if (count < WINDOW_SIZE) {
            count++;
        }

        return getAverage();
    }

    // Get current average
    float getAverage() const {
        if (count == 0) {
            return 0.0f;
        }
        return sum / count;
    }

    // Reset filter
    void reset() {
        index = 0;
        count = 0;
        sum = 0.0f;
        for (size_t i = 0; i < WINDOW_SIZE; i++) {
            samples[i] = 0.0f;
        }
    }

    // Status
    bool isFull() const { return count == WINDOW_SIZE; }
    size_t getCount() const { return count; }

private:
    float samples[WINDOW_SIZE];
    size_t index;
    size_t count;
    float sum;
};

#endif // MOVING_AVERAGE_H
