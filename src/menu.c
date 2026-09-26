#include "menu.h"
#include "game_entry.h"
#include "i18n.h"
#include <eadk.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_W           320
#define SCREEN_H           240

#define ITEMS_PER_PAGE     8
#define ROW_HEIGHT         20
#define LIST_START_Y       32
#define HEADER_HEIGHT      30
#define FOOTER_HEIGHT      46
#define FOOTER_Y           (SCREEN_H - FOOTER_HEIGHT)

#define MAX_TITLE_CHARS    26
#define MAX_LINE_BUF       48

#define COLOR_BG           ((eadk_color_t)0x10A2)
#define COLOR_HEADER_BG    ((eadk_color_t)0x1927)
#define COLOR_FOOTER_BG    ((eadk_color_t)0x1927)
#define COLOR_ACCENT       ((eadk_color_t)0x04FF)
#define COLOR_SEL_BG       ((eadk_color_t)0x2378)
#define COLOR_SEL_BORDER   ((eadk_color_t)0x5D3F)

#define COLOR_WHITE        ((eadk_color_t)0xFFFF)
#define COLOR_YELLOW       ((eadk_color_t)0xFFE0)
#define COLOR_CYAN         ((eadk_color_t)0x07FF)
#define COLOR_LIGHT_GRAY   ((eadk_color_t)0xE71C)

static void wait_for_all_keys_released(void) {
  while (eadk_keyboard_scan() != 0) {
    eadk_timing_msleep(10);
  }
}

static const char *get_mapper_name(uint8_t mapper) {
  switch (mapper) {
    case 0: return "NROM";
    case 1: return "MMC1";
    case 2: return "UNROM";
    case 3: return "CNROM";
    case 4: return "MMC3";
    case 7: return "AOROM";
    case 66: return "GxROM";
    default: return "NES";
  }
}

static void truncate_title(char *dest, const char *src, size_t max_len) {
  if (!src) {
    dest[0] = '\0';
    return;
  }
  size_t len = strlen(src);
  if (len <= max_len) {
    strcpy(dest, src);
  } else {
    if (max_len > 3) {
      strncpy(dest, src, max_len - 3);
      dest[max_len - 3] = '\0';
      strcat(dest, "...");
    } else {
      strncpy(dest, src, max_len);
      dest[max_len] = '\0';
    }
  }
}

static void draw_header(size_t current_index, size_t total_count) {
  eadk_display_push_rect_uniform(
      (eadk_rect_t){0, 0, SCREEN_W, HEADER_HEIGHT},
      COLOR_HEADER_BG
  );

  eadk_display_push_rect_uniform(
      (eadk_rect_t){0, HEADER_HEIGHT - 1, SCREEN_W, 1},
      COLOR_ACCENT
  );

  const char *title = (i18n_get_language() == LANG_FR) ? "NOFRENDO - CATALOGUE NES" : "NOFRENDO - NES CATALOG";
  eadk_display_draw_string(
      title,
      (eadk_point_t){12, 8},
      false,
      COLOR_WHITE,
      COLOR_HEADER_BG
  );

  char counter_buf[24];
  const char *lang_str = (i18n_get_language() == LANG_FR) ? "FR" : "EN";
  snprintf(counter_buf, sizeof(counter_buf), "[%s] [%u/%u]", lang_str, (unsigned)(current_index + 1), (unsigned)total_count);
  eadk_display_draw_string(
      counter_buf,
      (eadk_point_t){SCREEN_W - 95, 8},
      false,
      COLOR_YELLOW,
      COLOR_HEADER_BG
  );
}

