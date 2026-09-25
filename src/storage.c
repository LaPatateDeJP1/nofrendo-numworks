#include <stdbool.h>
#include "storage.h"
#include <stdint.h>
#include <string.h>

inline uint32_t reverse32(uint32_t value) {
  return (((value & 0x000000FF) << 24) |
          ((value & 0x0000FF00) <<  8) |
          ((value & 0x00FF0000) >>  8) |
          ((value & 0xFF000000) >> 24));
}

bool extapp_isValid(const uint32_t * address) {
  if (!address || (uint32_t)address < 0x20000000) {
    return false;
  }
  return *address == reverse32(0xBADD0BEE);
}

const uint8_t extapp_calculatorModel() {
  return 0;
}

const uint32_t extapp_userlandAddress() {
  return 0;
}

uint32_t extapp_address() {
  return 0;
}

const uint32_t extapp_size() {
  return 0;
}

int extapp_fileList(const char ** filename, int maxrecord, const char * extension) {
  return 0;
}

int extapp_fileListWithExtension(const char ** filename, int maxrecord, const char * extension_to_match) {
  return 0;
}

bool extapp_fileExists(const char * filename) {
  return false;
}

const char * extapp_fileRead(const char * filename, size_t * len) {
  if (len) *len = 0;
  return NULL;
}

bool extapp_fileWrite(const char * filename, const char * content, size_t len) {
  return false;
}

bool extapp_fileErase(const char * filename) {
  return false;
}

const uint32_t * extapp_nextFree() {
  return NULL;
}

const uint32_t extapp_used() {
  return 0;
}
