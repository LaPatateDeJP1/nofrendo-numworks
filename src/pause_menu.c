#include "pause_menu.h"
#include "display.h"
#include "settings.h"
#include "i18n.h"
#include "statefile_wrapper.h"
#include <eadk.h>
#include <stdio.h>
#include <string.h>

#define WIN_W              240
#define WIN_H              216
#define WIN_X              ((EADK_SCREEN_WIDTH - WIN_W) / 2)
#define WIN_Y              ((EADK_SCREEN_HEIGHT - WIN_H) / 2)

#define COLOR_BORDER       ((eadk_color_t)0x041D)
#define COLOR_BG           ((eadk_color_t)0x10C6)
#define COLOR_HEADER_BG    ((eadk_color_t)0x1927)
#define COLOR_SEL_BG       ((eadk_color_t)0x235F)
#define COLOR_SEL_BAR      ((eadk_color_t)0x067F)

#define COLOR_TEXT_WHITE   ((eadk_color_t)0xFFFF)
#define COLOR_TEXT_SILVER  ((eadk_color_t)0xEF5D)
#define COLOR_TEXT_YELLOW  ((eadk_color_t)0xFFE0)
#define COLOR_TEXT_CYAN    ((eadk_color_t)0x56BF)
#define COLOR_SUCCESS_BG   ((eadk_color_t)0x05E5)
#define COLOR_ALERT_BG     ((eadk_color_t)0xF800)

enum {
  ITEM_RESUME = 0,
  ITEM_SLOT,
  ITEM_SAVE_STATE,
  ITEM_LOAD_STATE,
  ITEM_SPEED,
  ITEM_SCALE,
  ITEM_PALETTE,
  ITEM_FPS,
  ITEM_LANGUAGE,
  ITEM_RESET,
  ITEM_EXIT,
  TOTAL_ITEMS
};

static void wait_for_keys_released(void) {
  while (eadk_keyboard_scan() != 0) {
    eadk_timing_msleep(10);
  }
}

static const char *get_palette_name_localized(PaletteMode mode, bool is_fr) {
  switch (mode) {
    case PALETTE_STANDARD:
      return "NES Standard";
    case PALETTE_SMOOTH:
      return is_fr ? "CRT Doux" : "CRT Smooth";
    case PALETTE_VIVID:
      return is_fr ? "Sony Eclatant" : "Sony Vivid";
    case PALETTE_GAMEBOY:
      return "Game Boy";
    case PALETTE_NOIR_ET_BLANC:
      return is_fr ? "Noir & Blanc" : "Black & White";
    default:
      return "Standard";
  }
}

static void show_toast(const char *msg, bool is_success) {
  uint16_t toast_w = 204;
  uint16_t toast_h = 24;
  uint16_t toast_x = (EADK_SCREEN_WIDTH - toast_w) / 2;
  uint16_t toast_y = WIN_Y + 90;

  eadk_color_t bg = is_success ? COLOR_SUCCESS_BG : COLOR_ALERT_BG;
  eadk_display_push_rect_uniform((eadk_rect_t){toast_x, toast_y, toast_w, toast_h}, bg);
  eadk_display_push_rect_uniform((eadk_rect_t){toast_x, toast_y, toast_w, 1}, COLOR_TEXT_WHITE);
  eadk_display_push_rect_uniform((eadk_rect_t){toast_x, toast_y + toast_h - 1, toast_w, 1}, COLOR_TEXT_WHITE);

  eadk_display_draw_string(msg, (eadk_point_t){toast_x + 8, toast_y + 6}, false, COLOR_TEXT_WHITE, bg);
  eadk_timing_msleep(600);
}

static void cleanup_pause_screen(void) {
  eadk_display_push_rect_uniform((eadk_rect_t){WIN_X, WIN_Y, WIN_W, WIN_H}, (eadk_color_t)0x0000);
  if (display_get_scale_mode() == VIDEO_SCALE_4_3) {
    display_draw_bezels();
  }
}

