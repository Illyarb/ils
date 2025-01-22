#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "logo.h"

#define MAX_PATH_LENGTH 1024
#define ICON_BASE_PATH "/usr/share/icons"

#define ICON_LEMON      "$HOME/.local/share/customLogos/lemon.svg"
#define ICON_SENTIMENTS "$HOME/.local/share/customLogos/sentiments.svg"
#define ICON_MEMORY     "$HOME/.local/share/customLogos/memory.svg"
#define ICON_STATE      "$HOME/.local/share/customLogos/state.svg"
#define ICON_TEST       "$HOME/.local/share/customLogos/test.svg"


typedef struct {
    const char* extension;
    const char* mimetype;
} ExtensionMapping;

static const ExtensionMapping extension_mappings[] = {
    {".txt", "text-plain"},
    {".md", "text-markdown"},
    {".log", "text-x-log"},
    {".c", "text-x-csrc"},
    {".h", "text-x-chdr"},
    {".cpp", "text-x-c++src"},
    {".hpp", "text-x-c++hdr"},
    {".py", "text-x-python"},
    {".js", "text-javascript"},
    {".html", "text-html"},
    {".css", "text-css"},
    {".java", "text-x-java"},
    {".sh", "text-x-script"},
    {".pdf", "application-pdf"},
    {".doc", "application-msword"},
    {".docx", "application-vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".odt", "application-vnd.oasis.opendocument.text"},
    {".jpg", "image-jpeg"},
    {".jpeg", "image-jpeg"},
    {".png", "image-png"},
    {".gif", "image-gif"},
    {".svg", "image-svg+xml"},
    {".zip", "application-zip"},
    {".tar", "application-x-tar"},
    {".gz", "application-gzip"},
    {".7z", "application-x-7z-compressed"},
    {".mp3", "audio-mpeg"},
    {".wav", "audio-wav"},
    {".ogg", "audio-ogg"},
    {".mp4", "video-mp4"},
    {".avi", "video-x-msvideo"},
    {".mkv", "video-x-matroska"},
    {NULL, NULL}
};

static ThemeConfig current_theme;
static char default_file_icon[MAX_PATH_LENGTH];
static char default_directory_icon[MAX_PATH_LENGTH];

static char* trim(char* str) {
    char* end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static void parse_theme_line(char* line) {
    char* key = strtok(line, "=");
    char* value = strtok(NULL, "=");
    if (!key || !value) return;
    
    key = trim(key);
    value = trim(value);
    
    if (strcmp(key, "Directories") == 0) {
        char* dir = strtok(value, ",");
        current_theme.num_directories = 0;
        while (dir && current_theme.num_directories < 64) {
            current_theme.directories[current_theme.num_directories++] = strdup(trim(dir));
            dir = strtok(NULL, ",");
        }
    }
}

static int directory_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static char* find_best_directory(const char* category) {
    char path[MAX_PATH_LENGTH];
    snprintf(path, sizeof(path), "%s/%s/scalable", current_theme.theme_path, category);
    if (directory_exists(path)) {
        return strdup("scalable");
    }
    
    const char* sizes[] = {"64", "48", "32", "24", "16"};
    for (int i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++) {
        snprintf(path, sizeof(path), "%s/%s/%s", current_theme.theme_path, category, sizes[i]);
        if (directory_exists(path)) {
            return strdup(sizes[i]);
        }
    }
    
    for (int i = 0; i < current_theme.num_directories; i++) {
        if (strstr(current_theme.directories[i], category) == current_theme.directories[i]) {
            snprintf(path, sizeof(path), "%s/%s", current_theme.theme_path, current_theme.directories[i]);
            if (directory_exists(path)) {
                char* slash = strchr(current_theme.directories[i], '/');
                if (slash) {
                    *slash = '\0';
                    return strdup(slash + 1);
                }
                return strdup(current_theme.directories[i]);
            }
        }
    }
    
    return NULL;
}

static char* find_icon_with_fzf(const char* category, const char* size, const char* name) {
    char command[MAX_PATH_LENGTH * 2];
    char result[MAX_PATH_LENGTH];
    char full_path[MAX_PATH_LENGTH];
    
    snprintf(full_path, sizeof(full_path), "%s/%s/%s", 
             current_theme.theme_path, category, size);
             
    if (!directory_exists(full_path)) {
        return NULL;
    }
             
    snprintf(command, sizeof(command), 
             "find %s -type f -name '*.svg' | fzf --filter='%s' | head -n1",
             full_path, name);
             
    FILE* pipe = popen(command, "r");
    if (!pipe) return NULL;
    
    if (fgets(result, sizeof(result), pipe) != NULL) {
        result[strcspn(result, "\n")] = 0;
        pclose(pipe);
        return strdup(result);
    }
    
    pclose(pipe);
    return NULL;
}

static const char* get_file_extension(const char* filename) {
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) {
        return NULL;
    }
    return dot;
}

static const char* get_mimetype_for_extension(const char* extension) {
    if (!extension) return NULL;
    
    for (int i = 0; extension_mappings[i].extension != NULL; i++) {
        if (strcasecmp(extension, extension_mappings[i].extension) == 0) {
            return extension_mappings[i].mimetype;
        }
    }
    return NULL;
}

static char* find_mimetype_icon(const char* mimetype) {
    char* mimetype_size = find_best_directory("mimetypes");
    if (!mimetype_size) return NULL;
    
    char* icon_path = find_icon_with_fzf("mimetypes", mimetype_size, mimetype);
    free(mimetype_size);
    
    return icon_path;
}

void init_theme(const char* theme_name) {
    char theme_path[MAX_PATH_LENGTH];
    char line[MAX_PATH_LENGTH];
    
    memset(&current_theme, 0, sizeof(ThemeConfig));
    current_theme.theme_name = strdup(theme_name);
    
    snprintf(theme_path, sizeof(theme_path), "%s/%s", ICON_BASE_PATH, theme_name);
    current_theme.theme_path = strdup(theme_path);
    
    char index_path[MAX_PATH_LENGTH];
    snprintf(index_path, sizeof(index_path), "%s/index.theme", theme_path);
    
    FILE* f = fopen(index_path, "r");
    if (!f) {
        fprintf(stderr, "Could not open theme file: %s\n", index_path);
        return;
    }
    
    while (fgets(line, sizeof(line), f)) {
        parse_theme_line(line);
    }
    
    fclose(f);
    
    char* mimetype_size = find_best_directory("mimetypes");
    char* places_size = find_best_directory("places");
    
    if (mimetype_size) {
        char* file_icon = find_icon_with_fzf("mimetypes", mimetype_size, "text-x-generic");
        if (file_icon) {
            strncpy(default_file_icon, file_icon, MAX_PATH_LENGTH - 1);
            free(file_icon);
        }
        free(mimetype_size);
    }
    
    if (places_size) {
        char* dir_icon = find_icon_with_fzf("places", places_size, "folder");
        if (dir_icon) {
            strncpy(default_directory_icon, dir_icon, MAX_PATH_LENGTH - 1);
            free(dir_icon);
        }
        free(places_size);
    }
}

void cleanup_theme(void) {
    free(current_theme.theme_name);
    free(current_theme.theme_path);
    
    for (int i = 0; i < current_theme.num_directories; i++) {
        free(current_theme.directories[i]);
    }
    
    memset(&current_theme, 0, sizeof(ThemeConfig));
}

char* get_file_logo(const char* filename, mode_t permissions, uid_t owner) {
    if (strcmp(filename, "lemon") == 0) return ICON_LEMON;
    if (strcmp(filename, "sentiments") == 0) return ICON_SENTIMENTS;
    if (strcmp(filename, "memory") == 0) return ICON_MEMORY;
    if (strcmp(filename, "state") == 0) return ICON_STATE;
    if (strcmp(filename, "test") == 0) return ICON_TEST;
    
    if (S_ISDIR(permissions)) {
        return default_directory_icon;
    }
    
    const char* extension = get_file_extension(filename);
    if (extension) {
        const char* mimetype = get_mimetype_for_extension(extension);
        if (mimetype) {
            char* icon_path = find_mimetype_icon(mimetype);
            if (icon_path) {
                static char cached_path[MAX_PATH_LENGTH];
                strncpy(cached_path, icon_path, MAX_PATH_LENGTH - 1);
                free(icon_path);
                return cached_path;
            }
        }
    }
    
    return default_file_icon;
}
