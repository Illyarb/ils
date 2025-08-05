#ifndef LSD_CONFIG_H
#define LSD_CONFIG_H

#include <sys/types.h>

typedef struct {
    char* name;
    char* icon;
} IconEntry;

typedef struct {
    IconEntry* names;
    IconEntry* extensions;
    IconEntry* filetypes;
    int name_count;
    int ext_count;
    int type_count;
} LsdConfig;

extern LsdConfig lsd_config;

void init_lsd_config(void);
void cleanup_lsd_config(void);
const char* get_lsd_icon(const char* filename, mode_t mode);

#endif
