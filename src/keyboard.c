#include "noftypes.h"
#include <eadk.h>
#undef false
#undef true
#undef bool
#include <osd.h>
#include <event.h>
#include <nesinput.h>
#include "pause_menu.h"
#include "game_entry.h"

extern void timing_set_fast_forward(bool ff);
extern bool timing_get_fast_forward(void);

static bool exitNextIteration = false;
static uint64_t old_keyboard_state = 0;

void osd_queue_loadstate(void) {
  exitNextIteration = false;
  old_keyboard_state = 0;
}

void osd_getinput(void) {
  typedef struct {
    eadk_key_t key;
    int event;
  } key_event_mapping;

  const key_event_mapping key_to_events[] = {
    {eadk_key_left, event_joypad1_left},
    {eadk_key_up, event_joypad1_up},
    {eadk_key_down, event_joypad1_down},
    {eadk_key_right, event_joypad1_right},
    {eadk_key_ok, event_joypad1_b},
    {eadk_key_back, event_joypad1_a},
    {eadk_key_ans, event_joypad1_b},
    {eadk_key_exe, event_joypad1_a},
    {eadk_key_shift, event_joypad1_select},
    {eadk_key_backspace, event_joypad1_start},
    {eadk_event_tangent, event_hard_reset},
  };

  uint64_t current_keyboard_state = eadk_keyboard_scan();

  bool is_shift_down = eadk_keyboard_key_down(current_keyboard_state, eadk_key_shift);
  bool is_back_down = eadk_keyboard_key_down(current_keyboard_state, eadk_key_back);
  bool is_toolbox_down = eadk_keyboard_key_down(current_keyboard_state, eadk_key_toolbox);
  bool is_var_down = eadk_keyboard_key_down(current_keyboard_state, eadk_key_var);

  if (is_toolbox_down || is_var_down || (is_shift_down && is_back_down)) {
    const GameEntry *g = game_get_current();
    bool ff = timing_get_fast_forward();
    PauseAction action = pause_menu_show(g ? g->title : "NES", &ff);
    timing_set_fast_forward(ff);

    old_keyboard_state = 0;
    current_keyboard_state = 0;

    switch (action) {
      case PAUSE_ACTION_RESUME:
        break;
      case PAUSE_ACTION_RESET:
        event_get(event_hard_reset)(INP_STATE_MAKE);
        break;
      case PAUSE_ACTION_QUIT_TO_CATALOG:
        exitNextIteration = true;
        break;
    }
  }

  for (size_t i = 0; i < sizeof(key_to_events) / sizeof(key_to_events[0]); i++) {
    bool wasDown = eadk_keyboard_key_down(old_keyboard_state, key_to_events[i].key);
    bool isDown = eadk_keyboard_key_down(current_keyboard_state, key_to_events[i].key);
    if (isDown != wasDown) {
      event_t evt = event_get(key_to_events[i].event);
      evt(isDown ? INP_STATE_MAKE : INP_STATE_BREAK);
    }
  }

  if (exitNextIteration) {
    exitNextIteration = false;
    old_keyboard_state = 0;
    event_get(event_quit)(INP_STATE_MAKE);
    return;
  }

  old_keyboard_state = current_keyboard_state;
}
