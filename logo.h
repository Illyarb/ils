#ifndef LOGO_H
#define LOGO_H

#include <sys/types.h>
#include "config.h"

extern int current_icon_size;

typedef struct {
    char* name;
    char* size;
    char* context;
    char* type;
    int min_size;
    int max_size;
    int threshold;
} IconDirectory;

typedef struct {
    char* theme_name;
    char* theme_path;
    char* comment;
    char* example;
    char** inherits;
    int inherits_count;
    IconDirectory* directories;
    int directory_count;
    bool hidden;
} ThemeConfig;

typedef struct ThemeNode {
    ThemeConfig theme;
    struct ThemeNode* next;
} ThemeNode;

typedef struct {
    const char* extension;
    const char* mimetype;
} ExtensionMapping;

void init_theme(const char* theme_name);
void cleanup_theme(void);
char* get_file_logo(const char* filename, mode_t permissions, uid_t owner);
const char* get_file_extension(const char* filename);
const char* get_mimetype_for_extension(const char* extension);
bool is_image_file(const char* filename);
char* get_thumbnail_path(const char* filename);
bool generate_thumbnail(const char* source_path, const char* thumbnail_path);
#endif
