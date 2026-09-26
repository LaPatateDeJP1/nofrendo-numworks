#include "i18n.h"

static AppLanguage s_current_lang = LANG_EN;

static const char *s_strings[LANG_COUNT][STR_COUNT] = {
  [LANG_EN] = {
    [STR_APP_TITLE]        = "NES LOADER",
    [STR_CART_INFO]        = "Cartridge:",
    [STR_MAPPER]           = "Mapper",
    [STR_SIZE]             = "Size",
    [STR_HINT_PLAY]        = "OK: Play",
    [STR_HINT_PAGE]        = "< >: Page",
    [STR_HINT_EXIT]        = "Home: Exit",
    [STR_HINT_LANG]        = "Shift: Lang",
    [STR_BATTERY]          = "BAT",
    [STR_CHARGING]         = "CHG",
    [STR_PAUSE_TITLE]      = "PAUSE MENU",
    [STR_PAUSE_RESUME]     = "Resume Game",
    [STR_PAUSE_SAVE_STATE] = "Save State (RAM)",
    [STR_PAUSE_LOAD_STATE] = "Load State (RAM)",
    [STR_PAUSE_SPEED]      = "Speed",
    [STR_PAUSE_SCALE]      = "Display",
    [STR_PAUSE_PALETTE]    = "Palette",
    [STR_PAUSE_BRIGHTNESS] = "Brightness",
    [STR_PAUSE_FPS]        = "Show FPS",
    [STR_PAUSE_LANGUAGE]   = "Language",
    [STR_PAUSE_RESET]      = "Reset Game",
    [STR_PAUSE_EXIT]       = "Exit to Catalog",
    [STR_STATE_SAVED]      = "State Saved to RAM!",
    [STR_STATE_LOADED]     = "State Restored!",
    [STR_STATE_EMPTY]      = "No State in RAM!",
    [STR_SCALE_4_3]        = "4:3 Original",
    [STR_SCALE_FULL]       = "5:4 Fullscreen",
    [STR_PAL_ORIGINAL]     = "NES Standard",
    [STR_PAL_SMOOTH]       = "CRT Smooth",
    [STR_PAL_VIVID]        = "Sony Vivid",
    [STR_PAL_GAMEBOY]      = "Game Boy DMG",
    [STR_PAL_BW]           = "Black & White",
    [STR_ON]               = "ON",
    [STR_OFF]              = "OFF",
    [STR_READY]            = "Ready",
    [STR_EMPTY]            = "Empty"
  },
  [LANG_FR] = {
    [STR_APP_TITLE]        = "SELECTEUR NES",
    [STR_CART_INFO]        = "Cartouche :",
    [STR_MAPPER]           = "Mapper",
    [STR_SIZE]             = "Taille",
    [STR_HINT_PLAY]        = "OK: Jouer",
    [STR_HINT_PAGE]        = "< >: Page",
    [STR_HINT_EXIT]        = "Home: Quitter",
    [STR_HINT_LANG]        = "Shift: Langue",
    [STR_BATTERY]          = "BAT",
    [STR_CHARGING]         = "CHG",
    [STR_PAUSE_TITLE]      = "MENU PAUSE",
    [STR_PAUSE_RESUME]     = "Reprendre",
    [STR_PAUSE_SAVE_STATE] = "Sauvegarder (RAM)",
    [STR_PAUSE_LOAD_STATE] = "Charger (RAM)",
    [STR_PAUSE_SPEED]      = "Vitesse",
    [STR_PAUSE_SCALE]      = "Affichage",
    [STR_PAUSE_PALETTE]    = "Palette",
    [STR_PAUSE_BRIGHTNESS] = "Luminosite",
    [STR_PAUSE_FPS]        = "Afficher FPS",
    [STR_PAUSE_LANGUAGE]   = "Langue",
    [STR_PAUSE_RESET]      = "Reinitialiser",
    [STR_PAUSE_EXIT]       = "Quitter le jeu",
    [STR_STATE_SAVED]      = "Partie sauvegardee !",
    [STR_STATE_LOADED]     = "Partie restauree !",
    [STR_STATE_EMPTY]      = "Aucune sauvegarde !",
    [STR_SCALE_4_3]        = "4:3 Original",
    [STR_SCALE_FULL]       = "5:4 Plein ecran",
    [STR_PAL_ORIGINAL]     = "NES Standard",
    [STR_PAL_SMOOTH]       = "CRT Doux",
    [STR_PAL_VIVID]        = "Sony Eclatant",
    [STR_PAL_GAMEBOY]      = "Game Boy DMG",
    [STR_PAL_BW]           = "Noir & Blanc",
    [STR_ON]               = "OUI",
    [STR_OFF]              = "NON",
    [STR_READY]            = "Prete",
    [STR_EMPTY]            = "Vide"
  }
};

AppLanguage i18n_get_language(void) {
  return s_current_lang;
}

void i18n_set_language(AppLanguage lang) {
  if (lang < LANG_COUNT) {
    s_current_lang = lang;
  }
}

void i18n_toggle_language(void) {
  s_current_lang = (s_current_lang == LANG_EN) ? LANG_FR : LANG_EN;
}

const char *i18n_str(StringId id) {
  if (id < STR_COUNT) {
    return s_strings[s_current_lang][id];
  }
  return "";
}

const char *i18n_get_lang_name(AppLanguage lang) {
  switch (lang) {
    case LANG_EN: return "English";
    case LANG_FR: return "Francais";
    default: return "Unknown";
  }
}
