#ifndef GAME_ENTRY_H
#define GAME_ENTRY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    const char *title;
    const uint8_t *rom_data;
    size_t rom_size;
    uint32_t crc32;
    uint8_t mapper;
    uint16_t prg_size_kb;
    uint16_t chr_size_kb;
} GameEntry;

extern const GameEntry game_catalog[];
extern const size_t game_catalog_count;

void game_set_current(const GameEntry *game);
const GameEntry *game_get_current(void);

#endif
