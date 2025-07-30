#define _GNU_SOURCE
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <limits.h>
#include <dirent.h>
#include <pwd.h>
#include "logo.h"

static ThemeNode* theme_chain = NULL;
static char default_file_icon[MAX_PATH_LENGTH];
static char default_directory_icon[MAX_PATH_LENGTH];

// Cache for discovered icons to avoid repeated filesystem searches
typedef struct IconCacheEntry {
    char* name;
    char* path;
    int size;
    char* context;
    struct IconCacheEntry* next;
} IconCacheEntry;

static IconCacheEntry* icon_cache = NULL;

static const ExtensionMapping extension_mappings[] = {
    {".py", "text/x-python"},
    {".js", "text/javascript"},
    {".ts", "text/x-typescript"},
    {".c", "text/x-csrc"},
    {".cpp", "text/x-c++src"},
    {".cxx", "text/x-c++src"},
    {".cc", "text/x-c++src"},
    {".h", "text/x-chdr"},
    {".hpp", "text/x-c++hdr"},
    {".java", "text/x-java"},
    {".php", "text/x-php"},
    {".rb", "text/x-ruby"},
    {".go", "text/x-go"},
    {".rs", "text/x-rust"},
    {".sh", "application/x-shellscript"},
    {".bash", "application/x-shellscript"},
    
    {".html", "text/html"},
    {".htm", "text/html"},
    {".css", "text/css"},
    {".xml", "text/xml"},
    {".json", "application/json"},
    {".yaml", "text/x-yaml"},
    {".yml", "text/x-yaml"},
    
    {".pdf", "application/pdf"},
    {".doc", "application/msword"},
    {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".odt", "application/vnd.oasis.opendocument.text"},
    {".xls", "application/vnd.ms-excel"},
    {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".ppt", "application/vnd.ms-powerpoint"},
    {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    
    {".txt", "text/plain"},
    {".md", "text/x-markdown"},
    
    {".zip", "application/zip"},
    {".tar", "application/x-tar"},
    {".gz", "application/x-gzip"},
    {".7z", "application/x-7z-compressed"},
    {".rar", "application/x-rar"},
    {".bz2", "application/x-bzip2"},
    
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
    {".bmp", "image/bmp"},
    {".tiff", "image/tiff"},
    {".tif", "image/tiff"},
    
    {".mp3", "audio/mpeg"},
    {".wav", "audio/wav"},
    {".ogg", "audio/ogg"},
    {".flac", "audio/flac"},
    
    {".mp4", "video/mp4"},
    {".mkv", "video/x-matroska"},
    {".avi", "video/x-msvideo"},
    {".mov", "video/quicktime"},
    {".webm", "video/webm"},
    
    {".iso", "application/x-iso9660-image"},
    {".deb", "application/x-deb"},
    {".rpm", "application/x-rpm"},
    {".exe", "application/x-ms-dos-executable"},
    {".dmg", "application/x-apple-diskimage"},
    
    {NULL, NULL}
};

// Extended search paths for icon themes
static const char* get_icon_search_paths(void) {
    static char search_paths[4096];
    static bool initialized = false;
    
    if (!initialized) {
        const char* home = getenv("HOME");
        if (!home) {
            struct passwd *pw = getpwuid(getuid());
            home = pw ? pw->pw_dir : "/tmp";
        }
        
        snprintf(search_paths, sizeof(search_paths), 
                "%s/.local/share/icons:%s/.icons:/usr/local/share/icons:/usr/share/icons",
                home, home);
        initialized = true;
    }
    
    return search_paths;
}

static char* trim(char* str) {
    char* end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static bool directory_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static void add_to_cache(const char* name, const char* path, int size, const char* context) {
    IconCacheEntry* entry = malloc(sizeof(IconCacheEntry));
    if (!entry) return;
    
    entry->name = strdup(name);
    entry->path = strdup(path);
    entry->size = size;
    entry->context = context ? strdup(context) : NULL;
    entry->next = icon_cache;
    icon_cache = entry;
}

static char* find_in_cache(const char* name, int preferred_size, const char* preferred_context) {
    IconCacheEntry* entry = icon_cache;
    IconCacheEntry* best_match = NULL;
    int best_size_diff = INT_MAX;
    
    while (entry) {
        if (entry->name && strcmp(entry->name, name) == 0) {
            // Prefer exact context match
            if (preferred_context && entry->context && strcmp(entry->context, preferred_context) == 0) {
                int size_diff = abs(entry->size - preferred_size);
                if (size_diff < best_size_diff) {
                    best_match = entry;
                    best_size_diff = size_diff;
                }
            } else if (!preferred_context || !best_match) {
                int size_diff = abs(entry->size - preferred_size);
                if (size_diff < best_size_diff) {
                    best_match = entry;
                    best_size_diff = size_diff;
                }
            }
        }
        entry = entry->next;
    }
    
    return best_match ? strdup(best_match->path) : NULL;
}

static void cleanup_cache(void) {
    IconCacheEntry* entry = icon_cache;
    while (entry) {
        IconCacheEntry* next = entry->next;
        free(entry->name);
        free(entry->path);
        free(entry->context);
        free(entry);
        entry = next;
    }
    icon_cache = NULL;
}

// Scan a directory and add all icons to cache
static void scan_icon_directory(const char* dir_path, const char* context, int size) {
    DIR* dir = opendir(dir_path);
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG && entry->d_type != DT_LNK) continue;
        
        char* dot = strrchr(entry->d_name, '.');
        if (!dot) continue;
        
        // Check if it's an icon file
        char* ext = dot + 1;
        if (strcasecmp(ext, "png") != 0 && strcasecmp(ext, "svg") != 0 && strcasecmp(ext, "xpm") != 0) {
            continue;
        }
        
        size_t name_len = dot - entry->d_name;
        char* icon_name = malloc(name_len + 1);
        if (!icon_name) continue;
        
        strncpy(icon_name, entry->d_name, name_len);
        icon_name[name_len] = '\0';
        
        char full_path[MAX_PATH_LENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        add_to_cache(icon_name, full_path, size, context);
        free(icon_name);
    }
    
    closedir(dir);
}

static bool theme_already_loaded(const char* theme_name) {
    ThemeNode* current = theme_chain;
    while (current) {
        if (current->theme.theme_name && strcmp(current->theme.theme_name, theme_name) == 0) {
            return true;
        }
        current = current->next;
    }
    return false;
}

static ThemeNode* create_theme_node(void) {
    ThemeNode* node = malloc(sizeof(ThemeNode));
    if (!node) return NULL;
    
    memset(&node->theme, 0, sizeof(ThemeConfig));
    node->next = NULL;
    return node;
}

static void add_theme_to_chain(ThemeNode* node) {
    if (!theme_chain) {
        theme_chain = node;
    } else {
        ThemeNode* current = theme_chain;
        while (current->next) {
            current = current->next;
        }
        current->next = node;
    }
}

static void cleanup_theme_config(ThemeConfig* config) {
    free(config->theme_name);
    free(config->theme_path);
    free(config->comment);
    free(config->example);
    
    for (int i = 0; i < config->inherits_count; i++) {
        free(config->inherits[i]);
    }
    free(config->inherits);
    
    for (int i = 0; i < config->directory_count; i++) {
        IconDirectory* dir = &config->directories[i];
        free(dir->name);
        free(dir->size);
        free(dir->context);
        free(dir->type);
    }
    free(config->directories);
    
    memset(config, 0, sizeof(ThemeConfig));
}

static void parse_icon_theme_section(FILE* file, const char* section_name, ThemeConfig* theme) {
    char line[1024];
    IconDirectory* dir = NULL;
    
    if (strcmp(section_name, "Icon Theme") == 0) {
        while (fgets(line, sizeof(line), file)) {
            if (line[0] == '[') {
                fseek(file, -(long)strlen(line), SEEK_CUR);
                break;
            }
            
            char* equals = strchr(line, '=');
            if (!equals) continue;
            
            *equals = '\0';
            char* key = trim(line);
            char* value = trim(equals + 1);
            
            if (strcmp(key, "Name") == 0) {
                free(theme->theme_name);
                theme->theme_name = strdup(value);
            } else if (strcmp(key, "Comment") == 0) {
                free(theme->comment);
                theme->comment = strdup(value);
            } else if (strcmp(key, "Example") == 0) {
                free(theme->example);
                theme->example = strdup(value);
            } else if (strcmp(key, "Hidden") == 0) {
                theme->hidden = (strcmp(value, "true") == 0);
            } else if (strcmp(key, "Inherits") == 0) {
                char* value_copy = strdup(value);
                char* token = strtok(value_copy, ",");
                theme->inherits_count = 0;
                
                while (token && theme->inherits_count < 16) {
                    theme->inherits = realloc(theme->inherits, 
                        (theme->inherits_count + 1) * sizeof(char*));
                    if (!theme->inherits) break;
                    theme->inherits[theme->inherits_count] = strdup(trim(token));
                    theme->inherits_count++;
                    token = strtok(NULL, ",");
                }
                free(value_copy);
            } else if (strcmp(key, "Directories") == 0) {
                char* value_copy = strdup(value);
                char* token = strtok(value_copy, ",");
                theme->directory_count = 0;
                
                while (token) {
                    theme->directories = realloc(theme->directories,
                        (theme->directory_count + 1) * sizeof(IconDirectory));
                    if (!theme->directories) break;
                    
                    token = trim(token);
                    IconDirectory* new_dir = &theme->directories[theme->directory_count];
                    memset(new_dir, 0, sizeof(IconDirectory));
                    new_dir->name = strdup(token);
                    new_dir->type = strdup("Threshold");
                    new_dir->threshold = 2;
                    new_dir->size = strdup("48");
                    theme->directory_count++;
                    
                    token = strtok(NULL, ",");
                }
                free(value_copy);
            }
        }
    } else {
        for (int i = 0; i < theme->directory_count; i++) {
            if (strcmp(theme->directories[i].name, section_name) == 0) {
                dir = &theme->directories[i];
                break;
            }
        }
        
        if (!dir) return;
        
        while (fgets(line, sizeof(line), file)) {
            if (line[0] == '[') {
                fseek(file, -(long)strlen(line), SEEK_CUR);
                break;
            }
            
            char* equals = strchr(line, '=');
            if (!equals) continue;
            
            *equals = '\0';
            char* key = trim(line);
            char* value = trim(equals + 1);
            
            if (strcmp(key, "Size") == 0) {
                free(dir->size);
                dir->size = strdup(value);
            } else if (strcmp(key, "Context") == 0) {
                free(dir->context);
                dir->context = strdup(value);
            } else if (strcmp(key, "Type") == 0) {
                free(dir->type);
                dir->type = strdup(value);
            } else if (strcmp(key, "MinSize") == 0) {
                dir->min_size = atoi(value);
            } else if (strcmp(key, "MaxSize") == 0) {
                dir->max_size = atoi(value);
            } else if (strcmp(key, "Threshold") == 0) {
                dir->threshold = atoi(value);
            }
        }
        
        if (!dir->size) {
            dir->size = strdup("48");
        }
        if (!dir->type) {
            dir->type = strdup("Threshold");
        }
        if (strcmp(dir->type, "Scalable") == 0) {
            if (dir->min_size == 0) dir->min_size = 1;
            if (dir->max_size == 0) dir->max_size = 256;
        }
        if (strcmp(dir->type, "Threshold") == 0 && dir->threshold == 0) {
            dir->threshold = 2;
        }
    }
}

static bool parse_index_theme(const char* theme_path, ThemeConfig* theme) {
    char index_path[MAX_PATH_LENGTH];
    snprintf(index_path, sizeof(index_path), "%s/index.theme", theme_path);
    
    FILE* file = fopen(index_path, "r");
    if (!file) {
        return false;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        if (line[0] == '[' && line[strlen(line) - 1] == ']') {
            line[strlen(line) - 1] = '\0';
            char* section_name = line + 1;
            parse_icon_theme_section(file, section_name, theme);
        }
    }
    
    fclose(file);
    return true;
}

static char* find_theme_path(const char* theme_name) {
    const char* search_paths = get_icon_search_paths();
    char* paths_copy = strdup(search_paths);
    if (!paths_copy) return NULL;
    
    char* path = strtok(paths_copy, ":");
    while (path) {
        char theme_path[MAX_PATH_LENGTH];
        snprintf(theme_path, sizeof(theme_path), "%s/%s", path, theme_name);
        
        if (directory_exists(theme_path)) {
            char* result = strdup(theme_path);
            free(paths_copy);
            return result;
        }
        path = strtok(NULL, ":");
    }
    
    free(paths_copy);
    return NULL;
}

static void scan_theme_icons(const ThemeConfig* theme) {
    if (!theme || !theme->theme_path) return;
    
    for (int i = 0; i < theme->directory_count; i++) {
        const IconDirectory* dir = &theme->directories[i];
        
        char dir_path[MAX_PATH_LENGTH];
        snprintf(dir_path, sizeof(dir_path), "%s/%s", theme->theme_path, dir->name);
        
        if (directory_exists(dir_path)) {
            int size = dir->size ? atoi(dir->size) : 48;
            scan_icon_directory(dir_path, dir->context, size);
        }
    }
}

static bool load_theme(const char* theme_name) {
    if (theme_already_loaded(theme_name)) {
        return true;
    }
    
    char* theme_path = find_theme_path(theme_name);
    if (!theme_path) {
        return false;
    }
    
    ThemeNode* node = create_theme_node();
    if (!node) {
        free(theme_path);
        return false;
    }
    
    node->theme.theme_path = theme_path;
    
    if (!parse_index_theme(theme_path, &node->theme)) {
        cleanup_theme_config(&node->theme);
        free(node);
        return false;
    }
    
    add_theme_to_chain(node);
    
    scan_theme_icons(&node->theme);
    
    for (int i = 0; i < node->theme.inherits_count; i++) {
        load_theme(node->theme.inherits[i]);
    }
    
    return true;
}

static char* mimetype_to_icon_name(const char* mimetype) {
    if (!mimetype) return NULL;
    
    char* icon_name = strdup(mimetype);
    if (!icon_name) return NULL;
    
    for (int i = 0; icon_name[i]; i++) {
        if (icon_name[i] == '/') {
            icon_name[i] = '-';
        }
    }
    
    return icon_name;
}

const char* get_mimetype_for_extension(const char* extension) {
    if (!extension) return NULL;
    
    for (int i = 0; extension_mappings[i].extension; i++) {
        if (strcasecmp(extension, extension_mappings[i].extension) == 0) {
            return extension_mappings[i].mimetype;
        }
    }
    
    return NULL;
}

static char* detect_mimetype_with_file(const char* filename) {
    char cmd[MAX_PATH_LENGTH + 50];
    snprintf(cmd, sizeof(cmd), "file --mime-type -b \"%s\" 2>/dev/null", filename);
    
    FILE* fp = popen(cmd, "r");
    if (!fp) return NULL;
    
    char* mimetype = malloc(256);
    if (!mimetype) {
        pclose(fp);
        return NULL;
    }
    
    if (fgets(mimetype, 256, fp)) {
        char* newline = strchr(mimetype, '\n');
        if (newline) *newline = '\0';
        
        // Trim whitespace
        char* trimmed = trim(mimetype);
        if (strlen(trimmed) > 0) {
            char* result = strdup(trimmed);
            free(mimetype);
            pclose(fp);
            return result;
        }
    }
    
    free(mimetype);
    pclose(fp);
    return NULL;
}

// Fuzzy search fallback using fzf
static char* fuzzy_search_icon(const char* query, int size) {
    if (!query) return NULL;
    
    char temp_file[] = "/tmp/icon_list_XXXXXX";
    int fd = mkstemp(temp_file);
    if (fd == -1) return NULL;
    
    FILE* fp = fdopen(fd, "w");
    if (!fp) {
        close(fd);
        unlink(temp_file);
        return NULL;
    }
    
    // Write all icon names to temp file
    IconCacheEntry* entry = icon_cache;
    while (entry) {
        if (entry->name) {
            fprintf(fp, "%s\n", entry->name);
        }
        entry = entry->next;
    }
    fclose(fp);
    
    // Use fzf to find best match
    char cmd[MAX_PATH_LENGTH * 2];
    snprintf(cmd, sizeof(cmd), "echo \"%s\" | fzf --filter=\"%s\" --select-1 < %s 2>/dev/null | head -1", 
             query, query, temp_file);
    
    fp = popen(cmd, "r");
    char* result = NULL;
    if (fp) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), fp)) {
            char* newline = strchr(buffer, '\n');
            if (newline) *newline = '\0';
            
            if (strlen(buffer) > 0) {
                result = find_in_cache(buffer, size, NULL);
            }
        }
        pclose(fp);
    }
    
    unlink(temp_file);
    return result;
}

