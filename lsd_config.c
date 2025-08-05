#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include "lsd_config.h"
#include "logo.h"

LsdConfig lsd_config = {0};

static char* trim(char* str) {
    while(isspace(*str)) str++;
    if(!*str) return str;
    char* end = str + strlen(str) - 1;
    while(end > str && isspace(*end)) end--;
    end[1] = '\0';
    return str;
}

static char* unquote(char* str) {
    str = trim(str);
    size_t len = strlen(str);
    if (len >= 2 && ((str[0] == '"' && str[len-1] == '"') || (str[0] == '\'' && str[len-1] == '\''))) {
        str[len-1] = '\0';
        return str + 1;
    }
    return str;
}

static void add_entry(IconEntry** entries, int* count, const char* name, const char* icon) {
    *entries = realloc(*entries, (*count + 1) * sizeof(IconEntry));
    if (!*entries) return;
    
    (*entries)[*count].name = strdup(name);
    (*entries)[*count].icon = strdup(icon);
    (*count)++;
}

static bool load_config(const char* path) {
    FILE* file = fopen(path, "r");
    if (!file) return false;
    
    char line[1024];
    enum { NONE, NAME, EXT, TYPE } section = NONE;
    
    while (fgets(line, sizeof(line), file)) {
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        char* trimmed = trim(line);
        if (!*trimmed || *trimmed == '#') continue;
        
        if (strcmp(trimmed, "name:") == 0) { section = NAME; continue; }
        if (strcmp(trimmed, "extension:") == 0) { section = EXT; continue; }
        if (strcmp(trimmed, "filetype:") == 0) { section = TYPE; continue; }
        
        if (section != NONE) {
            char* colon = strchr(trimmed, ':');
            if (colon) {
                *colon = '\0';
                char* key = unquote(trimmed);
                char* value = unquote(colon + 1);
                
                if (*value) {
                    switch (section) {
                        case NAME: add_entry(&lsd_config.names, &lsd_config.name_count, key, value); break;
                        case EXT: add_entry(&lsd_config.extensions, &lsd_config.ext_count, key, value); break;
                        case TYPE: add_entry(&lsd_config.filetypes, &lsd_config.type_count, key, value); break;
                        default: break;
                    }
                }
            }
        }
    }
    
    fclose(file);
    return true;
}

void init_lsd_config(void) {
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    
    char path[1024];
    snprintf(path, sizeof(path), "%s/.config/lsd/icons.yaml", home);
    load_config(path);
}

void cleanup_lsd_config(void) {
    for (int i = 0; i < lsd_config.name_count; i++) {
        free(lsd_config.names[i].name);
        free(lsd_config.names[i].icon);
    }
    for (int i = 0; i < lsd_config.ext_count; i++) {
        free(lsd_config.extensions[i].name);
        free(lsd_config.extensions[i].icon);
    }
    for (int i = 0; i < lsd_config.type_count; i++) {
        free(lsd_config.filetypes[i].name);
        free(lsd_config.filetypes[i].icon);
    }
    
    free(lsd_config.names);
    free(lsd_config.extensions);
    free(lsd_config.filetypes);
    memset(&lsd_config, 0, sizeof(lsd_config));
}

const char* get_lsd_icon(const char* filename, mode_t mode) {
    for (int i = 0; i < lsd_config.name_count; i++) {
        if (strcmp(lsd_config.names[i].name, filename) == 0) {
            return lsd_config.names[i].icon;
        }
    }
    
    const char* ext = get_file_extension(filename);
    if (ext && *ext == '.') ext++;
    if (ext) {
        for (int i = 0; i < lsd_config.ext_count; i++) {
            if (strcmp(lsd_config.extensions[i].name, ext) == 0) {
                return lsd_config.extensions[i].icon;
            }
        }
    }
    
    const char* type = S_ISDIR(mode) ? "dir" : 
                      (mode & S_IXUSR) ? "executable" : "file";
    
    for (int i = 0; i < lsd_config.type_count; i++) {
        if (strcmp(lsd_config.filetypes[i].name, type) == 0) {
            return lsd_config.filetypes[i].icon;
        }
    }
    
    return NULL;
}
