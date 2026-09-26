#include <osd.h>
#include <eadk.h>
#include <stdbool.h>
#include <stdint.h>

int timerfreq = 60;
static bool s_fast_forward = false;
static uint64_t s_last_time_ms = 0;
static uint32_t s_accum_ticks = 0;
static uint32_t s_remainder_us = 0;

void timing_set_fast_forward(bool ff) {
  s_fast_forward = ff;
}

bool timing_get_fast_forward(void) {
  return s_fast_forward;
}

void timing_reset(void) {
  s_last_time_ms = 0;
  s_accum_ticks = 0;
  s_remainder_us = 0;
}

int osd_installtimer(int frequency, void *func, int funcsize, void *counter, int countersize) {
  if (frequency > 0) {
    timerfreq = frequency;
  }
  return 0;
}

int osd_nofrendo_ticks(void) {
  uint64_t now_ms = eadk_timing_millis();
  if (s_last_time_ms == 0) {
    s_last_time_ms = now_ms;
    return (int)s_accum_ticks;
  }

  uint32_t dt_ms = (uint32_t)(now_ms - s_last_time_ms);
  s_last_time_ms = now_ms;

  // Clamp dt to max 50ms (avoid huge catch-up burst after pause menu)
  if (dt_ms > 50) {
    dt_ms = 20;
  }

  // 1x = 1, 2x = 2
  uint32_t speed = s_fast_forward ? 2 : 1;
  uint32_t effective_us = (dt_ms * 1000 * speed) + s_remainder_us;

  // Standard NTSC NES frame period (~16666 us for 60Hz, 20000 us for 50Hz)
  uint32_t period_us = (timerfreq > 0) ? (1000000 / timerfreq) : 16666;
  if (period_us == 0) period_us = 16666;

  uint32_t ticks = effective_us / period_us;
  s_remainder_us = effective_us % period_us;
  s_accum_ticks += ticks;

  return (int)s_accum_ticks;
}