static char* find_icon_for_extension(const char* extension, int size) {
    if (!extension) return NULL;
    
    // Step 1: Get mimetype for extension
    const char* mimetype = get_mimetype_for_extension(extension);
    
    if (mimetype) {
        // Step 2: Convert mimetype to icon name per XDG spec
        char* icon_name = mimetype_to_icon_name(mimetype);
        if (icon_name) {
            // Step 3: Look for the icon in cache
            char* icon_path = find_in_cache(icon_name, size, "MimeTypes");
            if (!icon_path) {
                icon_path = find_in_cache(icon_name, size, NULL);
            }
            
            if (icon_path) {
                free(icon_name);
                return icon_path;
            }
            
            if (getenv("DEBUG_ICONS")) {
                printf("Standard icon '%s' not found for mimetype '%s'\n", icon_name, mimetype);
            }
            
            free(icon_name);
        }
    }
    
    // Step 4: Fallback to fuzzy search with fzf
    if (getenv("DEBUG_ICONS")) {
        printf("Using fuzzy search fallback for extension '%s'\n", extension);
    }
    
    char query[64];
    snprintf(query, sizeof(query), "%s", extension + 1); // Skip the dot
    
    return fuzzy_search_icon(query, size);
}

bool is_image_file(const char* filename) {
    const char* extension = get_file_extension(filename);
    if (!extension) return false;
    
    return (strcasecmp(extension, ".png") == 0 ||
            strcasecmp(extension, ".jpg") == 0 ||
            strcasecmp(extension, ".jpeg") == 0 ||
            strcasecmp(extension, ".gif") == 0 ||
            strcasecmp(extension, ".bmp") == 0 ||
            strcasecmp(extension, ".tiff") == 0 ||
            strcasecmp(extension, ".tif") == 0 ||
            strcasecmp(extension, ".svg") == 0);
}

