#include <eadk.h>
#include <nofrendo.h>
#include <stddef.h>
#include <string.h>
#include "game_entry.h"
#include "menu.h"
#include "statefile_wrapper.h"

const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "NES";
const uint32_t eadk_api_level  __attribute__((section(".rodata.eadk_api_level"))) = 0;

int rand() {
  return 0;
}

const char *osd_getromdata(const char *name) {
  const GameEntry *game = game_get_current();
  if (game && game->rom_data) {
    return (const char *)game->rom_data;
  }
  return NULL;
}

void osd_unloadromdata() {
}

static void waitForKeyReleased() {
  while(eadk_keyboard_scan()) {
    eadk_timing_msleep(10);
  }
}

int main(int argc, char * argv[]) {
  while (1) {
    eadk_display_push_rect_uniform(eadk_screen_rect, eadk_color_black);

    const GameEntry *selected = menu_select_game(game_catalog, game_catalog_count);
    if (selected == NULL) {
      break;
    }

    game_set_current(selected);
    ram_state_clear();
    extern void timing_reset(void);
    timing_reset();

    eadk_display_push_rect_uniform(eadk_screen_rect, eadk_color_black);
    nofrendo_main(0, NULL);

    waitForKeyReleased();
  }

  return 0;
}
