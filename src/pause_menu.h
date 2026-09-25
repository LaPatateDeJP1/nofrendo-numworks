#ifndef PAUSE_MENU_H
#define PAUSE_MENU_H

#include <stdbool.h>

typedef enum {
  PAUSE_ACTION_RESUME,
  PAUSE_ACTION_RESET,
  PAUSE_ACTION_QUIT_TO_CATALOG
} PauseAction;

PauseAction pause_menu_show(const char *game_title, bool *fast_forward);

#endif
