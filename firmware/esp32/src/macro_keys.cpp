#include "macro_keys.h"
#include <Arduino.h>
#include "../include/pins.h"

namespace {
    // TTP229 configuration - adjust based on hardware jumpers
    // Most TTP229-BSF modules default to active-low (touch = low)
    constexpr bool ACTIVE_LOW = true;
    
    // Previous key state to detect transitions
    uint16_t previousState = 0x0000;
    
    // Internal function to read 16 bits from TTP229
    uint16_t readTtp229() {
        uint16_t keyState = 0;
        
        // Generate 16 clock cycles to read all keys
        for (int i = 0; i < 16; i++) {
            // Pull SCL low to begin bit read
            digitalWrite(Pins::Ttp229Scl, LOW);
            delayMicroseconds(10); // Wait minimum 10us as per datasheet
            
            // Read SDO bit (LSB first, which is key 1 first)
            if (digitalRead(Pins::Ttp229Sdo)) {
                keyState |= (1 << i);
            }
            
            // Pull SCL high to end bit read
            digitalWrite(Pins::Ttp229Scl, HIGH);
            delayMicroseconds(10); // Wait minimum 10us
        }
        
        return keyState;
    }
    
    // Process state transitions and print to serial
    void processStateTransitions(uint16_t currentState) {
        if (currentState == previousState) {
            return; // No changes
        }
        
        // Find pressed keys (transition from 0 to 1 in active-high, or 1 to 0 in active-low)
        uint16_t pressed = 0;
        uint16_t released = 0;
        
        if (ACTIVE_LOW) {
            // In active-low, 0 = pressed, 1 = released
            pressed = previousState & (~currentState); // Was 1 (released), now 0 (pressed)
            released = currentState & (~previousState); // Was 0 (pressed), now 1 (released)
        } else {
            // In active-high, 1 = pressed, 0 = released
            pressed = currentState & (~previousState); // Was 0 (released), now 1 (pressed)
            released = previousState & (~currentState); // Was 1 (pressed), now 0 (released)
        }
        
        // Print pressed keys
        for (int i = 0; i < 16; i++) {
            if (pressed & (1 << i)) {
                Serial.printf("[MACRO] KEY %d PRESSED\n", i + 1);
            }
            if (released & (1 << i)) {
                Serial.printf("[MACRO] KEY %d RELEASED\n", i + 1);
            }
        }
        
        // Update previous state
        previousState = currentState;
    }
}

namespace MacroKeys {
    void begin() {
        // Initialize pins
        pinMode(Pins::Ttp229Scl, OUTPUT);
        pinMode(Pins::Ttp229Sdo, INPUT); // GPIO35 is input-only, no pullup/down
        
        // Set SCL idle high (as per TTP229 requirements)
        digitalWrite(Pins::Ttp229Scl, HIGH);
        
        // Initialize previous state to idle state
        if (ACTIVE_LOW) {
            previousState = 0xFFFF; // All keys released (high) in active-low
        } else {
            previousState = 0x0000; // All keys released (low) in active-high
        }
        
        // Print initialization banner
        Serial.println("\n[MACRO] TTP229-BSF initialized");
        Serial.printf("[MACRO] SCL=GPIO%d SDO=GPIO%d\n", Pins::Ttp229Scl, Pins::Ttp229Sdo);
        Serial.println("[MACRO] Waiting for key input...\n");
    }
    
    void update() {
        uint16_t currentState = readTtp229();
        processStateTransitions(currentState);
    }
}