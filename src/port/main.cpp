// DOOM on the ESP32-2432S028R "Cheap Yellow Display".
// Port of GBADoom (doomhack), which is based on PrBoom.
#include <Arduino.h>
#include "doomport.h"

extern "C" {
#include "doomdef.h"
#include "d_main.h"
#include "z_zone.h"
#include "global_data.h"
void I_PreInitGraphics(void);
void InitGlobals(void);
}

static void doomTask(void*)
{
    WadInit();
    SramInit();

    printf("heap before Z_Init: free=%u largest=%u\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    // Z_Init first: the zone wants the largest contiguous heap block, so it
    // must run before the display/framebuffer allocations fragment the heap.
    Z_Init();

    printf("heap after Z_Init: free=%u largest=%u\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    I_PreInitGraphics();   // display + input + framebuffers

    SetupRunIfNeeded();    // first-boot wizard (or hold BOOT at power-on)

    InitGlobals();

    D_DoomMain();

    for (;;)
        vTaskDelay(portMAX_DELAY);
}

// Static stack keeps 24KB out of the heap regions the zone/framebuffers need.
static StaticTask_t s_doomTcb;
static StackType_t s_doomStack[24576 / sizeof(StackType_t)];

void setup()
{
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println("DOOM for ESP32-2432S028R (GBADoom/PrBoom port)");
    Serial.println("keys: w/a/s/d move, q/e strafe, f fire, space use, m menu, t map");

    xTaskCreateStaticPinnedToCore(doomTask, "doom", 24576 / sizeof(StackType_t),
                                  nullptr, 2, s_doomStack, &s_doomTcb, 1);

    vTaskDelete(nullptr); // setup runs in loopTask; we don't need it
}

void loop() {}
