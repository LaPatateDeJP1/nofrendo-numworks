#include "pause_menu.h"
#include "display.h"
#include "settings.h"
#include "i18n.h"
#include "statefile_wrapper.h"
#include <eadk.h>
#include <stdio.h>
#include <string.h>

#define WIN_W              272
#define WIN_H              172
#define WIN_X              ((EADK_SCREEN_WIDTH - WIN_W) / 2)
#define WIN_Y              ((EADK_SCREEN_HEIGHT - WIN_H) / 2)

#define COLOR_BORDER       ((eadk_color_t)0x051F)
#define COLOR_BG           ((eadk_color_t)0x08A5)
#define COLOR_HEADER_BG    ((eadk_color_t)0x112C)
#define COLOR_SEL_BG       ((eadk_color_t)0x1B38)
#define COLOR_SEL_BAR      ((eadk_color_t)0x067F)
#define COLOR_MUTED        ((eadk_color_t)0x8C92)
#define COLOR_ACCENT       ((eadk_color_t)0x07FF)
#define COLOR_SUCCESS_BG   ((eadk_color_t)0x1404)
#define COLOR_ALERT_BG     ((eadk_color_t)0x8000)

#define VISIBLE_ITEMS      7
#define TOTAL_ITEMS        11

enum {
  ITEM_RESUME = 0,
  ITEM_SAVE_STATE,
  ITEM_LOAD_STATE,
  ITEM_SPEED,
  ITEM_SCALE,
  ITEM_PALETTE,
  ITEM_BRIGHTNESS,
  ITEM_FPS,
  ITEM_LANGUAGE,
  ITEM_RESET,
  ITEM_EXIT
};

static void wait_for_keys_released(void) {
  while (eadk_keyboard_scan() != 0) {
    eadk_timing_msleep(10);
  }
}

static void show_toast(const char *msg, bool is_success) {
  uint16_t toast_w = 210;
  uint16_t toast_h = 24;
  uint16_t toast_x = (EADK_SCREEN_WIDTH - toast_w) / 2;
  uint16_t toast_y = WIN_Y + WIN_H - 32;

  eadk_color_t bg = is_success ? COLOR_SUCCESS_BG : COLOR_ALERT_BG;
  eadk_display_push_rect_uniform((eadk_rect_t){toast_x, toast_y, toast_w, toast_h}, bg);
  eadk_display_push_rect_uniform((eadk_rect_t){toast_x, toast_y, toast_w, 1}, eadk_color_white);
  eadk_display_push_rect_uniform((eadk_rect_t){toast_x, toast_y + toast_h - 1, toast_w, 1}, eadk_color_white);

  eadk_display_draw_string(msg, (eadk_point_t){toast_x + 10, toast_y + 6}, false, eadk_color_white, bg);
  eadk_timing_msleep(600);
}