char* get_thumbnail_path(const char* filename) {
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    
    char* cache_base = malloc(MAX_PATH_LENGTH);
    if (!cache_base) return NULL;
    
    snprintf(cache_base, MAX_PATH_LENGTH, "%s/%s", home, CACHE_DIRECTORY_PATH);
    
    char* thumbnail_path = malloc(MAX_PATH_LENGTH);
    if (!thumbnail_path) {
        free(cache_base);
        return NULL;
    }
    
    snprintf(thumbnail_path, MAX_PATH_LENGTH, "%s/thumb_%s_%dx%d.png", 
             cache_base, filename, current_icon_size, current_icon_size);
    
    free(cache_base);
    return thumbnail_path;
}

bool generate_thumbnail(const char* source_path, const char* thumbnail_path) {
    char cmd[MAX_PATH_LENGTH * 2];
    
    const char* extension = get_file_extension(source_path);
    if (!extension) return false;
    
    if (strcasecmp(extension, ".svg") == 0) {
        snprintf(cmd, sizeof(cmd), "rsvg-convert \"%s\" -o \"%s\" --width=%d --height=%d 2>/dev/null", 
                 source_path, thumbnail_path, current_icon_size, current_icon_size);
    } else {
        snprintf(cmd, sizeof(cmd), "convert \"%s\" -thumbnail %dx%d \"%s\" 2>/dev/null", 
                 source_path, current_icon_size, current_icon_size, thumbnail_path);
    }
    
    return system(cmd) == 0;
}

