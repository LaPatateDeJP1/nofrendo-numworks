#include "statefile_wrapper.h"
#undef false
#undef true
#undef bool
#include <nesstate.h>
#include <stdlib.h>
#include <string.h>

extern int LZ4_compressBound(int isize);
extern int LZ4_compress_default(const char* src, char* dst, int srcSize, int dstCapacity);
extern int LZ4_decompress_safe(const char* src, char* dst, int compressedSize, int dstCapacity);

#define RAM_STATE_MAX_SIZE (18 * 1024)

typedef struct {
  uint8_t *comp_data;
  size_t comp_len;
  size_t raw_len;
  int valid;
} ram_slot_t;

static ram_slot_t s_slots[NUM_SAVE_SLOTS];
static int s_current_slot = 0;

typedef struct {
  uint8_t *work_buf;
  size_t pos;
  size_t len;
  int is_write;
} statefile_desc_t;

int ram_state_get_slot(void) {
  return s_current_slot;
}

void ram_state_set_slot(int slot) {
  if (slot >= 0 && slot < NUM_SAVE_SLOTS) {
    s_current_slot = slot;
  }
}

int ram_state_exists_slot(int slot) {
  if (slot >= 0 && slot < NUM_SAVE_SLOTS) {
    return (s_slots[slot].valid && s_slots[slot].comp_data && s_slots[slot].raw_len > 0) ? 1 : 0;
  }
  return 0;
}

int ram_state_exists(void) {
  return ram_state_exists_slot(s_current_slot);
}

void ram_state_clear_slot(int slot) {
  if (slot >= 0 && slot < NUM_SAVE_SLOTS) {
    if (s_slots[slot].comp_data) {
      free(s_slots[slot].comp_data);
      s_slots[slot].comp_data = NULL;
    }
    s_slots[slot].comp_len = 0;
    s_slots[slot].raw_len = 0;
    s_slots[slot].valid = 0;
  }
}

void ram_state_clear(void) {
  for (int i = 0; i < NUM_SAVE_SLOTS; i++) {
    ram_state_clear_slot(i);
  }
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
    if (!ram_state_exists()) {
      free(s);
      return NULL;
    }
    s->work_buf = (uint8_t *)malloc(s_slots[s_current_slot].raw_len);
    if (!s->work_buf) {
      free(s);
      return NULL;
    }
    int dec_sz = LZ4_decompress_safe(
        (const char *)s_slots[s_current_slot].comp_data,
        (char *)s->work_buf,
        (int)s_slots[s_current_slot].comp_len,
        (int)s_slots[s_current_slot].raw_len
    );
    if (dec_sz <= 0) {
      free(s->work_buf);
      free(s);
      return NULL;
    }
    s->pos = 0;
    s->len = (size_t)dec_sz;
    s->is_write = 0;
  } else if (mode[0] == 'w') {
    s->work_buf = (uint8_t *)malloc(RAM_STATE_MAX_SIZE);
    if (!s->work_buf) {
      free(s);
      return NULL;
    }
    s->pos = 0;
    s->len = 0;
    s->is_write = 1;
  } else {
    free(s);
    return NULL;
  }

  return (FILE *)s;
}

int statefile_fclose(FILE *stream) {
  statefile_desc_t *s = (statefile_desc_t *)stream;
  if (!s) return 0;

  if (s->is_write && s->len > 0) {
    int max_comp = LZ4_compressBound((int)s->len);
    uint8_t *comp = (uint8_t *)malloc(max_comp);
    if (comp) {
      int comp_sz = LZ4_compress_default(
          (const char *)s->work_buf,
          (char *)comp,
          (int)s->len,
          max_comp
      );
      if (comp_sz > 0) {
        if (s_slots[s_current_slot].comp_data) {
          free(s_slots[s_current_slot].comp_data);
        }
        s_slots[s_current_slot].comp_data = comp;
        s_slots[s_current_slot].comp_len = comp_sz;
        s_slots[s_current_slot].raw_len = s->len;
        s_slots[s_current_slot].valid = 1;
      } else {
        free(comp);
      }
    }
  }

  if (s->work_buf) {
    free(s->work_buf);
    s->work_buf = NULL;
  }

  free(s);
  return 0;
}

size_t statefile_fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  statefile_desc_t *s = (statefile_desc_t *)stream;
  if (!s || size * nmemb == 0) return nmemb;
  size_t bytes = size * nmemb;
  if (s->pos + bytes > s->len) {
    bytes = (s->len > s->pos) ? (s->len - s->pos) : 0;
  }
  if (bytes > 0) {
    memcpy(ptr, s->work_buf + s->pos, bytes);
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
  memcpy(s->work_buf + s->pos, ptr, bytes);
  s->pos += bytes;
  if (s->pos > s->len) {
    s->len = s->pos;
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
    s->pos = (size_t)((long)s->len + offset);
  }
  return 0;
}
