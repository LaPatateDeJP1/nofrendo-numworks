#include <eadk.h>
#undef false
#undef true
#undef bool
#include <osd.h>
#include <bitmap.h>
#include <nes.h>
#include "display.h"
#include <stdio.h>

#define DEFAULT_WIDTH        256
#define DEFAULT_HEIGHT       NES_VISIBLE_HEIGHT

static int init(int width, int height);
static void shutdown(void);
static int set_mode(int width, int height);
static void set_palette(rgb_t *pal);
static void clear(uint8 color);
static bitmap_t *lock_write(void);
static void free_write(int num_dirties, rect_t *dirty_rects);
static void custom_blit(bitmap_t *bmp, int num_dirties, rect_t *dirty_rects);
static char fb[1];

viddriver_t pkspDriver =
{
   "video",
   init,
   shutdown,
   set_mode,
   set_palette,
   clear,
   lock_write,
   free_write,
   custom_blit,
   false
};

bitmap_t *myBitmap;

static VideoScaleMode s_scale_mode = VIDEO_SCALE_4_3;
static PaletteMode s_palette_mode = PALETTE_STANDARD;

static rgb_t s_raw_palette[256];
static uint16_t myPalette[256];
static bool s_has_raw_palette = false;

static inline uint16_t to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) | (uint16_t)(b >> 3);
}

static inline uint8_t clamp_u8(int v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return (uint8_t)v;
}

static void apply_palette(void) {
  if (!s_has_raw_palette) {
    return;
  }

  for (int i = 0; i < 256; i++) {
    uint8_t r = s_raw_palette[i].r;
    uint8_t g = s_raw_palette[i].g;
    uint8_t b = s_raw_palette[i].b;

    switch (s_palette_mode) {
      case PALETTE_SMOOTH: {
        int nr = (r * 240 + 10) / 256;
        int ng = (g * 245 + 5) / 256;
        int nb = (b * 235 + 15) / 256;
        myPalette[i] = to_rgb565(clamp_u8(nr), clamp_u8(ng), clamp_u8(nb));
        break;
      }
      case PALETTE_VIVID: {
        int nr = 128 + ((int)r - 128) * 115 / 100;
        int ng = 128 + ((int)g - 128) * 115 / 100;
        int nb = 128 + ((int)b - 128) * 120 / 100;
        myPalette[i] = to_rgb565(clamp_u8(nr), clamp_u8(ng), clamp_u8(nb));
        break;
      }
      case PALETTE_GAMEBOY: {
        uint32_t lum = ((uint32_t)r * 77 + (uint32_t)g * 150 + (uint32_t)b * 29) >> 8;
        if (lum < 64) {
          myPalette[i] = 0x09C1;
        } else if (lum < 128) {
          myPalette[i] = 0x3306;
        } else if (lum < 192) {
          myPalette[i] = 0x8D61;
        } else {
          myPalette[i] = 0x9DE1;
        }
        break;
      }
      case PALETTE_NOIR_ET_BLANC: {
        uint8_t lum = ((uint32_t)r * 77 + (uint32_t)g * 150 + (uint32_t)b * 29) >> 8;
        myPalette[i] = to_rgb565(lum, lum, lum);
        break;
      }
      case PALETTE_STANDARD:
      default:
        myPalette[i] = (b >> 3) + ((g >> 2) << 5) + ((r >> 3) << 11);
        break;
    }
  }
}

void display_set_scale_mode(VideoScaleMode mode) {
  s_scale_mode = mode;
  if (s_scale_mode == VIDEO_SCALE_4_3) {
    display_draw_bezels();
  }
}

VideoScaleMode display_get_scale_mode(void) {
  return s_scale_mode;
}

const char *display_get_scale_mode_name(VideoScaleMode mode) {
  switch (mode) {
    case VIDEO_SCALE_4_3:
      return "4:3 (256x240)";
    case VIDEO_SCALE_FULLSCREEN:
      return "Plein ecran (320x240)";
    default:
      return "Inconnu";
  }
}

void display_set_palette_mode(PaletteMode mode) {
  s_palette_mode = mode;
  apply_palette();
}

PaletteMode display_get_palette_mode(void) {
  return s_palette_mode;
}

const char *display_get_palette_mode_name(PaletteMode mode) {
  switch (mode) {
    case PALETTE_STANDARD:
      return "Originale NES";
    case PALETTE_SMOOTH:
      return "Smooth Composite";
    case PALETTE_VIVID:
      return "Arcade Vivid";
    case PALETTE_GAMEBOY:
      return "Game Boy (DMG)";
    case PALETTE_NOIR_ET_BLANC:
      return "Noir & Blanc";
    default:
      return "Standard";
  }
}

void display_draw_bezels(void) {
  if (s_scale_mode == VIDEO_SCALE_4_3) {
    eadk_display_push_rect_uniform((eadk_rect_t){0, 0, 32, EADK_SCREEN_HEIGHT}, (eadk_color_t)0x18C3);
    eadk_display_push_rect_uniform((eadk_rect_t){288, 0, 32, EADK_SCREEN_HEIGHT}, (eadk_color_t)0x18C3);

    eadk_display_push_rect_uniform((eadk_rect_t){31, 0, 1, EADK_SCREEN_HEIGHT}, (eadk_color_t)0x39E7);
    eadk_display_push_rect_uniform((eadk_rect_t){288, 0, 1, EADK_SCREEN_HEIGHT}, (eadk_color_t)0x39E7);

    eadk_display_draw_string("N", (eadk_point_t){12, 105}, false, (eadk_color_t)0x7BEF, (eadk_color_t)0x18C3);
    eadk_display_draw_string("E", (eadk_point_t){12, 117}, false, (eadk_color_t)0x7BEF, (eadk_color_t)0x18C3);
    eadk_display_draw_string("S", (eadk_point_t){12, 129}, false, (eadk_color_t)0x7BEF, (eadk_color_t)0x18C3);

    eadk_display_draw_string("N", (eadk_point_t){300, 105}, false, (eadk_color_t)0x7BEF, (eadk_color_t)0x18C3);
    eadk_display_draw_string("E", (eadk_point_t){300, 117}, false, (eadk_color_t)0x7BEF, (eadk_color_t)0x18C3);
    eadk_display_draw_string("S", (eadk_point_t){300, 129}, false, (eadk_color_t)0x7BEF, (eadk_color_t)0x18C3);
  }
}