const char* get_file_extension(const char* filename) {
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) {
        return NULL;
    }
    return dot;
}

void init_theme(const char* theme_name) {
    memset(default_file_icon, 0, sizeof(default_file_icon));
    memset(default_directory_icon, 0, sizeof(default_directory_icon));
    
    // Clear existing cache
    cleanup_cache();
    
    bool theme_loaded = load_theme(theme_name);
    
    // Always add hicolor as final fallback if not already loaded
    load_theme("hicolor");
    
    if (!theme_loaded) {
        load_theme("Adwaita");
        load_theme("gnome");
        load_theme("oxygen");
    }
    
    const char* file_icon_names[] = {
        "text-x-generic", "text-plain", "unknown", 
        "application-x-generic", "gtk-file", "file", NULL
    };
    
    for (int i = 0; file_icon_names[i] && !default_file_icon[0]; i++) {
        char* icon_path = find_in_cache(file_icon_names[i], current_icon_size, NULL);
        if (icon_path) {
            strncpy(default_file_icon, icon_path, MAX_PATH_LENGTH - 1);
            free(icon_path);
        }
    }
    
    const char* dir_icon_names[] = {
        "folder", "inode-directory", "directory", "folder-open", 
        "gtk-directory", "file-manager", NULL
    };
    
    for (int i = 0; dir_icon_names[i] && !default_directory_icon[0]; i++) {
        char* icon_path = find_in_cache(dir_icon_names[i], current_icon_size, NULL);
        if (icon_path) {
            strncpy(default_directory_icon, icon_path, MAX_PATH_LENGTH - 1);
            free(icon_path);
        }
    }
    
    // Debug output
    if (getenv("DEBUG_ICONS")) {
        printf("Theme initialization complete:\n");
        printf("Default file icon: %s\n", default_file_icon[0] ? default_file_icon : "NONE");
        printf("Default directory icon: %s\n", default_directory_icon[0] ? default_directory_icon : "NONE");
        
        ThemeNode* current = theme_chain;
        printf("Loaded themes:\n");
        while (current) {
            printf("  - %s (%s) - %d directories\n", 
                   current->theme.theme_name ? current->theme.theme_name : "unnamed",
                   current->theme.theme_path ? current->theme.theme_path : "no path",
                   current->theme.directory_count);
            current = current->next;
        }
    }
}

