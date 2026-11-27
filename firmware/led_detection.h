#pragma once
#include <Arduino.h>
#include <cstring>

// Integrar en el ejemplo de inferencia continua de Edge Impulse.
namespace keyword_led {
constexpr float threshold = 0.80f;
constexpr unsigned long duration_ms = 500;
static unsigned long started = 0;
static bool active = false;
inline void begin() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
}
inline void detection(const char* label, float probability) {
    if (std::strcmp(label, "carlos") == 0 && probability >= threshold) {
        started = millis();
        active = true;
        digitalWrite(LED_BUILTIN, HIGH);
    }
}
inline void update() {
    if (active && millis() - started >= duration_ms) {
        digitalWrite(LED_BUILTIN, LOW);
        active = false;
    }
}
}
