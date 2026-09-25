#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <noftypes.h>
#include <nofconfig.h>
#include <log.h>
#include <osd.h>
#include <nofrendo.h>
#include "game_entry.h"

#include <version.h>

#include <eadk.h>

char configfilename[]="na";

int osd_init() {
  return 0;
}

void osd_shutdown() {
}

typedef uint32_t crc;

#define WIDTH  (8 * sizeof(crc))
#define TOPBIT (1 << (WIDTH - 1))
#define POLYNOMIAL 0x04C11DB7

crc crcSlow(uint8_t const message[], int nBytes) {
    crc  remainder = 0;
    for (int byte = 0; byte < nBytes; ++byte) {
        remainder ^= (message[byte] << (WIDTH - 8));
        for (uint8_t bit = 8; bit > 0; --bit) {
            if (remainder & TOPBIT) {
                remainder = (remainder << 1) ^ POLYNOMIAL;
            } else {
                remainder = (remainder << 1);
            }
        }
    }
    return (remainder);
}

int osd_main(int argc, char *argv[]) {
  config.filename = configfilename;
  const GameEntry *current_game = game_get_current();
  const uint8_t *rom_ptr = (current_game && current_game->rom_data) 
                            ? current_game->rom_data 
                            : (const uint8_t *)"";
  size_t rom_len = (current_game && current_game->rom_data) 
                   ? current_game->rom_size 
                   : 0;
  uint32_t crc = current_game ? current_game->crc32 : 0;
  char crcHex[9];
  sprintf(crcHex, "%08x", (unsigned int)crc);

  return main_loop(crcHex, system_autodetect);
}

void osd_getmouse(int *x, int *y, int *button) {
}

void osd_fullname(char *fullname, const char *shortname)
{
   strncpy(fullname, shortname, PATH_MAX);
}

char *osd_newextension(char *string, char *ext)
{
    int l=strlen(string);
    while(l && string[l]!='.') {
        l--;
    }
    if (l) string[l]=0;
    strcat(string, ext);
    return string;
}

int osd_makesnapname(char *filename, int len)
{
   return -1;
}
