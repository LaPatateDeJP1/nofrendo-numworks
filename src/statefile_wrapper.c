#include "statefile_wrapper.h"
#undef false
#undef true
#undef bool
#include <nesstate.h>
#include <stdlib.h>
#include <string.h>

#define RAM_STATE_MAX_SIZE (24 * 1024)

static uint8_t *s_ram_state_buf = NULL;
static size_t s_ram_state_len = 0;
static int s_ram_state_valid = 0;

typedef struct {
  uint8_t *data;
  size_t pos;
  int isWrite;
} statefile_desc_t;

int ram_state_exists(void) {
  return (s_ram_state_valid && s_ram_state_buf && (s_ram_state_len > 0)) ? 1 : 0;
}

void ram_state_clear(void) {
  s_ram_state_valid = 0;
  s_ram_state_len = 0;
}

int ram_state_save(void) {
  return state_save();
}

int ram_state_load(void) {
  if (!ram_state_exists()) {
    return -1;
  }
  return state_load();
}

FILE * statefile_fopen(const char *pathname, const char *mode) {
  statefile_desc_t *s = (statefile_desc_t *)calloc(sizeof(statefile_desc_t), 1);
  if (!s) return NULL;

  if (mode[0] == 'r') {
    if (!s_ram_state_valid || !s_ram_state_buf || s_ram_state_len == 0) {
      free(s);
      return NULL;
    }
    s->data = s_ram_state_buf;
    s->pos = 0;
    s->isWrite = 0;
  } else if (mode[0] == 'w') {
    if (!s_ram_state_buf) {
      s_ram_state_buf = (uint8_t *)malloc(RAM_STATE_MAX_SIZE);
      if (!s_ram_state_buf) {
        free(s);
        return NULL;
      }
    }
    s_ram_state_len = 0;
    s_ram_state_valid = 0;
    s->data = s_ram_state_buf;
    s->pos = 0;
    s->isWrite = 1;
  } else {
    free(s);
    return NULL;
  }
  return (FILE *)s;
}

int statefile_fclose(FILE *stream) {
  statefile_desc_t *s = (statefile_desc_t *)stream;
  if (!s) return 0;
  if (s->isWrite) {
    if (s_ram_state_len > 0) {
      s_ram_state_valid = 1;
    }
  }
  free(s);
  return 0;
}

size_t statefile_fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  statefile_desc_t *s = (statefile_desc_t *)stream;
  if (!s || size * nmemb == 0) return nmemb;
  size_t bytes = size * nmemb;
  if (s->pos + bytes > s_ram_state_len) {
    bytes = (s_ram_state_len > s->pos) ? (s_ram_state_len - s->pos) : 0;
  }
  if (bytes > 0) {
    memcpy(ptr, s->data + s->pos, bytes);
    s->pos += bytes;
  }
  return (size > 0) ? (bytes / size) : 0;
}

size_t statefile_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
  statefile_desc_t *s = (statefile_desc_t *)stream;
  if (!s || size * nmemb == 0) return nmemb;
  size_t bytes = size * nmemb;
  if (s->pos + bytes > RAM_STATE_MAX_SIZE) {
    return 0;
  }
  memcpy(s->data + s->pos, ptr, bytes);
  s->pos += bytes;
  if (s->pos > s_ram_state_len) {
    s_ram_state_len = s->pos;
  }
  return nmemb;
}

int statefile_fseek(FILE *stream, long offset, int whence) {
  statefile_desc_t *s = (statefile_desc_t *)stream;
  if (!s) return -1;
  if (whence == SEEK_SET) {
    s->pos = (size_t)offset;
  } else if (whence == SEEK_CUR) {
    s->pos = (size_t)((long)s->pos + offset);
  } else if (whence == SEEK_END) {
    s->pos = (size_t)((long)s_ram_state_len + offset);
  }
  return 0;
}