static void draw_footer(const GameEntry *current_game, bool can_scroll_up, bool can_scroll_down) {
  eadk_display_push_rect_uniform(
      (eadk_rect_t){0, FOOTER_Y, SCREEN_W, FOOTER_HEIGHT},
      COLOR_FOOTER_BG
  );

  eadk_display_push_rect_uniform(
      (eadk_rect_t){0, FOOTER_Y, SCREEN_W, 1},
      COLOR_ACCENT
  );

  if (current_game) {
    char meta_buf[64];
    snprintf(meta_buf, sizeof(meta_buf), "Map:%u (%s) | P:%uK C:%uK | %u Ko",
             current_game->mapper,
             get_mapper_name(current_game->mapper),
             current_game->prg_size_kb,
             current_game->chr_size_kb,
             (unsigned)(current_game->rom_size / 1024));

    eadk_display_draw_string(
        meta_buf,
        (eadk_point_t){10, FOOTER_Y + 5},
        false,
        COLOR_WHITE,
        COLOR_FOOTER_BG
    );
  }

  if (can_scroll_up) {
    eadk_display_draw_string("^", (eadk_point_t){8, FOOTER_Y + 24}, false, COLOR_CYAN, COLOR_FOOTER_BG);
  }
  if (can_scroll_down) {
    eadk_display_draw_string("v", (eadk_point_t){18, FOOTER_Y + 24}, false, COLOR_CYAN, COLOR_FOOTER_BG);
  }

  const char *hint_str = (i18n_get_language() == LANG_FR)
      ? "[OK] Lancer  [Shift] Lang  [Home] Quitter"
      : "[OK] Play    [Shift] Lang  [Home] Exit";

  eadk_display_draw_string(
      hint_str,
      (eadk_point_t){30, FOOTER_Y + 24},
      false,
      COLOR_YELLOW,
      COLOR_FOOTER_BG
  );
}

static void draw_game_list(const GameEntry *games, size_t count, size_t selected, size_t top_index) {
  for (size_t i = 0; i < ITEMS_PER_PAGE; i++) {
    size_t game_idx = top_index + i;
    uint16_t row_y = LIST_START_Y + (i * ROW_HEIGHT);

    if (game_idx < count) {
      bool is_selected = (game_idx == selected);
      eadk_color_t row_bg = is_selected ? COLOR_SEL_BG : COLOR_BG;
      eadk_color_t text_col = is_selected ? COLOR_WHITE : COLOR_LIGHT_GRAY;

      eadk_display_push_rect_uniform(
          (eadk_rect_t){0, row_y, SCREEN_W, ROW_HEIGHT},
          COLOR_BG
      );
      eadk_display_push_rect_uniform(
          (eadk_rect_t){6, row_y + 1, SCREEN_W - 12, ROW_HEIGHT - 2},
          row_bg
      );

      if (is_selected) {
        eadk_display_push_rect_uniform(
            (eadk_rect_t){6, row_y + 1, 3, ROW_HEIGHT - 2},
            COLOR_CYAN
        );
      }

      char clean_title[MAX_TITLE_CHARS + 4];
      truncate_title(clean_title, games[game_idx].title, MAX_TITLE_CHARS);

      char line_str[MAX_LINE_BUF];
      snprintf(line_str, sizeof(line_str), "%s %2u. %s",
               is_selected ? ">" : " ",
               (unsigned)(game_idx + 1),
               clean_title);

      eadk_display_draw_string(
          line_str,
          (eadk_point_t){12, row_y + 3},
          false,
          text_col,
          row_bg
      );

      char size_str[16];
      snprintf(size_str, sizeof(size_str), "%u Ko", (unsigned)(games[game_idx].rom_size / 1024));
      eadk_display_draw_string(
          size_str,
          (eadk_point_t){SCREEN_W - 65, row_y + 3},
          false,
          is_selected ? COLOR_YELLOW : COLOR_CYAN,
          row_bg
      );
    } else {
      eadk_display_push_rect_uniform(
          (eadk_rect_t){0, row_y, SCREEN_W, ROW_HEIGHT},
          COLOR_BG
      );
    }
  }
}

