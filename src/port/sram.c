// Save games + settings, backed by the "sram" flash partition.
// GBADoom writes < 2KB total (8 save slots + settings), so everything lives
// in a small RAM shadow that is flushed to one 4KB flash sector on save.
#include <string.h>
#include "esp_partition.h"
#include "doomport.h"

#define SRAM_SHADOW_SIZE 4096

static const esp_partition_t* s_part = 0;
static unsigned char s_shadow[SRAM_SHADOW_SIZE];

disp_cfg_t g_dispCfg;

extern void I_Error(const char* error, ...);

void SramInit(void)
{
    s_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, "sram");

    if (!s_part)
        I_Error("SramInit: no 'sram' partition");

    esp_partition_read(s_part, 0, s_shadow, SRAM_SHADOW_SIZE);

    // Fresh flash reads as 0xFF; GBADoom expects zero-filled virgin SRAM
    // (it checks explicit cookies, but be tidy about it).
    int blank = 1;
    for (int i = 0; i < 64; i++)
        if (s_shadow[i] != 0xFF) { blank = 0; break; }
    if (blank)
        memset(s_shadow, 0, SRAM_SHADOW_SIZE);

    memcpy(&g_dispCfg, s_shadow + DISPCFG_SRAM_OFFSET, sizeof(g_dispCfg));
    if (g_dispCfg.magic != DISPCFG_MAGIC)
        memset(&g_dispCfg, 0, sizeof(g_dispCfg));
}

void CfgSave(void)
{
    memcpy(s_shadow + DISPCFG_SRAM_OFFSET, &g_dispCfg, sizeof(g_dispCfg));
    esp_partition_erase_range(s_part, 0, SRAM_SHADOW_SIZE);
    esp_partition_write(s_part, 0, s_shadow, SRAM_SHADOW_SIZE);
}

void SramRead(unsigned char* dst, unsigned int size, unsigned int offset)
{
    if (offset + size > SRAM_SHADOW_SIZE)
        I_Error("SramRead: out of range (%u+%u)", offset, size);

    memcpy(dst, s_shadow + offset, size);
}

void SramWrite(const unsigned char* src, unsigned int size, unsigned int offset)
{
    if (offset + size > SRAM_SHADOW_SIZE)
        I_Error("SramWrite: out of range (%u+%u)", offset, size);

    memcpy(s_shadow + offset, src, size);

    esp_partition_erase_range(s_part, 0, SRAM_SHADOW_SIZE);
    esp_partition_write(s_part, 0, s_shadow, SRAM_SHADOW_SIZE);
}
