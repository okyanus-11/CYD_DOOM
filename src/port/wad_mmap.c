// Memory-maps the "wad" flash partition and exposes it as doom_iwad,
// replacing GBADoom's linked-in C array.
#include <stdint.h>
#include "esp_partition.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "doomport.h"

const unsigned char* doom_iwad = 0;
unsigned int doom_iwad_len = 0;

extern void I_Error(const char* error, ...);

void WadInit(void)
{
    const esp_partition_t* part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, 0x41, "wad");

    if (!part)
        I_Error("WadInit: no 'wad' partition");

    const void* ptr = 0;
    spi_flash_mmap_handle_t handle;

    esp_err_t err = esp_partition_mmap(part, 0, part->size,
                                       SPI_FLASH_MMAP_DATA, &ptr, &handle);
    if (err != ESP_OK)
        I_Error("WadInit: mmap failed (%d)", (int)err);

    const unsigned char* p = (const unsigned char*)ptr;

    if (p[0] != 'I' || p[1] != 'W' || p[2] != 'A' || p[3] != 'D')
        I_Error("WadInit: partition does not contain an IWAD (flash it with tools/flashwad.py)");

    doom_iwad = p;
    doom_iwad_len = part->size;

    ESP_LOGI("wad", "IWAD mapped at %p (%u bytes)", ptr, (unsigned)part->size);
}
