/*
 * ENGR 290 - Fall 2026, Technical Assignment 1
 * Board: Arduino Nano 3.x (ATmega328P) - the course custom PCB
 *
 * Behavior (per TA1 spec):
 *   - Reads IR rangefinder (Sharp GP2Y0A21, analog) and US rangefinder (HC-SR04, digital)
 *   - D3 brightness: d <= d1 (16 cm) -> 100%, d >= d2 (49 cm) -> 0%, linear in between
 *   - "L" LED (PB5 / D13) flashes with T = 1.5 s while obstacle is OUTSIDE [d1; d2]
 *     (closer than d1, farther than d2, or no reading) and is solid ON inside it.
 *     Below d1, D3 is at 100% AND L flashes: the two rules are independent.
 *   - Prints ADC counts, millivolts, distances and the US echo time over UART (9600 8-N-1)
 *
 * Integer math only (TA1 question 6): no floats anywhere. Distances come from a
 * piecewise-linear lookup table / us / 58 division.
 *
 * Run ONE sensor at a time for the experiments: change SENSOR_SOURCE below.
 */

#include <Arduino.h>

// ---------------- Pins (mirrored in diagram.json) ----------------
const uint8_t PIN_IR   = A0; // GP2Y0A21 out: ADC channel of the 3-pin header you use (course code reads A0-A3, A6, A7)
const uint8_t PIN_TRIG = 8;  // HC-SR04 TRIG: check which Nano pin your 4-pin header routes TRIG to. NEVER D4-D7 (PD4-PD7: power control, init_290.c)
const uint8_t PIN_ECHO = 2;  // HC-SR04 ECHO (INT0, same channel as course sample code)
const uint8_t PIN_D3   = 11; // D3 LED = PB3 (Arduino pin 11), ACTIVE-LOW on the course PCB (init_290.c: "PB3-HI (D3 OFF)"); PWM via OC2A
const uint8_t PIN_L    = 13; // "L" LED (PB5), active-high on the course board

// ---------------- Configuration ----------------
enum sensor_t : uint8_t { SENSOR_IR, SENSOR_US };
#define SENSOR_SOURCE SENSOR_IR   // which sensor drives D3 / L for the experiments

// ADC reference = the voltage on the AREF pin (analogReference(EXTERNAL) in
// setup, same as the course's adc_init). On the course PCB, RV1 sets it (TA1
// experiment 1). The Wokwi simulator ignores AREF and always measures against
// 5 V, so simulator builds use 5000. flash.cmd builds with -DON_BOARD, which
// uses VREF_MV_BOARD instead: write your DMM reading of AREF there, in mV.
const uint16_t VREF_MV_BOARD = 5000;
#ifdef ON_BOARD
const uint16_t VREF_MV = VREF_MV_BOARD;
#else
const uint16_t VREF_MV = 5000;
#endif
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

uint16_t ir_mv(uint16_t adc) {
    return (uint16_t)((uint32_t)adc * VREF_MV / 1023);
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

// HC-SR04: 10 us TRIG pulse, then measure how long ECHO stays high (the "time"
// reading for TA1 table 1). Returns 0 when no echo arrives within 30 ms.
uint16_t us_echo_us() {
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(4);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);
    return (uint16_t)pulseIn(PIN_ECHO, HIGH, 30000UL);  // 30 ms ~ 5 m max
}

void setup() {
    // FIRST, before any analogRead(): RV1 drives AREF on the course PCB, and the
    // default (AVcc) reference would short AVcc to it inside the chip.
    analogReference(EXTERNAL);
    // D4-D7 (PD4-PD7) are power-control outputs on the course PCB. Hold them
    // LOW like the course's gpio_init() does, so nothing switches on by itself.
    for (uint8_t p = 4; p <= 7; p++) {
        digitalWrite(p, LOW);
        pinMode(p, OUTPUT);
    }
    pinMode(PIN_IR, INPUT);
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    pinMode(PIN_D3, OUTPUT);
    pinMode(PIN_L, OUTPUT);
    Serial.begin(9600);
    Serial.println(F("TA1 READY"));
#ifdef ON_BOARD
    Serial.print(F("board build"));
#else
    Serial.print(F("simulator build"));
#endif
    Serial.print(F(" | VREF_MV="));
    Serial.print(VREF_MV);
    Serial.print(F(" | IR=A"));
    Serial.print(PIN_IR - A0);
    Serial.print(F(" TRIG=D"));
    Serial.print(PIN_TRIG);
    Serial.print(F(" ECHO=D"));
    Serial.println(PIN_ECHO);
    Serial.println(F("t_ms;IR_adc;IR_mV;IR_cm;US_cm;src_cm;PWM;L;US_us"));
}

void loop() {
    static unsigned long tPrint = 0, tBlink = 0;
    static bool lState = false;

    const unsigned long now = millis();
    const uint16_t adc = analogRead(PIN_IR);
    const uint16_t mv = ir_mv(adc);
    const uint16_t cmIr = ir_cm(mv);
    const uint16_t echoUs = us_echo_us();
    const uint16_t cmUs = (echoUs == 0) ? IR_OUT_OF_RANGE : echoUs / 58;
    const uint16_t cm = (SENSOR_SOURCE == SENSOR_IR) ? cmIr : cmUs;

    // D3: 100% at d1, 0% at d2, linear in between (map is 32-bit integer math).
    uint8_t pwm;
    if (cm == IR_OUT_OF_RANGE || cm >= D2_CM) pwm = 0;
    else if (cm <= D1_CM)                    pwm = 255;
    else                                     pwm = (uint8_t)map(cm, D1_CM, D2_CM, 255, 0);
    analogWrite(PIN_D3, 255 - pwm);  // D3 is ACTIVE-LOW on the course PCB: duty 255 -> pin 0 -> LED full ON

    // L (option B): solid ON while inside [d1; d2]; flash T=1.5 s while outside.
    const bool outside = (cm == IR_OUT_OF_RANGE) || (cm < D1_CM) || (cm > D2_CM);
    if (!outside) {
        lState = true;  // inside range -> solid ON
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
        Serial.print(lState ? 1 : 0);
        Serial.print(';');
        Serial.println(echoUs);
    }
}
