/*
 * ENGR 290 - Fall 2026, Technical Assignment 1
 * Board: Arduino Nano 3.x (ATmega328P) - the course custom PCB
 *
 * Behavior (per TA1 spec):
 *   - Reads IR rangefinder (Sharp GP2Y0A21, analog) and US rangefinder (HC-SR04, digital)
 *   - D3 brightness: d <= d1 (16 cm) -> 100%, d >= d2 (49 cm) -> 0%, linear in between
 *   - "L" LED (PB5 / D13) flashes with T = 1.5 s while obstacle is OUTSIDE [d1; d2]
 *   - Prints ADC counts, millivolts, and distance over UART (9600 8-N-1)
 *
 * Integer math only (TA1 question 6): no floats anywhere. Distances come from a
 * piecewise-linear lookup table / us / 58 division.
 *
 * Run ONE sensor at a time for the experiments: change SENSOR_SOURCE below.
 */

#include <Arduino.h>

// ---------------- Pins (mirrored in diagram.json) ----------------
const uint8_t PIN_IR   = A0; // GP2Y0A21 analog out -> any ADC header on the course PCB
const uint8_t PIN_TRIG = 8;  // HC-SR04 TRIG (D8; NEVER D4-D7 = PD4-PD7: power-control outputs on the course PCB, init_290.c)
const uint8_t PIN_ECHO = 2;  // HC-SR04 ECHO (INT0, same channel as course sample code)
const uint8_t PIN_D3   = 11; // D3 LED = PB3 (Arduino pin 11), ACTIVE-LOW on the course PCB (init_290.c: "PB3-HI (D3 OFF)"); PWM via OC2A
const uint8_t PIN_L    = 13; // "L" LED (PB5), active-high on the course board

// ---------------- Configuration ----------------
enum sensor_t : uint8_t { SENSOR_IR, SENSOR_US };
#define SENSOR_SOURCE SENSOR_IR   // which sensor drives D3 / L for the experiments

const uint16_t VREF_MV = 5000;     // TA1 Q1: set to YOUR chosen Vref (default = AVcc 5V)
const uint16_t D1_CM   = 16;       // at or below -> brightness 100%
const uint16_t D2_CM   = 49;       // at or above -> brightness 0%
const uint16_t IR_OUT_OF_RANGE = 999;
const unsigned long BLINK_HALF_MS = 750;   // T = 1.5 s -> 750 ms half period
const unsigned long PRINT_MS      = 500;

// GP2Y0A21YK typical output curve, {millivolts, cm}, monotonic (V falls as d grows).
// TYPICAL values only - re-measure with the DMM in H-933/H-941 and replace with your
// calibration points for the report (TA1 Q1/Q4/Q5).
const uint16_t IR_TABLE[][2] PROGMEM = {
    {2550, 10}, {2000, 12}, {1550, 15}, {1250, 18}, {1120, 20},
    {900, 25},  {730, 30},  {620, 35},  {550, 40},  {450, 50},
    {380, 60},  {330, 70},  {300, 80},
};
const uint8_t IR_TABLE_N = sizeof(IR_TABLE) / sizeof(IR_TABLE[0]);

uint16_t ir_mv() {
    return (uint16_t)((uint32_t)analogRead(PIN_IR) * VREF_MV / 1023);
}

