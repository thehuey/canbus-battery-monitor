#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <Arduino.h>

// Template ring buffer for fixed-size circular storage
template<typename T, size_t SIZE>
class RingBuffer {
public:
    RingBuffer() : head(0), tail(0), count(0) {}

    // Add item to buffer (overwrites oldest if full)
    bool push(const T& item) {
        buffer[head] = item;
        head = (head + 1) % SIZE;

        if (count < SIZE) {
            count++;
        } else {
            // Buffer full, move tail
            tail = (tail + 1) % SIZE;
        }
        return true;
    }

    // Get oldest item (FIFO)
    bool pop(T& item) {
        if (isEmpty()) {
            return false;
        }

        item = buffer[tail];
        tail = (tail + 1) % SIZE;
        count--;
        return true;
    }

    // Peek at item without removing
    bool peek(T& item, size_t index = 0) const {
        if (index >= count) {
            return false;
        }

        size_t pos = (tail + index) % SIZE;
        item = buffer[pos];
        return true;
    }

    // Get most recent item
    bool peekLast(T& item) const {
        if (isEmpty()) {
            return false;
        }

        size_t pos = (head + SIZE - 1) % SIZE;
        item = buffer[pos];
        return true;
    }

    // Status
    bool isEmpty() const { return count == 0; }
    bool isFull() const { return count == SIZE; }
    size_t size() const { return count; }
    size_t capacity() const { return SIZE; }

    // Clear buffer
    void clear() {
        head = 0;
        tail = 0;
        count = 0;
    }

    // Iterate (callback for each item, oldest to newest)
    template<typename Func>
    void forEach(Func callback) const {
        for (size_t i = 0; i < count; i++) {
            size_t pos = (tail + i) % SIZE;
            callback(buffer[pos]);
        }
    }

private:
    T buffer[SIZE];
    size_t head;
    size_t tail;
    size_t count;
};

#endif // RING_BUFFER_H
