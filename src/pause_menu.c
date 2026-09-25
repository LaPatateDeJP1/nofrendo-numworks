#include "pause_menu.h"
#include "display.h"
#include <eadk.h>
#include <stdio.h>
#include <string.h>

#define WIN_W              260
#define WIN_H              160
#define WIN_X              ((EADK_SCREEN_WIDTH - WIN_W) / 2)
#define WIN_Y              ((EADK_SCREEN_HEIGHT - WIN_H) / 2)

#define COLOR_BORDER       ((eadk_color_t)0x04FF)
#define COLOR_BG           ((eadk_color_t)0x10A2)
#define COLOR_HEADER_BG    ((eadk_color_t)0x1927)
#define COLOR_SEL_BG       ((eadk_color_t)0x2378)
#define COLOR_MUTED        ((eadk_color_t)0x9CD3)

#define NUM_ITEMS          6

static void wait_for_keys_released(void) {
  while (eadk_keyboard_scan() != 0) {
    eadk_timing_msleep(10);
  }
}

PauseAction pause_menu_show(const char *game_title, bool *fast_forward) {
  wait_for_keys_released();

  size_t selected = 0;
  bool local_ff = fast_forward ? *fast_forward : false;
  bool needs_redraw = true;

  eadk_keyboard_state_t prev_state = 0;
  uint32_t repeat_counter = 0;

  while (1) {
    if (needs_redraw) {
      eadk_display_push_rect_uniform(
          (eadk_rect_t){WIN_X, WIN_Y, WIN_W, WIN_H},
          COLOR_BORDER
      );
      eadk_display_push_rect_uniform(
          (eadk_rect_t){WIN_X + 2, WIN_Y + 2, WIN_W - 4, WIN_H - 4},
          COLOR_BG
      );

      eadk_display_push_rect_uniform(
          (eadk_rect_t){WIN_X + 2, WIN_Y + 2, WIN_W - 4, 22},
          COLOR_HEADER_BG
      );

      char title_buf[32];
      snprintf(title_buf, sizeof(title_buf), "PAUSE : %.18s", game_title ? game_title : "NES");
      eadk_display_draw_string(
          title_buf,
          (eadk_point_t){WIN_X + 10, WIN_Y + 6},
          false,
          eadk_color_white,
          COLOR_HEADER_BG
      );

      char ff_label[32];
      snprintf(ff_label, sizeof(ff_label), "Vitesse : %s", local_ff ? "Rapide (2x)" : "Normale (1x)");

      char format_label[32];
      VideoScaleMode scale_mode = display_get_scale_mode();
      snprintf(format_label, sizeof(format_label), "Format : %s",
               (scale_mode == VIDEO_SCALE_FULLSCREEN) ? "Plein ecran (320)" : "4:3 Original (256)");

      char pal_label[32];
      PaletteMode pal_mode = display_get_palette_mode();
      snprintf(pal_label, sizeof(pal_label), "Palette : %s", display_get_palette_mode_name(pal_mode));

      const char *labels[NUM_ITEMS] = {
        "Reprendre la partie",
        ff_label,
        format_label,
        pal_label,
        "Redemarrer le jeu",
        "Quitter vers catalogue"
      };

      for (size_t i = 0; i < NUM_ITEMS; i++) {
        uint16_t row_y = WIN_Y + 28 + (i * 18);
        bool is_sel = (i == selected);
        eadk_color_t row_bg = is_sel ? COLOR_SEL_BG : COLOR_BG;
        eadk_color_t text_col = is_sel ? eadk_color_white : ((eadk_color_t)0xD6BA);

        eadk_display_push_rect_uniform(
            (eadk_rect_t){WIN_X + 4, row_y, WIN_W - 8, 17},
            row_bg
        );

        char item_str[40];
        snprintf(item_str, sizeof(item_str), "%s %s", is_sel ? ">" : " ", labels[i]);
        eadk_display_draw_string(
            item_str,
            (eadk_point_t){WIN_X + 8, row_y + 2},
            false,
            text_col,
            row_bg
        );
      }

      eadk_display_draw_string(
          "[OK] Valider/Basculer  [Toolbox] Retour",
          (eadk_point_t){WIN_X + 10, WIN_Y + WIN_H - 16},
          false,
          COLOR_MUTED,
          COLOR_BG
      );

      needs_redraw = false;
    }

    eadk_keyboard_state_t cur_state = eadk_keyboard_scan();

    if (eadk_keyboard_key_down(cur_state, eadk_key_toolbox) ||
        eadk_keyboard_key_down(cur_state, eadk_key_back)) {
      wait_for_keys_released();
      if (fast_forward) *fast_forward = local_ff;
      if (display_get_scale_mode() == VIDEO_SCALE_4_3) {
        display_draw_bezels();
      }
      return PAUSE_ACTION_RESUME;
    }

    if (eadk_keyboard_key_down(cur_state, eadk_key_ok) ||
        eadk_keyboard_key_down(cur_state, eadk_key_exe)) {
      wait_for_keys_released();
      if (fast_forward) *fast_forward = local_ff;

      switch (selected) {
        case 0:
          if (display_get_scale_mode() == VIDEO_SCALE_4_3) {
            display_draw_bezels();
          }
          return PAUSE_ACTION_RESUME;

        case 1:
          local_ff = !local_ff;
          if (fast_forward) *fast_forward = local_ff;
          needs_redraw = true;
          break;

        case 2:
          {
            VideoScaleMode cur = display_get_scale_mode();
            display_set_scale_mode((cur == VIDEO_SCALE_FULLSCREEN) ? VIDEO_SCALE_4_3 : VIDEO_SCALE_FULLSCREEN);
            needs_redraw = true;
          }
          break;

        case 3:
          {
            PaletteMode cur_pal = display_get_palette_mode();
            display_set_palette_mode((PaletteMode)((cur_pal + 1) % 5));
            needs_redraw = true;
          }
          break;

        case 4:
          if (display_get_scale_mode() == VIDEO_SCALE_4_3) {
            display_draw_bezels();
          }
          return PAUSE_ACTION_RESET;

        case 5:
          return PAUSE_ACTION_QUIT_TO_CATALOG;
      }
    }

    bool key_up = eadk_keyboard_key_down(cur_state, eadk_key_up);
    bool key_down = eadk_keyboard_key_down(cur_state, eadk_key_down);

    bool trigger_up = false;
    bool trigger_down = false;

    if (key_up || key_down) {
      if (prev_state == 0) {
        trigger_up = key_up;
        trigger_down = key_down;
        repeat_counter = 0;
      } else {
        repeat_counter++;
        if (repeat_counter >= 12 && (repeat_counter % 4) == 0) {
          trigger_up = key_up;
          trigger_down = key_down;
        }
      }
    } else {
      repeat_counter = 0;
    }

    if (trigger_up) {
      selected = (selected > 0) ? (selected - 1) : (NUM_ITEMS - 1);
      needs_redraw = true;
    } else if (trigger_down) {
      selected = (selected + 1 < NUM_ITEMS) ? (selected + 1) : 0;
      needs_redraw = true;
    }

    prev_state = cur_state;
    eadk_timing_msleep(20);
  }
}
