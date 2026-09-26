#ifndef GAME_ENTRY_H
#define GAME_ENTRY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_CATALOG_GAMES 32

typedef struct {
    const char *title;
    const uint8_t *rom_data;
    size_t rom_size;
    uint32_t crc32;
    uint8_t mapper;
    uint8_t pad;
    uint16_t prg_size_kb;
    uint16_t chr_size_kb;
    uint16_t pad2;
} GameEntry;

typedef struct {
    uint32_t magic; // 0x5441434E ("NCAT")
    uint32_t count;
    GameEntry entries[MAX_CATALOG_GAMES];
    char titles[MAX_CATALOG_GAMES][32];
} RomCatalogHeader;

extern const RomCatalogHeader g_rom_catalog;

#define game_catalog ((const GameEntry *)(g_rom_catalog.entries))
#define game_catalog_count (*(volatile const uint32_t *)&(g_rom_catalog.count))

void game_set_current(const GameEntry *game);
const GameEntry *game_get_current(void);

#endif