PauseAction pause_menu_show(const char *game_title, bool *fast_forward) {
  wait_for_keys_released();

  int selected = 0;
  bool local_ff = fast_forward ? *fast_forward : false;
  bool needs_redraw = true;

  eadk_keyboard_state_t prev_state = 0;
  uint32_t repeat_counter = 0;

  while (1) {
    bool is_fr = (i18n_get_language() == LANG_FR);

    if (needs_redraw) {
      eadk_display_push_rect_uniform((eadk_rect_t){WIN_X, WIN_Y, WIN_W, WIN_H}, COLOR_BORDER);
      eadk_display_push_rect_uniform((eadk_rect_t){WIN_X + 2, WIN_Y + 2, WIN_W - 4, WIN_H - 4}, COLOR_BG);
      eadk_display_push_rect_uniform((eadk_rect_t){WIN_X + 2, WIN_Y + 2, WIN_W - 4, 18}, COLOR_HEADER_BG);
      eadk_display_push_rect_uniform((eadk_rect_t){WIN_X + 2, WIN_Y + 20, WIN_W - 4, 1}, COLOR_BORDER);

      char title_buf[32];
      snprintf(title_buf, sizeof(title_buf), "%s: %.10s", is_fr ? "PAUSE" : "PAUSE", game_title ? game_title : "NES");
      eadk_display_draw_string(title_buf, (eadk_point_t){WIN_X + 8, WIN_Y + 4}, false, COLOR_TEXT_YELLOW, COLOR_HEADER_BG);

      const char *lang_badge = is_fr ? "[FR]" : "[EN]";
      eadk_display_draw_string(lang_badge, (eadk_point_t){WIN_X + WIN_W - 38, WIN_Y + 4}, false, COLOR_TEXT_CYAN, COLOR_HEADER_BG);

      int active_slot = ram_state_get_slot();

      for (int i = 0; i < TOTAL_ITEMS; i++) {
        uint16_t row_y = WIN_Y + 23 + (i * 15);
        bool is_sel = (i == selected);
        eadk_color_t row_bg = is_sel ? COLOR_SEL_BG : COLOR_BG;
        eadk_color_t text_col = is_sel ? COLOR_TEXT_WHITE : COLOR_TEXT_SILVER;

        eadk_display_push_rect_uniform((eadk_rect_t){WIN_X + 3, row_y, WIN_W - 6, 14}, row_bg);
        if (is_sel) {
          eadk_display_push_rect_uniform((eadk_rect_t){WIN_X + 3, row_y, 3, 14}, COLOR_SEL_BAR);
        }

        char label[36];
        switch (i) {
          case ITEM_RESUME:
            snprintf(label, sizeof(label), "%s", is_fr ? "Reprendre la partie" : "Resume Game");
            break;
          case ITEM_SLOT:
            snprintf(label, sizeof(label), "%s: < Slot %d >", is_fr ? "Emplacement" : "Save Slot", active_slot + 1);
            break;
          case ITEM_SAVE_STATE:
            snprintf(label, sizeof(label), "%s (Slot %d)", is_fr ? "Sauvegarder" : "Save State", active_slot + 1);
            break;
          case ITEM_LOAD_STATE:
            if (ram_state_exists_slot(active_slot)) {
              snprintf(label, sizeof(label), "%s (Slot %d) [%s]", is_fr ? "Charger" : "Load State", active_slot + 1, is_fr ? "Dispo" : "Ready");
            } else {
              snprintf(label, sizeof(label), "%s (Slot %d) [%s]", is_fr ? "Charger" : "Load State", active_slot + 1, is_fr ? "Vide" : "Empty");
            }
            break;
          case ITEM_SPEED:
            snprintf(label, sizeof(label), "%s: %s",
                     is_fr ? "Vitesse" : "Speed",
                     local_ff ? (is_fr ? "Rapide (2x)" : "Fast (2x)") : (is_fr ? "Normale (1x)" : "Normal (1x)"));
            break;
          case ITEM_SCALE:
            snprintf(label, sizeof(label), "%s: %s",
                     is_fr ? "Format" : "Display",
                     (display_get_scale_mode() == VIDEO_SCALE_FULLSCREEN)
                       ? (is_fr ? "Plein ecran" : "Fullscreen")
                       : "4:3 Standard");
            break;
          case ITEM_PALETTE:
            snprintf(label, sizeof(label), "%s: %s",
                     is_fr ? "Palette" : "Palette",
                     get_palette_name_localized(display_get_palette_mode(), is_fr));
            break;
          case ITEM_FPS:
            snprintf(label, sizeof(label), "%s: %s",
                     is_fr ? "Compteur FPS" : "Show FPS",
                     display_get_show_fps() ? (is_fr ? "OUI" : "ON") : (is_fr ? "NON" : "OFF"));
            break;
          case ITEM_LANGUAGE:
            snprintf(label, sizeof(label), "%s: %s",
                     is_fr ? "Langue" : "Language",
                     is_fr ? "Francais" : "English");
            break;
          case ITEM_RESET:
            snprintf(label, sizeof(label), "%s", is_fr ? "Redemarrer le jeu" : "Reset Game");
            break;
          case ITEM_EXIT:
            snprintf(label, sizeof(label), "%s", is_fr ? "Quitter vers menu" : "Quit to Menu");
            break;
          default:
            snprintf(label, sizeof(label), "Option");
            break;
        }

        char item_str[40];
        snprintf(item_str, sizeof(item_str), "%s %s", is_sel ? ">" : " ", label);
        eadk_display_draw_string(item_str, (eadk_point_t){WIN_X + 6, row_y + 1}, false, text_col, row_bg);
      }

      char hint_buf[36];
      snprintf(hint_buf, sizeof(hint_buf), "%s",
               is_fr ? "[OK] Valider   [Back] Reprendre" : "[OK] Select    [Back] Resume");
      eadk_display_push_rect_uniform((eadk_rect_t){WIN_X + 3, WIN_Y + WIN_H - 17, WIN_W - 6, 15}, COLOR_HEADER_BG);
      eadk_display_draw_string(hint_buf, (eadk_point_t){WIN_X + 10, WIN_Y + WIN_H - 16}, false, COLOR_TEXT_YELLOW, COLOR_HEADER_BG);

      needs_redraw = false;
    }

    eadk_keyboard_state_t cur_state = eadk_keyboard_scan();

    if (eadk_keyboard_key_down(cur_state, eadk_key_toolbox)) {
      wait_for_keys_released();
      if (fast_forward) *fast_forward = local_ff;
      cleanup_pause_screen();
      return PAUSE_ACTION_RESUME;
    }

    bool key_back = eadk_keyboard_key_down(cur_state, eadk_key_back) &&
                    !eadk_keyboard_key_down(prev_state, eadk_key_back);
    if (key_back) {
      wait_for_keys_released();
      if (fast_forward) *fast_forward = local_ff;
      cleanup_pause_screen();
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
      needs_redraw = true;
    } else if (down_pressed) {
      selected = (selected + 1) % TOTAL_ITEMS;
      needs_redraw = true;
    }

    if (key_action || key_left || key_right) {
      switch (selected) {
        case ITEM_RESUME:
          if (key_action) {
            wait_for_keys_released();
            if (fast_forward) *fast_forward = local_ff;
            cleanup_pause_screen();
            return PAUSE_ACTION_RESUME;
          }
          break;

        case ITEM_SLOT: {
          int s = ram_state_get_slot();
          if (key_left) {
            s = (s > 0) ? (s - 1) : (NUM_SAVE_SLOTS - 1);
          } else {
            s = (s + 1) % NUM_SAVE_SLOTS;
          }
          ram_state_set_slot(s);
          needs_redraw = true;
          break;
        }

        case ITEM_SAVE_STATE:
          if (key_action) {
            int ret = ram_state_save();
            char msg[32];
            int cur_slot = ram_state_get_slot() + 1;
            if (ret == 0) {
              snprintf(msg, sizeof(msg), is_fr ? "Sauvegarde Slot %d OK !" : "Slot %d Saved to RAM!", cur_slot);
            } else {
              snprintf(msg, sizeof(msg), is_fr ? "Erreur Slot %d !" : "Slot %d Save Error!", cur_slot);
            }
            show_toast(msg, ret == 0);
            needs_redraw = true;
          }
          break;

        case ITEM_LOAD_STATE:
          if (key_action) {
            int cur_slot = ram_state_get_slot() + 1;
            if (ram_state_exists()) {
              int ret = ram_state_load();
              char msg[32];
              if (ret == 0) {
                snprintf(msg, sizeof(msg), is_fr ? "Restauration Slot %d OK !" : "Slot %d Restored!", cur_slot);
              } else {
                snprintf(msg, sizeof(msg), is_fr ? "Erreur de chargement !" : "Load Error!");
              }
              show_toast(msg, ret == 0);
            } else {
              char msg[32];
              snprintf(msg, sizeof(msg), is_fr ? "Slot %d vide !" : "Slot %d is Empty!", cur_slot);
              show_toast(msg, false);
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
            cleanup_pause_screen();
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