void cleanup_theme(void) {
    ThemeNode* current = theme_chain;
    while (current) {
        ThemeNode* next = current->next;
        cleanup_theme_config(&current->theme);
        free(current);
        current = next;
    }
    theme_chain = NULL;
    
    cleanup_cache();
    
    memset(default_file_icon, 0, sizeof(default_file_icon));
    memset(default_directory_icon, 0, sizeof(default_directory_icon));
}

char* get_file_logo(const char* filename, mode_t permissions, uid_t owner) {
    (void)owner;
    
    // TODO: this needs to be from the config
    if (strcmp(filename, "lemon") == 0) return ICON_LEMON;
    if (strcmp(filename, "sentiments") == 0) return ICON_SENTIMENTS;
    if (strcmp(filename, "memory") == 0) return ICON_MEMORY;
    if (strcmp(filename, "state") == 0) return ICON_STATE;
    if (strcmp(filename, "test") == 0) return ICON_TEST;
    
    if (S_ISDIR(permissions)) {
        if (default_directory_icon[0]) {
            return default_directory_icon;
        }
        return NULL;
    }
    
    if (is_image_file(filename)) {
        return get_thumbnail_path(filename);
    }
    
    const char* extension = get_file_extension(filename);
    if (extension) {
        char* icon_path = find_icon_for_extension(extension, current_icon_size);
        if (icon_path) {
            static char cached_paths[20][MAX_PATH_LENGTH];
            static int path_index = 0;
            path_index = (path_index + 1) % 20;
            
            strncpy(cached_paths[path_index], icon_path, MAX_PATH_LENGTH - 1);
            cached_paths[path_index][MAX_PATH_LENGTH - 1] = '\0';
            free(icon_path);
            return cached_paths[path_index];
        }
    }
    
    if (default_file_icon[0]) {
        return default_file_icon;
    }
    
    return NULL;
}