const GameEntry *menu_select_game(const GameEntry *games, size_t count) {
  if (!games || count == 0) {
    return NULL;
  }

  if (count == 1) {
    return &games[0];
  }

  size_t selected = 0;
  size_t top_index = 0;
  bool needs_full_redraw = true;

  wait_for_all_keys_released();

  eadk_display_push_rect_uniform(eadk_screen_rect, COLOR_BG);

  eadk_keyboard_state_t prev_state = 0;
  uint32_t repeat_counter = 0;

  while (1) {
    if (needs_full_redraw) {
      draw_header(selected, count);
      draw_game_list(games, count, selected, top_index);
      draw_footer(&games[selected], top_index > 0, (top_index + ITEMS_PER_PAGE) < count);
      needs_full_redraw = false;
    }

    eadk_keyboard_state_t cur_state = eadk_keyboard_scan();

    if (eadk_keyboard_key_down(cur_state, eadk_key_ok) ||
        eadk_keyboard_key_down(cur_state, eadk_key_exe)) {
      wait_for_all_keys_released();
      return &games[selected];
    }

    if (eadk_keyboard_key_down(cur_state, eadk_key_home) ||
        eadk_keyboard_key_down(cur_state, eadk_key_back)) {
      wait_for_all_keys_released();
      return NULL;
    }

    bool key_shift = eadk_keyboard_key_down(cur_state, eadk_key_shift) &&
                     !eadk_keyboard_key_down(prev_state, eadk_key_shift);
    if (key_shift) {
      i18n_toggle_language();
      needs_full_redraw = true;
    }

    bool key_up = eadk_keyboard_key_down(cur_state, eadk_key_up);
    bool key_down = eadk_keyboard_key_down(cur_state, eadk_key_down);
    bool key_left = eadk_keyboard_key_down(cur_state, eadk_key_left);
    bool key_right = eadk_keyboard_key_down(cur_state, eadk_key_right);

    bool trigger_up = false;
    bool trigger_down = false;
    bool trigger_page_up = false;
    bool trigger_page_down = false;

    if (key_up || key_down || key_left || key_right) {
      if (prev_state == 0) {
        trigger_up = key_up;
        trigger_down = key_down;
        trigger_page_up = key_left;
        trigger_page_down = key_right;
        repeat_counter = 0;
      } else {
        repeat_counter++;
        if (repeat_counter >= 12 && (repeat_counter % 4) == 0) {
          trigger_up = key_up;
          trigger_down = key_down;
          trigger_page_up = key_left;
          trigger_page_down = key_right;
        }
      }
    } else {
      repeat_counter = 0;
    }

    if (trigger_up) {
      if (selected > 0) {
        selected--;
      } else {
        selected = count - 1;
      }
      if (selected < top_index) {
        top_index = selected;
      } else if (selected >= top_index + ITEMS_PER_PAGE) {
        top_index = selected - ITEMS_PER_PAGE + 1;
      }
      needs_full_redraw = true;
    } else if (trigger_down) {
      if (selected + 1 < count) {
        selected++;
      } else {
        selected = 0;
      }
      if (selected < top_index) {
        top_index = selected;
      } else if (selected >= top_index + ITEMS_PER_PAGE) {
        top_index = selected - ITEMS_PER_PAGE + 1;
      }
      needs_full_redraw = true;
    } else if (trigger_page_up) {
      if (selected >= ITEMS_PER_PAGE) {
        selected -= ITEMS_PER_PAGE;
      } else {
        selected = 0;
      }
      if (top_index >= ITEMS_PER_PAGE) {
        top_index -= ITEMS_PER_PAGE;
      } else {
        top_index = 0;
      }
      needs_full_redraw = true;
    } else if (trigger_page_down) {
      if (selected + ITEMS_PER_PAGE < count) {
        selected += ITEMS_PER_PAGE;
      } else {
        selected = count - 1;
      }
      if (top_index + ITEMS_PER_PAGE < count) {
        top_index += ITEMS_PER_PAGE;
      }
      needs_full_redraw = true;
    }

    prev_state = cur_state;
    eadk_timing_msleep(20);
  }
}
