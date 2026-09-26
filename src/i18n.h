#ifndef I18N_H
#define I18N_H

typedef enum {
  LANG_EN = 0,
  LANG_FR = 1,
  LANG_COUNT
} AppLanguage;

typedef enum {
  STR_APP_TITLE,
  STR_CART_INFO,
  STR_MAPPER,
  STR_SIZE,
  STR_HINT_PLAY,
  STR_HINT_PAGE,
  STR_HINT_EXIT,
  STR_HINT_LANG,
  STR_BATTERY,
  STR_CHARGING,
  STR_PAUSE_TITLE,
  STR_PAUSE_RESUME,
  STR_PAUSE_SAVE_STATE,
  STR_PAUSE_LOAD_STATE,
  STR_PAUSE_SPEED,
  STR_PAUSE_SCALE,
  STR_PAUSE_PALETTE,
  STR_PAUSE_BRIGHTNESS,
  STR_PAUSE_FPS,
  STR_PAUSE_LANGUAGE,
  STR_PAUSE_RESET,
  STR_PAUSE_EXIT,
  STR_STATE_SAVED,
  STR_STATE_LOADED,
  STR_STATE_EMPTY,
  STR_SCALE_4_3,
  STR_SCALE_FULL,
  STR_PAL_ORIGINAL,
  STR_PAL_SMOOTH,
  STR_PAL_VIVID,
  STR_PAL_GAMEBOY,
  STR_PAL_BW,
  STR_ON,
  STR_OFF,
  STR_READY,
  STR_EMPTY,
  STR_COUNT
} StringId;

AppLanguage i18n_get_language(void);
void i18n_set_language(AppLanguage lang);
void i18n_toggle_language(void);
const char *i18n_str(StringId id);
const char *i18n_get_lang_name(AppLanguage lang);

#endif