// Piecewise-linear inverse of IR_TABLE, integer arithmetic only.
// A voltage above the 10 cm table entry means the obstacle is CLOSER than 10 cm:
// clamp to 10 cm so "d1 or less -> 100%" holds (TA1). Only readings below the
// far end (> 80 cm) are out of range. In the non-monotonic <7 cm zone the table
// returns a larger distance - that observed anomaly is what TA1 Q3 asks about.
uint16_t ir_cm(uint16_t mv) {
    if (mv < pgm_read_word(&IR_TABLE[IR_TABLE_N - 1][0])) {
        return IR_OUT_OF_RANGE;  // farther than 80 cm
    }
    if (mv > pgm_read_word(&IR_TABLE[0][0])) {
        return pgm_read_word(&IR_TABLE[0][1]);  // closer than 10 cm -> 10 cm
    }
    for (uint8_t i = 0; i < IR_TABLE_N - 1; i++) {
        const uint16_t vHi = pgm_read_word(&IR_TABLE[i][0]);
        const uint16_t vLo = pgm_read_word(&IR_TABLE[i + 1][0]);
        if (mv <= vHi && mv >= vLo) {
            const uint16_t dHi = pgm_read_word(&IR_TABLE[i][1]);
            const uint16_t dLo = pgm_read_word(&IR_TABLE[i + 1][1]);
            return dHi + (uint32_t)(vHi - mv) * (dLo - dHi) / (vHi - vLo);
        }
    }
    return IR_OUT_OF_RANGE;
}

// HC-SR04: 10 us TRIG pulse, measure ECHO high time. cm = us / 58.
uint16_t us_cm() {
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(4);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);
    const unsigned long us = pulseIn(PIN_ECHO, HIGH, 30000UL);  // 30 ms ~ 5 m max
    return (us == 0) ? IR_OUT_OF_RANGE : (uint16_t)(us / 58);
}

void setup() {
    pinMode(PIN_IR, INPUT);
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    pinMode(PIN_D3, OUTPUT);
    pinMode(PIN_L, OUTPUT);
    // TA1 Q1: if you chose an external reference, set it here, e.g.
    // analogReference(EXTERNAL); and update VREF_MV accordingly.
    Serial.begin(9600);
    Serial.println(F("TA1 READY"));
    Serial.println(F("ENGR290 TA1 | IR=GP2Y0A21@A0 US=HC-SR04 TRIG=D8 ECHO=D2 | 9600 8N1"));
    Serial.println(F("t_ms;IR_adc;IR_mV;IR_cm;US_cm;src_cm;PWM;L"));
}

void loop() {
    static unsigned long tPrint = 0, tBlink = 0;
    static bool lState = false;

    const unsigned long now = millis();
    const uint16_t adc = analogRead(PIN_IR);
    const uint16_t mv = ir_mv();
    const uint16_t cmIr = ir_cm(mv);
    const uint16_t cmUs = us_cm();
    const uint16_t cm = (SENSOR_SOURCE == SENSOR_IR) ? cmIr : cmUs;

    // D3: 100% at d1, 0% at d2, linear in between (map is 32-bit integer math).
    uint8_t pwm;
    if (cm == IR_OUT_OF_RANGE || cm >= D2_CM) pwm = 0;
    else if (cm <= D1_CM)                    pwm = 255;
    else                                     pwm = (uint8_t)map(cm, D1_CM, D2_CM, 255, 0);
    analogWrite(PIN_D3, 255 - pwm);  // D3 is ACTIVE-LOW on the course PCB: duty 255 -> pin 0 -> LED full ON

    // L: flash T=1.5 s while outside [d1; d2]; solid off while inside.
    const bool outside = (cm == IR_OUT_OF_RANGE) || (cm < D1_CM) || (cm > D2_CM);
    if (!outside) {
        lState = false;
    } else if (now - tBlink >= BLINK_HALF_MS) {
        tBlink = now;
        lState = !lState;
    }
    digitalWrite(PIN_L, lState ? HIGH : LOW);

    if (now - tPrint >= PRINT_MS) {
        tPrint = now;
        Serial.print(now);
        Serial.print(';');
        Serial.print(adc);
        Serial.print(';');
        Serial.print(mv);
        Serial.print(';');
        Serial.print(cmIr);
        Serial.print(';');
        Serial.print(cmUs);
        Serial.print(';');
        Serial.print(cm);
        Serial.print(';');
        Serial.print(pwm);
        Serial.print(';');
        Serial.println(lState ? 1 : 0);
    }
}
