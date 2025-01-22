#ifndef LOGO_H
#define LOGO_H

#include <sys/types.h>

typedef struct {
    char* theme_name;
    char* theme_path;
    char* icon_sizes[64];  // Support up to 64 different icon sizes
    int num_sizes;
    char* directories[64]; // Support up to 64 different directories
    int num_directories;
} ThemeConfig;

char* get_file_logo(const char* filename, mode_t permissions, uid_t owner);

void init_theme(const char* theme_name);

void cleanup_theme(void);

#endif /* LOGO_H */
