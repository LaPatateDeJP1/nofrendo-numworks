#ifndef STATEFILE_WRAPPER_H
#define STATEFILE_WRAPPER_H

#include <stdio.h>

FILE * statefile_fopen(const char *pathname, const char *mode);
int statefile_fclose(FILE *stream);
size_t statefile_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t statefile_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int statefile_fseek(FILE *stream, long offset, int whence);

int ram_state_exists(void);
void ram_state_clear(void);
int ram_state_save(void);
int ram_state_load(void);

#endif
