#include "menu.h"
#include "game_entry.h"
#include "settings.h"
#include "i18n.h"
#include <eadk.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_W           320
#define SCREEN_H           240

#define ITEMS_PER_PAGE     8
#define ROW_HEIGHT         19
#define LIST_START_Y       28
#define HEADER_HEIGHT      26
#define FOOTER_HEIGHT      48
#define FOOTER_Y           (SCREEN_H - FOOTER_HEIGHT)

#define MAX_TITLE_CHARS    24

#define COLOR_BG           ((eadk_color_t)0x08A5)
#define COLOR_HEADER_BG    ((eadk_color_t)0x112C)
#define COLOR_FOOTER_BG    ((eadk_color_t)0x0C88)
#define COLOR_ACCENT       ((eadk_color_t)0x067F)
#define COLOR_SEL_BG       ((eadk_color_t)0x1B38)
#define COLOR_SEL_BAR      ((eadk_color_t)0x07FF)
#define COLOR_TEXT_MUTED   ((eadk_color_t)0x8C92)
#define COLOR_TEXT_DIM     ((eadk_color_t)0xB5B7)
#define COLOR_BADGE_BG     ((eadk_color_t)0x18E5)

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
    case 5: return "MMC5";
    case 7: return "AOROM";
    case 9: return "MMC2";
    case 10: return "MMC4";
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

  char title_buf[32];
  snprintf(title_buf, sizeof(title_buf), "%s", i18n_str(STR_APP_TITLE));
  eadk_display_draw_string(
      title_buf,
      (eadk_point_t){8, 7},
      false,
      eadk_color_white,
      COLOR_HEADER_BG
  );

  char count_buf[24];
  snprintf(count_buf, sizeof(count_buf), "[%u/%u Games]", (unsigned)(current_index + 1), (unsigned)total_count);
  eadk_display_draw_string(
      count_buf,
      (eadk_point_t){SCREEN_W - 130, 7},
      false,
      COLOR_TEXT_MUTED,
      COLOR_HEADER_BG
  );

  const char *lang_tag = (i18n_get_language() == LANG_FR) ? "FR" : "EN";
  eadk_display_push_rect_uniform((eadk_rect_t){SCREEN_W - 32, 5, 24, 15}, COLOR_BADGE_BG);
  eadk_display_draw_string(
      lang_tag,
      (eadk_point_t){SCREEN_W - 28, 7},
      false,
      eadk_color_white,
      COLOR_BADGE_BG
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
    char line1[64];
    snprintf(line1, sizeof(line1), "%s %s (Mapper %u)",
             i18n_str(STR_CART_INFO),
             get_mapper_name(current_game->mapper),
             current_game->mapper);
    eadk_display_draw_string(
        line1,
        (eadk_point_t){8, FOOTER_Y + 5},
        false,
        eadk_color_white,
        COLOR_FOOTER_BG
    );

    char line2[64];
    snprintf(line2, sizeof(line2), "PRG:%uK  CHR:%uK  %s:%uK",
             (unsigned)(current_game->prg_size_kb),
             (unsigned)(current_game->chr_size_kb),
             i18n_str(STR_SIZE),
             (unsigned)(current_game->rom_size / 1024));
    eadk_display_draw_string(
        line2,
        (eadk_point_t){8, FOOTER_Y + 19},
        false,
        COLOR_TEXT_DIM,
        COLOR_FOOTER_BG
    );
  }

  char hint[64];
  snprintf(hint, sizeof(hint), "%s  %s  %s  %s",
           i18n_str(STR_HINT_PLAY),
           i18n_str(STR_HINT_PAGE),
           i18n_str(STR_HINT_LANG),
           i18n_str(STR_HINT_EXIT));
  eadk_display_draw_string(
      hint,
      (eadk_point_t){8, FOOTER_Y + 34},
      false,
      COLOR_TEXT_MUTED,
      COLOR_FOOTER_BG
  );
}

