#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stdint.h>
#include "i18n.h"
#include "display.h"

typedef struct {
  AppLanguage language;
  VideoScaleMode scale_mode;
  PaletteMode palette_mode;
  uint8_t brightness_idx;
  bool show_fps;
} AppSettings;

extern AppSettings g_settings;

void settings_init(void);
void settings_apply_brightness(void);
void settings_cycle_brightness(void);
uint8_t settings_get_brightness_pct(void);

#endif
