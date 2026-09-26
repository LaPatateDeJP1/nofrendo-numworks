#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>

typedef enum {
  VIDEO_SCALE_4_3 = 0,
  VIDEO_SCALE_FULLSCREEN = 1
} VideoScaleMode;

typedef enum {
  PALETTE_STANDARD = 0,
  PALETTE_SMOOTH = 1,
  PALETTE_VIVID = 2,
  PALETTE_GAMEBOY = 3,
  PALETTE_NOIR_ET_BLANC = 4
} PaletteMode;

void display_set_scale_mode(VideoScaleMode mode);
VideoScaleMode display_get_scale_mode(void);
const char *display_get_scale_mode_name(VideoScaleMode mode);

void display_set_palette_mode(PaletteMode mode);
PaletteMode display_get_palette_mode(void);
const char *display_get_palette_mode_name(PaletteMode mode);

void display_set_show_fps(bool show);
bool display_get_show_fps(void);
int display_get_fps(void);

void display_draw_bezels(void);

#endif
