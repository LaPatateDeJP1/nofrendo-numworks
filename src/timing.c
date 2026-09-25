#include <osd.h>
#include <eadk.h>
#include <stdbool.h>

int timerfreq = 50;
static bool s_fast_forward = false;

void timing_set_fast_forward(bool ff) {
  s_fast_forward = ff;
}

bool timing_get_fast_forward(void) {
  return s_fast_forward;
}

int osd_installtimer(int frequency, void *func, int funcsize, void *counter, int countersize) {
  timerfreq = frequency;
  return 0;
}

int osd_nofrendo_ticks(void) {
  int freq = s_fast_forward ? (timerfreq * 2) : timerfreq;
  if (freq <= 0) freq = 50;
  return eadk_timing_millis() / (1000 / freq);
}
