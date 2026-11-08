// Input for the CYD port: XPT2046 resistive touch (bit-banged), serial
// keyboard fallback, and the BOOT button.
//
// Touch zones (320x240 landscape):
//   +--------------------------------+
//   | MENU          |        | MAP   |   top strip (y < 48)
//   |---------------+--------+-------|
//   |               |                |
//   |   virtual     |   USE (A)      |   y < 144
//   |   d-pad       |----------------|
//   |   (x < 176)   |   FIRE (B)     |
//   +--------------------------------+
// D-pad: dominant axis from pad centre decides up/down/left/right.
// Serial keys: w/a/s/d move+turn, q/e strafe, f=fire, space=use,
//              m/enter=menu, t/tab=map. Keys auto-release after a timeout.
#include <Arduino.h>
#include "doomport.h"

extern "C" {
#include "doomdef.h"
#include "doomtype.h"
#include "d_event.h"
void D_PostEvent(event_t* ev);
}

#define PIN_TOUCH_CLK  25
#define PIN_TOUCH_CS   33
#define PIN_TOUCH_DIN  32
#define PIN_TOUCH_DOUT 39
#define PIN_TOUCH_IRQ  36
#define PIN_BOOT_BTN    0

// Raw ADC ranges for the 2.8" CYD panel (portrait-native), tuned generously.
#define RAW_MIN 300
#define RAW_MAX 3800

#define SERIAL_KEY_HOLD_MS 180

static void postKey(evtype_t type, int key)
{
    event_t ev;
    ev.type = type;
    ev.data1 = key;
    D_PostEvent(&ev);
}

// ---------------------------------------------------------------- touch ---

static int touchKeyHeld = -1;

static uint16_t xptTransfer16(uint8_t cmdByte)
{
    uint16_t result = 0;
    for (int i = 7; i >= 0; i--) {
        digitalWrite(PIN_TOUCH_DIN, (cmdByte >> i) & 1);
        digitalWrite(PIN_TOUCH_CLK, HIGH);
        digitalWrite(PIN_TOUCH_CLK, LOW);
    }
    // DIN must be LOW while clocking the reply out: a high level here is
    // taken as the start bit of a new command, aborting the conversion
    // (it then reads the temperature channel - constant ~204).
    digitalWrite(PIN_TOUCH_DIN, LOW);
    for (int i = 15; i >= 0; i--) {
        digitalWrite(PIN_TOUCH_CLK, HIGH);
        digitalWrite(PIN_TOUCH_CLK, LOW);
        if (digitalRead(PIN_TOUCH_DOUT))
            result |= (1 << i);
    }
    return result >> 3; // [busy][12 bits data][3 pad]
}

static int median3(int a, int b, int c)
{
    if (a > b) { int t = a; a = b; b = t; }
    if (b > c) { int t = b; b = c; c = t; }
    if (a > b) { int t = a; a = b; b = t; }
    return b;
}

// Raw touch sample. Returns pressure z1 (0-ish when untouched).
int InputRawTouch(int* prx, int* pry)
{
    digitalWrite(PIN_TOUCH_CS, LOW);

    // Pressure (Z1) detects touch more reliably than the IRQ line across
    // board variants.
    int z1 = xptTransfer16(0xB1);

    int xs[3], ys[3];
    xptTransfer16(0xD1); // throwaway to settle
    for (int i = 0; i < 3; i++) {
        xs[i] = xptTransfer16(0xD1) & 0x0FFF; // X position, keep powered
        ys[i] = xptTransfer16(0x91) & 0x0FFF; // Y position
    }
    xptTransfer16(0x90); // power down on last
    digitalWrite(PIN_TOUCH_CS, HIGH);

    *prx = median3(xs[0], xs[1], xs[2]);
    *pry = median3(ys[0], ys[1], ys[2]);
    return z1;
}

int InputBootPressed(void)
{
    return digitalRead(PIN_BOOT_BTN) == LOW;
}

bool InputTouchRead(int* sx, int* sy)
{
    int rx, ry;
    int z1 = InputRawTouch(&rx, &ry);

    // Raw diagnostics twice a second while touched
    static unsigned long lastDbg = 0;
    bool touched = z1 > 100;
    unsigned long now = millis();
    if (touched && now - lastDbg > 500) {
        printf("[touchdbg] z1=%d rx=%d ry=%d\n", z1, rx, ry);
        lastDbg = now;
    }

    if (!touched)
        return false;

    int x, y;
    if (g_dispCfg.magic == DISPCFG_MAGIC) {
        // Calibrated mapping from the setup wizard
        int rawX = g_dispCfg.touchSwapXY ? ry : rx;
        int rawY = g_dispCfg.touchSwapXY ? rx : ry;
        x = (int)((long)(rawX - g_dispCfg.xRaw0) * 319 /
                  (g_dispCfg.xRaw319 - g_dispCfg.xRaw0));
        y = (int)((long)(rawY - g_dispCfg.yRaw0) * 239 /
                  (g_dispCfg.yRaw239 - g_dispCfg.yRaw0));
    } else {
        if (rx < RAW_MIN || rx > RAW_MAX || ry < RAW_MIN || ry > RAW_MAX)
            return false;
        x = (int)((long)(ry - RAW_MIN) * 320 / (RAW_MAX - RAW_MIN));
        y = 239 - (int)((long)(rx - RAW_MIN) * 240 / (RAW_MAX - RAW_MIN));
    }

    if (x < 0) x = 0; if (x > 319) x = 319;
    if (y < 0) y = 0; if (y > 239) y = 239;
    *sx = x;
    *sy = y;
    return true;
}

