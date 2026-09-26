#include "settings.h"
#include <eadk.h>

AppSettings g_settings = {
  .language = LANG_EN,
  .scale_mode = VIDEO_SCALE_4_3,
  .palette_mode = PALETTE_STANDARD,
  .brightness_idx = 3,
  .show_fps = false
};

static const uint8_t s_brightness_values[4] = {64, 128, 192, 255};
static const uint8_t s_brightness_pcts[4]   = {25, 50, 75, 100};

void settings_init(void) {
  i18n_set_language(g_settings.language);
  display_set_scale_mode(g_settings.scale_mode);
  display_set_palette_mode(g_settings.palette_mode);
  settings_apply_brightness();
}

void settings_apply_brightness(void) {
  if (g_settings.brightness_idx > 3) {
    g_settings.brightness_idx = 3;
  }
  eadk_backlight_set_brightness(s_brightness_values[g_settings.brightness_idx]);
}

void settings_cycle_brightness(void) {
  g_settings.brightness_idx = (g_settings.brightness_idx + 1) % 4;
  settings_apply_brightness();
}

uint8_t settings_get_brightness_pct(void) {
  if (g_settings.brightness_idx > 3) {
    g_settings.brightness_idx = 3;
  }
  return s_brightness_pcts[g_settings.brightness_idx];
}