void osd_getvideoinfo(vidinfo_t *info) {
   info->default_width = DEFAULT_WIDTH;
   info->default_height = DEFAULT_HEIGHT;
   info->driver = &pkspDriver;
}

static int init(int width, int height) {
  if (s_scale_mode == VIDEO_SCALE_4_3) {
    display_draw_bezels();
  }
  return 0;
}

static void shutdown(void) {
}

static int set_mode(int width, int height) {
  return 0;
}

static void set_palette(rgb_t *pal) {
  for (int i = 0; i < 256; i++) {
    s_raw_palette[i] = pal[i];
  }
  s_has_raw_palette = true;
  apply_palette();
}

void vid_setpalette(rgb_t *pal) {
  set_palette(pal);
}

static void clear(uint8 color) {
}

static bitmap_t *lock_write(void) {
  myBitmap = bmp_createhw((uint8*)fb, DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_WIDTH*2);
  return myBitmap;
}

static void free_write(int num_dirties, rect_t *dirty_rects) {
  bmp_destroy(&myBitmap);
}

static void custom_blit(bitmap_t *bmp, int num_dirties, rect_t *dirty_rects) {
  if (s_scale_mode == VIDEO_SCALE_FULLSCREEN && bmp->width == NES_SCREEN_WIDTH) {
    uint16_t line[320];
    for (int y = 0; y < bmp->height; y++) {
      const uint8_t *src = bmp->line[y];
      for (int i = 0; i < 64; i++) {
        uint16_t c0 = myPalette[src[0]];
        uint16_t c1 = myPalette[src[1]];
        uint16_t c2 = myPalette[src[2]];
        uint16_t c3 = myPalette[src[3]];
        src += 4;
        line[5 * i + 0] = c0;
        line[5 * i + 1] = c1;
        line[5 * i + 2] = c1;
        line[5 * i + 3] = c2;
        line[5 * i + 4] = c3;
      }
      eadk_display_push_rect((eadk_rect_t){0, y, 320, 1}, line);
    }
  } else {
    uint16_t line[bmp->width];
    int xoffset = (EADK_SCREEN_WIDTH - bmp->width) / 2;
    int yoffset = (EADK_SCREEN_HEIGHT - bmp->height) / 2;

    for (int y = 0; y < bmp->height; y++) {
      for (int x = 0; x < bmp->width; x++) {
        line[x] = myPalette[bmp->line[y][x]];
      }
      eadk_display_push_rect((eadk_rect_t){xoffset, y + yoffset, bmp->width, 1}, line);
    }
  }
}

static bool s_show_fps = false;
static int s_current_fps = 60;
static uint32_t s_frame_count = 0;
static uint64_t s_last_fps_time = 0;

void display_set_show_fps(bool show) {
  s_show_fps = show;
}

bool display_get_show_fps(void) {
  return s_show_fps;
}

int display_get_fps(void) {
  return s_current_fps;
}

void ppu_scanline_blit(uint8_t *bmp, int scanline, bool draw_flag) {
  if (!draw_flag || scanline < 0 || scanline >= EADK_SCREEN_HEIGHT) {
    return;
  }
  bmp += 8;

  if (s_scale_mode == VIDEO_SCALE_FULLSCREEN) {
    uint16_t line[320];
    for (int i = 0; i < 64; i++) {
      uint16_t c0 = myPalette[bmp[0]];
      uint16_t c1 = myPalette[bmp[1]];
      uint16_t c2 = myPalette[bmp[2]];
      uint16_t c3 = myPalette[bmp[3]];
      bmp += 4;
      line[5 * i + 0] = c0;
      line[5 * i + 1] = c1;
      line[5 * i + 2] = c1;
      line[5 * i + 3] = c2;
      line[5 * i + 4] = c3;
    }
    eadk_display_push_rect((eadk_rect_t){0, scanline, 320, 1}, line);
  } else {
    uint16_t line[NES_SCREEN_WIDTH];
    const int xoffset = (EADK_SCREEN_WIDTH - NES_SCREEN_WIDTH) / 2;
    const int yoffset = (EADK_SCREEN_HEIGHT - NES_SCREEN_HEIGHT) / 2;
    for (int x = 0; x < NES_SCREEN_WIDTH; x++) {
      line[x] = myPalette[*bmp++];
    }
    eadk_display_push_rect((eadk_rect_t){xoffset, scanline + yoffset, NES_SCREEN_WIDTH, 1}, line);
  }

  if (scanline == (NES_SCREEN_HEIGHT - 1)) {
    s_frame_count++;
    uint64_t now = eadk_timing_millis();
    if (s_last_fps_time == 0) {
      s_last_fps_time = now;
    } else if (now - s_last_fps_time >= 1000) {
      s_current_fps = (int)(s_frame_count * 1000 / (now - s_last_fps_time));
      s_frame_count = 0;
      s_last_fps_time = now;
    }
    if (s_show_fps) {
      char buf[12];
      sprintf(buf, "%d FPS", s_current_fps);
      eadk_display_draw_string(buf, (eadk_point_t){4, 4}, false, eadk_color_white, eadk_color_black);
    }
  }
}