static int zoneToKey(int x, int y)
{
    if (y < 48) {
        if (x < 96) return KEYD_START;
        if (x >= 224) return KEYD_SELECT;
    }
    if (x < 176) {
        // virtual d-pad centred in left half of play area
        int dx = x - 88;
        int dy = y - 150;
        if (abs(dx) > abs(dy))
            return dx < 0 ? KEYD_LEFT : KEYD_RIGHT;
        return dy < 0 ? KEYD_UP : KEYD_DOWN;
    }
    return y < 144 ? KEYD_A : KEYD_B;
}

static void touchPoll(void)
{
    int x, y;
    int key = -1;

    if (InputTouchRead(&x, &y))
        key = zoneToKey(x, y);

    if (key != touchKeyHeld) {
        if (touchKeyHeld != -1)
            postKey(ev_keyup, touchKeyHeld);
        if (key != -1) {
            postKey(ev_keydown, key);
            printf("[touch] x=%d y=%d -> key %d\n", x, y, key);
        }
        touchKeyHeld = key;
    }
}

// --------------------------------------------------------------- serial ---

#define MAX_SERIAL_KEYS 8
static struct { int key; unsigned long expiry; } serialKeys[MAX_SERIAL_KEYS];

static int serialCharToKey(int c)
{
    switch (c) {
        case 'w': case 'W': return KEYD_UP;
        case 's': case 'S': return KEYD_DOWN;
        case 'a': case 'A': return KEYD_LEFT;
        case 'd': case 'D': return KEYD_RIGHT;
        case 'q': case 'Q': return KEYD_L;
        case 'e': case 'E': return KEYD_R;
        case 'f': case 'F': return KEYD_B;      // fire
        case ' ':           return KEYD_A;      // use / confirm
        case 'm': case 'M': case '\r': case '\n': return KEYD_START;
        case 't': case 'T': case '\t': return KEYD_SELECT;
    }
    return -1;
}

static void serialPoll(void)
{
    unsigned long now = millis();

    while (Serial.available()) {
        int key = serialCharToKey(Serial.read());
        if (key < 0)
            continue;

        int slot = -1;
        for (int i = 0; i < MAX_SERIAL_KEYS; i++) {
            if (serialKeys[i].key == key) { slot = i; break; }
            if (slot < 0 && serialKeys[i].key == -1) slot = i;
        }
        if (slot < 0)
            continue;

        if (serialKeys[slot].key == -1) {
            serialKeys[slot].key = key;
            postKey(ev_keydown, key);
        }
        serialKeys[slot].expiry = now + SERIAL_KEY_HOLD_MS;
    }

    for (int i = 0; i < MAX_SERIAL_KEYS; i++) {
        if (serialKeys[i].key != -1 && (long)(now - serialKeys[i].expiry) >= 0) {
            postKey(ev_keyup, serialKeys[i].key);
            serialKeys[i].key = -1;
        }
    }
}

// ----------------------------------------------------------- boot button ---

static bool bootHeld = false;

static void bootPoll(void)
{
    bool down = digitalRead(PIN_BOOT_BTN) == LOW;
    if (down != bootHeld) {
        postKey(down ? ev_keydown : ev_keyup, KEYD_START);
        bootHeld = down;
    }
}

// ----------------------------------------------------------------- api ---

void InputInit(void)
{
    pinMode(PIN_TOUCH_CLK, OUTPUT);
    pinMode(PIN_TOUCH_DIN, OUTPUT);
    pinMode(PIN_TOUCH_CS, OUTPUT);
    pinMode(PIN_TOUCH_DOUT, INPUT);
    pinMode(PIN_TOUCH_IRQ, INPUT);
    pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
    digitalWrite(PIN_TOUCH_CS, HIGH);
    digitalWrite(PIN_TOUCH_CLK, LOW);

    for (int i = 0; i < MAX_SERIAL_KEYS; i++)
        serialKeys[i].key = -1;
}

void InputPoll(void)
{
    touchPoll();
    serialPoll();
    bootPoll();
}
