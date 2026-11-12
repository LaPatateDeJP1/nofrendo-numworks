#include <stddef.h>
#include <sys/stat.h>

extern int errno;
#define ENOMEM_VAL 12

extern char _heap_start;
extern char _heap_end;

static char *s_heap_ptr = NULL;

void *_sbrk(ptrdiff_t incr) {
  if (s_heap_ptr == NULL) {
    s_heap_ptr = &_heap_start;
  }
  char *prev = s_heap_ptr;
  if (prev + incr > &_heap_end || prev + incr < &_heap_start) {
    errno = ENOMEM_VAL;
    return (void *)-1;
  }
  s_heap_ptr += incr;
  return prev;
}

int _close(int file) {
  (void)file;
  return -1;
}

int _fstat(int file, struct stat *st) {
  (void)file;
  if (st) {
    st->st_mode = S_IFCHR;
  }
  return 0;
}

int _isatty(int file) {
  (void)file;
  return 1;
}

int _lseek(int file, int ptr, int dir) {
  (void)file;
  (void)ptr;
  (void)dir;
  return 0;
}

int _open(const char *name, int flags, ...) {
  (void)name;
  (void)flags;
  return -1;
}

int _read(int file, char *ptr, int len) {
  (void)file;
  (void)ptr;
  (void)len;
  return 0;
}

int _write(int file, char *ptr, int len) {
  (void)file;
  (void)ptr;
  return len;
}