PauseAction pause_menu_show(const char *game_title, bool *fast_forward) {
  wait_for_keys_released();

  int selected = 0;
  int top_index = 0;
  bool local_ff = fast_forward ? *fast_forward : false;
  bool needs_redraw = true;

  eadk_keyboard_state_t prev_state = 0;
  uint32_t repeat_counter = 0;

  while (1) {
    if (needs_redraw) {
      eadk_display_push_rect_uniform((eadk_rect_t){WIN_X, WIN_Y, WIN_W, WIN_H}, COLOR_BORDER);
      eadk_display_push_rect_uniform((eadk_rect_t){WIN_X + 2, WIN_Y + 2, WIN_W - 4, WIN_H - 4}, COLOR_BG);
      eadk_display_push_rect_uniform((eadk_rect_t){WIN_X + 2, WIN_Y + 2, WIN_W - 4, 22}, COLOR_HEADER_BG);
      eadk_display_push_rect_uniform((eadk_rect_t){WIN_X + 2, WIN_Y + 23, WIN_W - 4, 1}, COLOR_BORDER);

      char title_buf[32];
      snprintf(title_buf, sizeof(title_buf), "%s: %.10s", i18n_str(STR_PAUSE_TITLE), game_title ? game_title : "NES");
      eadk_display_draw_string(title_buf, (eadk_point_t){WIN_X + 8, WIN_Y + 6}, false, eadk_color_white, COLOR_HEADER_BG);

      const char *lang_badge = (i18n_get_language() == LANG_FR) ? "FR" : "EN";
      eadk_display_push_rect_uniform((eadk_rect_t){WIN_X + WIN_W - 32, WIN_Y + 5, 24, 14}, COLOR_SEL_BG);
      eadk_display_draw_string(lang_badge, (eadk_point_t){WIN_X + WIN_W - 28, WIN_Y + 6}, false, eadk_color_white, COLOR_SEL_BG);

      for (int i = 0; i < VISIBLE_ITEMS; i++) {
        int item_idx = top_index + i;
        if (item_idx >= TOTAL_ITEMS) break;

        uint16_t row_y = WIN_Y + 26 + (i * 18);
        bool is_sel = (item_idx == selected);
        eadk_color_t row_bg = is_sel ? COLOR_SEL_BG : COLOR_BG;
        eadk_color_t text_col = is_sel ? eadk_color_white : ((eadk_color_t)0xD6BA);

        eadk_display_push_rect_uniform((eadk_rect_t){WIN_X + 4, row_y, WIN_W - 8, 17}, row_bg);
        if (is_sel) {
          eadk_display_push_rect_uniform((eadk_rect_t){WIN_X + 4, row_y, 3, 17}, COLOR_SEL_BAR);
        }

        char label[44];
        switch (item_idx) {
          case ITEM_RESUME:
            snprintf(label, sizeof(label), "%s", i18n_str(STR_PAUSE_RESUME));
            break;
          case ITEM_SAVE_STATE:
            snprintf(label, sizeof(label), "%s", i18n_str(STR_PAUSE_SAVE_STATE));
            break;
          case ITEM_LOAD_STATE:
            snprintf(label, sizeof(label), "%s [%s]",
                     i18n_str(STR_PAUSE_LOAD_STATE),
                     ram_state_exists() ? i18n_str(STR_READY) : i18n_str(STR_EMPTY));
            break;
          case ITEM_SPEED:
            snprintf(label, sizeof(label), "%s: %s",
                     i18n_str(STR_PAUSE_SPEED),
                     local_ff ? "2x Fast" : "1x Normal");
            break;
          case ITEM_SCALE:
            snprintf(label, sizeof(label), "%s: %s",
                     i18n_str(STR_PAUSE_SCALE),
                     (display_get_scale_mode() == VIDEO_SCALE_FULLSCREEN)
                       ? i18n_str(STR_SCALE_FULL)
                       : i18n_str(STR_SCALE_4_3));
            break;
          case ITEM_PALETTE:
            snprintf(label, sizeof(label), "%s: %s",
                     i18n_str(STR_PAUSE_PALETTE),
                     display_get_palette_mode_name(display_get_palette_mode()));
            break;
          case ITEM_BRIGHTNESS:
            snprintf(label, sizeof(label), "%s: %d%%",
                     i18n_str(STR_PAUSE_BRIGHTNESS),
                     settings_get_brightness_pct());
            break;
          case ITEM_FPS:
            snprintf(label, sizeof(label), "%s: %s",
                     i18n_str(STR_PAUSE_FPS),
                     display_get_show_fps() ? i18n_str(STR_ON) : i18n_str(STR_OFF));
            break;
          case ITEM_LANGUAGE:
            snprintf(label, sizeof(label), "%s: %s",
                     i18n_str(STR_PAUSE_LANGUAGE),
                     i18n_get_lang_name(i18n_get_language()));
            break;
          case ITEM_RESET:
            snprintf(label, sizeof(label), "%s", i18n_str(STR_PAUSE_RESET));
            break;
          case ITEM_EXIT:
            snprintf(label, sizeof(label), "%s", i18n_str(STR_PAUSE_EXIT));
            break;
          default:
            snprintf(label, sizeof(label), "Option");
            break;
        }

        char item_str[48];
        snprintf(item_str, sizeof(item_str), "%s %s", is_sel ? ">" : " ", label);
        eadk_display_draw_string(item_str, (eadk_point_t){WIN_X + 10, row_y + 2}, false, text_col, row_bg);
      }

      if (top_index > 0) {
        eadk_display_draw_string("^", (eadk_point_t){WIN_X + WIN_W - 14, WIN_Y + 28}, false, COLOR_ACCENT, COLOR_BG);
      }
      if (top_index + VISIBLE_ITEMS < TOTAL_ITEMS) {
        eadk_display_draw_string("v", (eadk_point_t){WIN_X + WIN_W - 14, WIN_Y + WIN_H - 24}, false, COLOR_ACCENT, COLOR_BG);
      }

      char hint_buf[40];
      snprintf(hint_buf, sizeof(hint_buf), "%s: OK  |  %s: Back",
               (selected == ITEM_RESUME || selected == ITEM_EXIT || selected == ITEM_RESET || selected == ITEM_SAVE_STATE || selected == ITEM_LOAD_STATE) ? "Select" : "Toggle",
               "Resume");
      eadk_display_push_rect_uniform((eadk_rect_t){WIN_X + 4, WIN_Y + WIN_H - 16, WIN_W - 8, 14}, COLOR_HEADER_BG);
      eadk_display_draw_string(hint_buf, (eadk_point_t){WIN_X + 12, WIN_Y + WIN_H - 14}, false, COLOR_MUTED, COLOR_HEADER_BG);

      needs_redraw = false;
    }

    eadk_keyboard_state_t cur_state = eadk_keyboard_scan();

    if (eadk_keyboard_key_down(cur_state, eadk_key_toolbox)) {
      wait_for_keys_released();
      if (fast_forward) *fast_forward = local_ff;
      return PAUSE_ACTION_RESUME;
    }

    bool key_back = eadk_keyboard_key_down(cur_state, eadk_key_back) &&
                    !eadk_keyboard_key_down(prev_state, eadk_key_back);
    if (key_back) {
      wait_for_keys_released();
      if (fast_forward) *fast_forward = local_ff;
      return PAUSE_ACTION_RESUME;
    }

    bool key_up = eadk_keyboard_key_down(cur_state, eadk_key_up);
    bool key_down = eadk_keyboard_key_down(cur_state, eadk_key_down);
    bool key_left = eadk_keyboard_key_down(cur_state, eadk_key_left) &&
                    !eadk_keyboard_key_down(prev_state, eadk_key_left);
    bool key_right = eadk_keyboard_key_down(cur_state, eadk_key_right) &&
                     !eadk_keyboard_key_down(prev_state, eadk_key_right);
    bool key_action = (eadk_keyboard_key_down(cur_state, eadk_key_ok) &&
                       !eadk_keyboard_key_down(prev_state, eadk_key_ok)) ||
                      (eadk_keyboard_key_down(cur_state, eadk_key_exe) &&
                       !eadk_keyboard_key_down(prev_state, eadk_key_exe));

    bool up_pressed = key_up && (!eadk_keyboard_key_down(prev_state, eadk_key_up) ||
                                 (repeat_counter > 15 && (repeat_counter % 4 == 0)));
    bool down_pressed = key_down && (!eadk_keyboard_key_down(prev_state, eadk_key_down) ||
                                     (repeat_counter > 15 && (repeat_counter % 4 == 0)));

    if (key_up || key_down) {
      repeat_counter++;
    } else {
      repeat_counter = 0;
    }

    if (up_pressed) {
      selected = (selected > 0) ? (selected - 1) : (TOTAL_ITEMS - 1);
      if (selected < top_index) {
        top_index = selected;
      } else if (selected >= top_index + VISIBLE_ITEMS) {
        top_index = selected - VISIBLE_ITEMS + 1;
      }
      needs_redraw = true;
    } else if (down_pressed) {
      selected = (selected + 1) % TOTAL_ITEMS;
      if (selected >= top_index + VISIBLE_ITEMS) {
        top_index = selected - VISIBLE_ITEMS + 1;
      } else if (selected < top_index) {
        top_index = selected;
      }
      needs_redraw = true;
    }

    if (key_action || key_left || key_right) {
      switch (selected) {
        case ITEM_RESUME:
          if (key_action) {
            wait_for_keys_released();
            if (fast_forward) *fast_forward = local_ff;
            return PAUSE_ACTION_RESUME;
          }
          break;

        case ITEM_SAVE_STATE:
          if (key_action) {
            int ret = ram_state_save();
            show_toast((ret == 0) ? i18n_str(STR_STATE_SAVED) : "Save Error!", ret == 0);
            needs_redraw = true;
          }
          break;

        case ITEM_LOAD_STATE:
          if (key_action) {
            if (ram_state_exists()) {
              int ret = ram_state_load();
              show_toast((ret == 0) ? i18n_str(STR_STATE_LOADED) : "Load Error!", ret == 0);
            } else {
              show_toast(i18n_str(STR_STATE_EMPTY), false);
            }
            needs_redraw = true;
          }
          break;

        case ITEM_SPEED:
          local_ff = !local_ff;
          needs_redraw = true;
          break;

        case ITEM_SCALE: {
          VideoScaleMode cur_mode = display_get_scale_mode();
          display_set_scale_mode((cur_mode == VIDEO_SCALE_4_3) ? VIDEO_SCALE_FULLSCREEN : VIDEO_SCALE_4_3);
          g_settings.scale_mode = display_get_scale_mode();
          needs_redraw = true;
          break;
        }

        case ITEM_PALETTE: {
          PaletteMode cur_pal = display_get_palette_mode();
          if (key_left) {
            cur_pal = (cur_pal > 0) ? (cur_pal - 1) : 4;
          } else {
            cur_pal = (cur_pal + 1) % 5;
          }
          display_set_palette_mode(cur_pal);
          g_settings.palette_mode = cur_pal;
          needs_redraw = true;
          break;
        }

        case ITEM_BRIGHTNESS:
          settings_cycle_brightness();
          needs_redraw = true;
          break;

        case ITEM_FPS: {
          bool cur_fps = display_get_show_fps();
          display_set_show_fps(!cur_fps);
          g_settings.show_fps = !cur_fps;
          needs_redraw = true;
          break;
        }

        case ITEM_LANGUAGE:
          i18n_toggle_language();
          g_settings.language = i18n_get_language();
          needs_redraw = true;
          break;

        case ITEM_RESET:
          if (key_action) {
            wait_for_keys_released();
            if (fast_forward) *fast_forward = local_ff;
            return PAUSE_ACTION_RESET;
          }
          break;

        case ITEM_EXIT:
          if (key_action) {
            wait_for_keys_released();
            if (fast_forward) *fast_forward = local_ff;
            return PAUSE_ACTION_QUIT_TO_CATALOG;
          }
          break;

        default:
          break;
      }
    }

    prev_state = cur_state;
    eadk_timing_msleep(20);
  }
}
