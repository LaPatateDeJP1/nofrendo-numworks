#include "settings.h"

AppSettings g_settings = {
  .language = LANG_EN,
  .scale_mode = VIDEO_SCALE_4_3,
  .palette_mode = PALETTE_STANDARD,
  .show_fps = false
};

void settings_init(void) {
  i18n_set_language(g_settings.language);
  display_set_scale_mode(g_settings.scale_mode);
  display_set_palette_mode(g_settings.palette_mode);
}
