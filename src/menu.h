#ifndef MENU_H
#define MENU_H

#include "game_entry.h"
#include <stddef.h>

const GameEntry *menu_select_game(const GameEntry *games, size_t count);

#endif