static void draw_game_list(const GameEntry *games, size_t count, size_t selected, size_t page_start) {
  for (size_t i = 0; i < ITEMS_PER_PAGE; i++) {
    size_t index = page_start + i;
    uint16_t row_y = LIST_START_Y + (i * ROW_HEIGHT);

    if (index < count) {
      bool is_selected = (index == selected);
      eadk_color_t row_bg = is_selected ? COLOR_SEL_BG : COLOR_BG;
      eadk_color_t text_col = is_selected ? eadk_color_white : COLOR_TEXT_DIM;

      eadk_display_push_rect_uniform(
          (eadk_rect_t){0, row_y, SCREEN_W, ROW_HEIGHT},
          COLOR_BG
      );
      eadk_display_push_rect_uniform(
          (eadk_rect_t){4, row_y + 1, SCREEN_W - 8, ROW_HEIGHT - 2},
          row_bg
      );

      if (is_selected) {
        eadk_display_push_rect_uniform(
            (eadk_rect_t){4, row_y + 1, 3, ROW_HEIGHT - 2},
            COLOR_SEL_BAR
        );
      }

      char row_text[64];
      char short_title[MAX_TITLE_CHARS + 1];
      truncate_title(short_title, games[index].title, MAX_TITLE_CHARS);
      snprintf(row_text, sizeof(row_text), "%s %u. %s",
               is_selected ? ">" : " ",
               (unsigned)(index + 1),
               short_title);

      eadk_display_draw_string(
          row_text,
          (eadk_point_t){8, row_y + 3},
          false,
          text_col,
          row_bg
      );

      char badge[16];
      if (is_selected) {
        snprintf(badge, sizeof(badge), "[%s]", get_mapper_name(games[index].mapper));
      } else {
        snprintf(badge, sizeof(badge), "%uK", (unsigned)(games[index].rom_size / 1024));
      }
      eadk_display_draw_string(
          badge,
          (eadk_point_t){SCREEN_W - 58, row_y + 3},
          false,
          is_selected ? COLOR_SEL_BAR : COLOR_TEXT_MUTED,
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

  wait_for_all_keys_released();

  size_t selected = 0;
  size_t prev_selected = 0;
  size_t page_start = 0;
  size_t prev_page_start = 0;

  bool full_redraw = true;

  eadk_display_push_rect_uniform(eadk_screen_rect, COLOR_BG);

  eadk_keyboard_state_t prev_state = 0;
  uint32_t repeat_counter = 0;

  while (1) {
    if (full_redraw) {
      draw_header(selected, count);
      draw_game_list(games, count, selected, page_start);
      draw_footer(&games[selected], page_start > 0, page_start + ITEMS_PER_PAGE < count);
      full_redraw = false;
      prev_selected = selected;
      prev_page_start = page_start;
    } else if (page_start != prev_page_start) {
      draw_header(selected, count);
      draw_game_list(games, count, selected, page_start);
      draw_footer(&games[selected], page_start > 0, page_start + ITEMS_PER_PAGE < count);
      prev_selected = selected;
      prev_page_start = page_start;
    } else if (selected != prev_selected) {
      draw_header(selected, count);
      draw_game_list(games, count, selected, page_start);
      draw_footer(&games[selected], page_start > 0, page_start + ITEMS_PER_PAGE < count);
      prev_selected = selected;
    }

    eadk_keyboard_state_t cur_state = eadk_keyboard_scan();

    if (eadk_keyboard_key_down(cur_state, eadk_key_ok) ||
        eadk_keyboard_key_down(cur_state, eadk_key_exe)) {
      wait_for_all_keys_released();
      return &games[selected];
    }

    if (eadk_keyboard_key_down(cur_state, eadk_key_home)) {
      wait_for_all_keys_released();
      return NULL;
    }

    bool key_shift = eadk_keyboard_key_down(cur_state, eadk_key_shift) &&
                     !eadk_keyboard_key_down(prev_state, eadk_key_shift);
    if (key_shift) {
      i18n_toggle_language();
      g_settings.language = i18n_get_language();
      full_redraw = true;
    }

    bool key_up = eadk_keyboard_key_down(cur_state, eadk_key_up);
    bool key_down = eadk_keyboard_key_down(cur_state, eadk_key_down);
    bool key_left = eadk_keyboard_key_down(cur_state, eadk_key_left);
    bool key_right = eadk_keyboard_key_down(cur_state, eadk_key_right);

    bool up_pressed = key_up && (!eadk_keyboard_key_down(prev_state, eadk_key_up) ||
                                 (repeat_counter > 15 && (repeat_counter % 4 == 0)));
    bool down_pressed = key_down && (!eadk_keyboard_key_down(prev_state, eadk_key_down) ||
                                     (repeat_counter > 15 && (repeat_counter % 4 == 0)));

    bool left_pressed = key_left && !eadk_keyboard_key_down(prev_state, eadk_key_left);
    bool right_pressed = key_right && !eadk_keyboard_key_down(prev_state, eadk_key_right);

    if (key_up || key_down) {
      repeat_counter++;
    } else {
      repeat_counter = 0;
    }

    if (up_pressed) {
      if (selected == 0) {
        selected = count - 1;
        page_start = (selected / ITEMS_PER_PAGE) * ITEMS_PER_PAGE;
      } else {
        selected--;
        if (selected < page_start) {
          page_start = (selected >= ITEMS_PER_PAGE) ? (selected - ITEMS_PER_PAGE + 1) : 0;
          page_start = (page_start / ITEMS_PER_PAGE) * ITEMS_PER_PAGE;
        }
      }
    } else if (down_pressed) {
      if (selected + 1 >= count) {
        selected = 0;
        page_start = 0;
      } else {
        selected++;
        if (selected >= page_start + ITEMS_PER_PAGE) {
          page_start += ITEMS_PER_PAGE;
        }
      }
    } else if (left_pressed) {
      if (page_start >= ITEMS_PER_PAGE) {
        page_start -= ITEMS_PER_PAGE;
        selected = page_start;
      } else {
        selected = 0;
      }
    } else if (right_pressed) {
      if (page_start + ITEMS_PER_PAGE < count) {
        page_start += ITEMS_PER_PAGE;
        selected = page_start;
      } else {
        selected = count - 1;
      }
    }

    prev_state = cur_state;
    eadk_timing_msleep(20);
  }
}
