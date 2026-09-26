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
  bool show_fps;
} AppSettings;

extern AppSettings g_settings;

void settings_init(void);

#endif
