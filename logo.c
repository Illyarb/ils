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

// Icon cache entry with proper XDG compliance
typedef struct IconCacheEntry {
    char* name;
    char* path;
    char* context;
    char* theme_name;
    IconDirectory dir_info; // Full directory info for proper size matching
    struct IconCacheEntry* next;
} IconCacheEntry;

static IconCacheEntry* icon_cache = NULL;

static const ExtensionMapping extension_mappings[] = {
    {".py", "text/x-python"},
    {".js", "text/javascript"},
    {".ts", "text/x-typescript"},
    {".jsx", "text/javascript"},
    {".tsx", "text/x-typescript"},
    {".c", "text/x-csrc"},
    {".cpp", "text/x-c++src"},
    {".cxx", "text/x-c++src"},
    {".cc", "text/x-c++src"},
    {".C", "text/x-c++src"},
    {".h", "text/x-chdr"},
    {".hpp", "text/x-c++hdr"},
    {".hxx", "text/x-c++hdr"},
    {".java", "text/x-java"},
    {".php", "text/x-php"},
    {".rb", "text/x-ruby"},
    {".go", "text/x-go"},
    {".rs", "text/x-rust"},
    {".sh", "application/x-shellscript"},
    {".bash", "application/x-shellscript"},
    {".zsh", "application/x-shellscript"},
    {".fish", "application/x-shellscript"},
    {".pl", "text/x-perl"},
    {".lua", "text/x-lua"},
    {".r", "text/x-r"},
    
    {".html", "text/html"},
    {".htm", "text/html"},
    {".css", "text/css"},
    {".scss", "text/css"},
    {".sass", "text/css"},
    {".less", "text/css"},
    {".xml", "text/xml"},
    {".xhtml", "application/xhtml+xml"},
    {".json", "application/json"},
    {".yaml", "text/x-yaml"},
    {".yml", "text/x-yaml"},
    {".toml", "text/x-toml"},
    {".ini", "text/plain"},
    {".conf", "text/plain"},
    {".cfg", "text/plain"},
    {".config", "text/plain"},
    
    {".pdf", "application/pdf"},
    {".doc", "application/msword"},
    {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".odt", "application/vnd.oasis.opendocument.text"},
    {".rtf", "application/rtf"},
    {".xls", "application/vnd.ms-excel"},
    {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".ods", "application/vnd.oasis.opendocument.spreadsheet"},
    {".ppt", "application/vnd.ms-powerpoint"},
    {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {".odp", "application/vnd.oasis.opendocument.presentation"},
    
    {".txt", "text/plain"},
    {".md", "text/x-markdown"},
    {".markdown", "text/x-markdown"},
    {".rst", "text/x-rst"},
    {".tex", "text/x-tex"},
    {".log", "text/x-log"},
    
    {".zip", "application/zip"},
    {".tar", "application/x-tar"},
    {".gz", "application/x-gzip"},
    {".bz2", "application/x-bzip2"},
    {".xz", "application/x-xz"},
    {".7z", "application/x-7z-compressed"},
    {".rar", "application/x-rar"},
    {".deb", "application/vnd.debian.binary-package"},
    {".rpm", "application/x-rpm"},
    {".pkg", "application/x-archive"},
    {".dmg", "application/x-apple-diskimage"},
    
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
    {".bmp", "image/bmp"},
    {".tiff", "image/tiff"},
    {".tif", "image/tiff"},
    {".webp", "image/webp"},
    {".ico", "image/x-icon"},
    {".xcf", "image/x-xcf"},
    
    {".mp3", "audio/mpeg"},
    {".wav", "audio/wav"},
    {".ogg", "audio/ogg"},
    {".flac", "audio/flac"},
    {".aac", "audio/aac"},
    {".m4a", "audio/mp4"},
    {".opus", "audio/opus"},
    {".wma", "audio/x-ms-wma"},
    
    {".mp4", "video/mp4"},
    {".mkv", "video/x-matroska"},
    {".avi", "video/x-msvideo"},
    {".mov", "video/quicktime"},
    {".webm", "video/webm"},
    {".wmv", "video/x-ms-wmv"},
    {".flv", "video/x-flv"},
    {".m4v", "video/x-m4v"},
    {".ogv", "video/ogg"},
    
    {".iso", "application/x-iso9660-image"},
    {".exe", "application/x-ms-dos-executable"},
    {".msi", "application/x-msi"},
    {".app", "application/x-executable"},
    {".appimage", "application/x-executable"},
    
    {NULL, NULL}
};

// Get XDG-compliant search paths
static const char* get_icon_search_paths(void) {
    static char search_paths[4096];
    static bool initialized = false;
    
    if (!initialized) {
        const char* home = getenv("HOME");
        if (!home) {
            struct passwd *pw = getpwuid(getuid());
            home = pw ? pw->pw_dir : "/tmp";
        }
        
        const char* xdg_data_home = getenv("XDG_DATA_HOME");
        const char* xdg_data_dirs = getenv("XDG_DATA_DIRS");
        
        if (!xdg_data_dirs) {
            xdg_data_dirs = "/usr/local/share:/usr/share";
        }
        
        // Build search path according to XDG spec
        search_paths[0] = '\0';
        
        // Add XDG_DATA_HOME or ~/.local/share
        if (xdg_data_home) {
            snprintf(search_paths, sizeof(search_paths), "%s/icons", xdg_data_home);
        } else {
            snprintf(search_paths, sizeof(search_paths), "%s/.local/share/icons", home);
        }
        
        // Add ~/.icons for backwards compatibility
        char temp[4096];
        snprintf(temp, sizeof(temp), "%s:%s/.icons", search_paths, home);
        strncpy(search_paths, temp, sizeof(search_paths) - 1);
        
        // Add XDG_DATA_DIRS
        char* xdg_copy = strdup(xdg_data_dirs);
        if (xdg_copy) {
            char* dir = strtok(xdg_copy, ":");
            while (dir) {
                snprintf(temp, sizeof(temp), "%s:%s/icons", search_paths, dir);
                strncpy(search_paths, temp, sizeof(search_paths) - 1);
                dir = strtok(NULL, ":");
            }
            free(xdg_copy);
        }
        
        search_paths[sizeof(search_paths) - 1] = '\0';
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

// Extract size from directory name (e.g., "48x48" -> 48, "scalable" -> 0)
static int extract_size_from_dirname(const char* dirname) {
    if (!dirname) return 0;
    
    // Handle scalable directories
    if (strstr(dirname, "scalable")) return 0;
    
    // Look for NxN pattern
    char* x_pos = strchr(dirname, 'x');
    if (x_pos) {
        // Find start of number
        char* start = x_pos;
        while (start > dirname && isdigit(*(start - 1))) {
            start--;
        }
        if (start < x_pos) {
            return atoi(start);
        }
    }
    
    // Look for just a number at the start
    if (isdigit(dirname[0])) {
        return atoi(dirname);
    }
    
    return 0;
}

// Check if directory has icon subdirectories (for themes without index.theme)
static bool has_icon_subdirs(const char* theme_path) {
    DIR* dir = opendir(theme_path);
    if (!dir) return false;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && entry->d_name[0] != '.') {
            // Check if this looks like an icon size directory
            if (extract_size_from_dirname(entry->d_name) > 0 || 
                strstr(entry->d_name, "scalable")) {
                closedir(dir);
                return true;
            }
        }
    }
    
    closedir(dir);
    return false;
}

// Forward declaration for fallback scanning
static void scan_theme_fallback(const ThemeConfig* theme);

// XDG-compliant size matching function
static int directory_size_distance(const IconDirectory* dir, int size) {
    if (!dir->type) return INT_MAX;
    
    int dir_size = dir->size ? atoi(dir->size) : 48;
    
    // If directory name contains size info, prefer that over Size field
    int dirname_size = extract_size_from_dirname(dir->name);
    if (dirname_size > 0) {
        dir_size = dirname_size;
    }
    
    if (strcmp(dir->type, "Fixed") == 0) {
        return (dir_size == size) ? 0 : INT_MAX;
    }
    
    if (strcmp(dir->type, "Scalable") == 0) {
        int min_size = dir->min_size > 0 ? dir->min_size : 1;
        int max_size = dir->max_size > 0 ? dir->max_size : 512;
        
        if (size < min_size) return min_size - size;
        if (size > max_size) return size - max_size;
        return 0;
    }
    
    if (strcmp(dir->type, "Threshold") == 0) {
        int threshold = dir->threshold > 0 ? dir->threshold : 2;
        int min_size = dir_size - threshold;
        int max_size = dir_size + threshold;
        
        if (size < min_size) return min_size - size;
        if (size > max_size) return size - max_size;
        return abs(dir_size - size);
    }
    
    return abs(dir_size - size);
}

static void add_to_cache(const char* name, const char* path, const char* context, 
                        const char* theme_name, const IconDirectory* dir_info) {
    IconCacheEntry* entry = malloc(sizeof(IconCacheEntry));
    if (!entry) return;
    
    entry->name = strdup(name);
    entry->path = strdup(path);
    entry->context = context ? strdup(context) : NULL;
    entry->theme_name = theme_name ? strdup(theme_name) : NULL;
    
    // Copy directory info for size matching
    memset(&entry->dir_info, 0, sizeof(IconDirectory));
    if (dir_info) {
        entry->dir_info.size = dir_info->size ? strdup(dir_info->size) : strdup("48");
        entry->dir_info.type = dir_info->type ? strdup(dir_info->type) : strdup("Threshold");
        entry->dir_info.context = dir_info->context ? strdup(dir_info->context) : NULL;
        entry->dir_info.min_size = dir_info->min_size;
        entry->dir_info.max_size = dir_info->max_size;
        entry->dir_info.threshold = dir_info->threshold;
    } else {
        entry->dir_info.size = strdup("48");
        entry->dir_info.type = strdup("Threshold");
        entry->dir_info.threshold = 2;
    }
    
    entry->next = icon_cache;
    icon_cache = entry;
}

// Find best matching icon using XDG algorithm with case-insensitive context matching
static char* find_best_icon_match(const char* name, int size, const char* preferred_context) {
    IconCacheEntry* best_match = NULL;
    int best_distance = INT_MAX;
    bool found_exact_context = false;
    
    IconCacheEntry* entry = icon_cache;
    while (entry) {
        if (!entry->name || strcmp(entry->name, name) != 0) {
            entry = entry->next;
            continue;
        }
        
        // Calculate size distance using XDG algorithm
        int distance = directory_size_distance(&entry->dir_info, size);
        if (distance == INT_MAX) {
            entry = entry->next;
            continue;
        }
        
        bool context_matches = false;
        if (preferred_context && entry->context) {
            context_matches = (strcasecmp(entry->context, preferred_context) == 0);
        }
        
        // Prefer exact context matches
        if (preferred_context) {
            if (context_matches && !found_exact_context) {
                // First exact context match
                best_match = entry;
                best_distance = distance;
                found_exact_context = true;
            } else if (context_matches && found_exact_context && distance < best_distance) {
                // Better exact context match
                best_match = entry;
                best_distance = distance;
            } else if (!found_exact_context && distance < best_distance) {
                // Better non-context match (only if no exact context found yet)
                best_match = entry;
                best_distance = distance;
            }
        } else {
            // No context preference
            if (distance < best_distance) {
                best_match = entry;
                best_distance = distance;
            }
        }
        
        entry = entry->next;
    }
    
    if (getenv("DEBUG_ICONS") && best_match) {
        printf("Found icon '%s' for size %d: %s (distance: %d, context: %s)\n", 
               name, size, best_match->path, best_distance, 
               best_match->context ? best_match->context : "none");
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
        free(entry->theme_name);
        free(entry->dir_info.name);
        free(entry->dir_info.size);
        free(entry->dir_info.context);
        free(entry->dir_info.type);
        free(entry);
        entry = next;
    }
    icon_cache = NULL;
}

// Scan directory and add icons to cache with proper directory info
static void scan_icon_directory(const char* dir_path, const IconDirectory* dir_info, const char* theme_name) {
    DIR* dir = opendir(dir_path);
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG && entry->d_type != DT_LNK) continue;
        
        char* dot = strrchr(entry->d_name, '.');
        if (!dot) continue;
        
        // Check for valid icon extensions
        char* ext = dot + 1;
        if (strcasecmp(ext, "png") != 0 && strcasecmp(ext, "svg") != 0 && 
            strcasecmp(ext, "xpm") != 0 && strcasecmp(ext, "svgz") != 0) {
            continue;
        }
        
        // Extract icon name (without extension)
        size_t name_len = dot - entry->d_name;
        char* icon_name = malloc(name_len + 1);
        if (!icon_name) continue;
        
        strncpy(icon_name, entry->d_name, name_len);
        icon_name[name_len] = '\0';
        
        char full_path[MAX_PATH_LENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        add_to_cache(icon_name, full_path, dir_info->context, theme_name, dir_info);
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

// Parse a single section of index.theme
static void parse_icon_theme_section(FILE* file, const char* section_name, ThemeConfig* theme) {
    char line[1024];
    IconDirectory* dir = NULL;
    
    if (strcmp(section_name, "Icon Theme") == 0) {
        // Parse main theme section
        while (fgets(line, sizeof(line), file)) {
            // Check for next section
            if (line[0] == '[') {
                fseek(file, -(long)strlen(line), SEEK_CUR);
                break;
            }
            
            char* equals = strchr(line, '=');
            if (!equals) continue;
            
            *equals = '\0';
            char* key = trim(line);
            char* value = trim(equals + 1);
            
            // Remove quotes if present
            if (value[0] == '"' && value[strlen(value)-1] == '"') {
                value[strlen(value)-1] = '\0';
                value++;
            }
            
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
                theme->hidden = (strcasecmp(value, "true") == 0);
            } else if (strcmp(key, "Inherits") == 0) {
                // Parse inherited themes
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
                // Parse directory list
                char* value_copy = strdup(value);
                char* token = strtok(value_copy, ",");
                theme->directory_count = 0;
                
                while (token) {
                    theme->directories = realloc(theme->directories,
                        (theme->directory_count + 1) * sizeof(IconDirectory));
                    if (!theme->directories) break;
                    
                    IconDirectory* new_dir = &theme->directories[theme->directory_count];
                    memset(new_dir, 0, sizeof(IconDirectory));
                    new_dir->name = strdup(trim(token));
                    // Set defaults - will be overridden by directory section
                    new_dir->type = strdup("Threshold");
                    new_dir->size = strdup("48");
                    new_dir->threshold = 2;
                    new_dir->min_size = 1;
                    new_dir->max_size = 512;
                    
                    theme->directory_count++;
                    token = strtok(NULL, ",");
                }
                free(value_copy);
            }
        }
    } else {
        // Parse directory-specific section
        for (int i = 0; i < theme->directory_count; i++) {
            if (strcmp(theme->directories[i].name, section_name) == 0) {
                dir = &theme->directories[i];
                break;
            }
        }
        
        if (!dir) return; // Directory not found in main list
        
        while (fgets(line, sizeof(line), file)) {
            // Check for next section
            if (line[0] == '[') {
                fseek(file, -(long)strlen(line), SEEK_CUR);
                break;
            }
            
            char* equals = strchr(line, '=');
            if (!equals) continue;
            
            *equals = '\0';
            char* key = trim(line);
            char* value = trim(equals + 1);
            
            // Remove quotes if present
            if (value[0] == '"' && value[strlen(value)-1] == '"') {
                value[strlen(value)-1] = '\0';
                value++;
            }
            
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
        
        // Validate and set defaults for directory
        if (!dir->size) dir->size = strdup("48");
        if (!dir->type) dir->type = strdup("Threshold");
        
        if (strcmp(dir->type, "Scalable") == 0) {
            if (dir->min_size <= 0) dir->min_size = 1;
            if (dir->max_size <= 0) dir->max_size = 512;
        } else if (strcmp(dir->type, "Threshold") == 0) {
            if (dir->threshold <= 0) dir->threshold = 2;
        }
    }
}

// Parse index.theme file completely with better error handling
static bool parse_index_theme(const char* theme_path, ThemeConfig* theme) {
    char index_path[MAX_PATH_LENGTH];
    snprintf(index_path, sizeof(index_path), "%s/index.theme", theme_path);
    
    FILE* file = fopen(index_path, "r");
    if (!file) {
        if (getenv("DEBUG_ICONS")) {
            printf("Cannot open %s\n", index_path);
        }
        
        // Create a minimal theme config for themes without index.theme
        theme->theme_name = strdup("unknown");
        theme->directory_count = 0;
        theme->directories = NULL;
        return true; // Let fallback scanning handle it
    }
    
    char line[1024];
    
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\0') continue;
        
        // Check for section headers
        if (line[0] == '[' && line[strlen(line) - 1] == ']') {
            line[strlen(line) - 1] = '\0';
            char* section_name = line + 1;
            
            parse_icon_theme_section(file, section_name, theme);
        }
    }
    
    fclose(file);
    
    // Set theme name if not set
    if (!theme->theme_name) {
        char* theme_basename = strrchr(theme_path, '/');
        theme->theme_name = strdup(theme_basename ? theme_basename + 1 : theme_path);
    }
    
    if (getenv("DEBUG_ICONS")) {
        printf("Parsed theme: %s (%d directories)\n", 
               theme->theme_name, theme->directory_count);
    }
    
    return true; // Always return true, let fallback handle missing directories
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
            // Check if index.theme exists or if directory has icon subdirectories
            char index_path[MAX_PATH_LENGTH];
            if (strlen(theme_path) + 13 < MAX_PATH_LENGTH) {
                snprintf(index_path, sizeof(index_path), "%s/index.theme", theme_path);
                if (file_exists(index_path) || has_icon_subdirs(theme_path)) {
                    char* result = strdup(theme_path);
                    free(paths_copy);
                    return result;
                }
            }
        }
        path = strtok(NULL, ":");
    }
    
    free(paths_copy);
    return NULL;
}

// Scan all directories for a theme, being robust about missing directories
static void scan_theme_icons(const ThemeConfig* theme) {
    if (!theme || !theme->theme_path) return;
    
    int scanned_count = 0;
    
    for (int i = 0; i < theme->directory_count; i++) {
        const IconDirectory* dir = &theme->directories[i];
        
        char dir_path[MAX_PATH_LENGTH];
        snprintf(dir_path, sizeof(dir_path), "%s/%s", theme->theme_path, dir->name);
        
        if (directory_exists(dir_path)) {
            scan_icon_directory(dir_path, dir, theme->theme_name);
            scanned_count++;
            
            if (getenv("DEBUG_ICONS")) {
                printf("Scanned %s/%s (context: %s, type: %s, size: %s)\n", 
                       theme->theme_name, dir->name,
                       dir->context ? dir->context : "none",
                       dir->type ? dir->type : "none",
                       dir->size ? dir->size : "none");
            }
        } else {
            if (getenv("DEBUG_ICONS")) {
                printf("Directory not found: %s\n", dir_path);
            }
        }
    }
    
    // If very few directories were found from index.theme, do a fallback scan
    if (scanned_count < 3) {
        if (getenv("DEBUG_ICONS")) {
            printf("Few directories found from index.theme (%d), doing fallback scan\n", scanned_count);
        }
        scan_theme_fallback(theme);
    }
}

// Fallback: scan actual directories when index.theme is incomplete/wrong
static void scan_theme_fallback(const ThemeConfig* theme) {
    if (!theme || !theme->theme_path) return;
    
    DIR* theme_dir = opendir(theme->theme_path);
    if (!theme_dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(theme_dir)) != NULL) {
        if (entry->d_type != DT_DIR || entry->d_name[0] == '.') continue;
        
        char size_dir_path[MAX_PATH_LENGTH];
        snprintf(size_dir_path, sizeof(size_dir_path), "%s/%s", theme->theme_path, entry->d_name);
        
        // Extract size from directory name
        int dir_size = extract_size_from_dirname(entry->d_name);
        if (dir_size == 0 && strncmp(entry->d_name, "scalable", 8) != 0) {
            continue; // Skip non-size directories
        }
        
        // Scan subdirectories (contexts)
        DIR* size_dir = opendir(size_dir_path);
        if (!size_dir) continue;
        
        struct dirent* context_entry;
        while ((context_entry = readdir(size_dir)) != NULL) {
            if (context_entry->d_type != DT_DIR || context_entry->d_name[0] == '.') continue;
            
            char context_dir_path[MAX_PATH_LENGTH];
            if (strlen(size_dir_path) + strlen(context_entry->d_name) + 2 < MAX_PATH_LENGTH) {
                snprintf(context_dir_path, sizeof(context_dir_path), "%s/%s", size_dir_path, context_entry->d_name);
            } else {
                continue; // Skip if path would be too long
            }
            
            // Create a fake IconDirectory for this discovered directory
            IconDirectory fake_dir;
            memset(&fake_dir, 0, sizeof(IconDirectory));
            
            // Set up directory info
            char size_str[16];
            snprintf(size_str, sizeof(size_str), "%d", dir_size > 0 ? dir_size : 48);
            fake_dir.size = size_str;
            fake_dir.name = entry->d_name;
            
            // Determine context from directory name
            if (strcasecmp(context_entry->d_name, "apps") == 0 || strcasecmp(context_entry->d_name, "applications") == 0) {
                fake_dir.context = "Applications";
            } else if (strcasecmp(context_entry->d_name, "mimetypes") == 0) {
                fake_dir.context = "MimeTypes";
            } else if (strcasecmp(context_entry->d_name, "places") == 0) {
                fake_dir.context = "Places";
            } else if (strcasecmp(context_entry->d_name, "devices") == 0) {
                fake_dir.context = "Devices";
            } else if (strcasecmp(context_entry->d_name, "actions") == 0) {
                fake_dir.context = "Actions";
            } else if (strcasecmp(context_entry->d_name, "categories") == 0) {
                fake_dir.context = "Categories";
            } else if (strcasecmp(context_entry->d_name, "status") == 0) {
                fake_dir.context = "Status";
            } else if (strcasecmp(context_entry->d_name, "emblems") == 0) {
                fake_dir.context = "Emblems";
            } else {
                fake_dir.context = context_entry->d_name; // Use directory name as context
            }
            
            // Set type based on directory name
            if (strncmp(entry->d_name, "scalable", 8) == 0) {
                fake_dir.type = "Scalable";
                fake_dir.min_size = 16;
                fake_dir.max_size = 512;
            } else {
                fake_dir.type = "Fixed";
            }
            
            scan_icon_directory(context_dir_path, &fake_dir, theme->theme_name);
            
            if (getenv("DEBUG_ICONS")) {
                printf("Fallback scanned %s/%s/%s (context: %s, size: %s)\n", 
                       theme->theme_name, entry->d_name, context_entry->d_name,
                       fake_dir.context, fake_dir.size);
            }
        }
        
        closedir(size_dir);
    }
    
    closedir(theme_dir);
}

// Load theme with proper inheritance handling
static bool load_theme_recursive(const char* theme_name, int depth) {
    if (depth > 10) {
        if (getenv("DEBUG_ICONS")) {
            printf("Theme inheritance too deep: %s\n", theme_name);
        }
        return false;
    }
    
    if (theme_already_loaded(theme_name)) {
        return true;
    }
    
    char* theme_path = find_theme_path(theme_name);
    if (!theme_path) {
        if (getenv("DEBUG_ICONS")) {
            printf("Theme not found: %s\n", theme_name);
        }
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
    
    // CRITICAL: Load inherited themes FIRST (they are fallbacks)
    for (int i = 0; i < node->theme.inherits_count; i++) {
        load_theme_recursive(node->theme.inherits[i], depth + 1);
    }
    
    // Then add this theme to chain and scan its icons
    add_theme_to_chain(node);
    scan_theme_icons(&node->theme);
    
    if (getenv("DEBUG_ICONS")) {
        printf("Loaded theme: %s (%d directories, %d inherited)\n", 
               theme_name, node->theme.directory_count, node->theme.inherits_count);
    }
    
    return true;
}

static bool load_theme(const char* theme_name) {
    return load_theme_recursive(theme_name, 0);
}

// Convert MIME type to icon name per XDG spec
static char* mimetype_to_icon_name(const char* mimetype) {
    if (!mimetype) return NULL;
    
    char* icon_name = strdup(mimetype);
    if (!icon_name) return NULL;
    
    // Replace '/' with '-' as per XDG spec
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

// Find icon with comprehensive fallback strategy
static char* find_icon_with_fallbacks(const char* icon_name, int size, const char* context) {
    if (!icon_name) return NULL;
    
    // Try different context variations for better compatibility
    const char* context_variants[] = {
        context,
        context ? (strcasecmp(context, "mimetypes") == 0 ? "MimeTypes" : 
                  strcasecmp(context, "MimeTypes") == 0 ? "mimetypes" : context) : NULL,
        NULL
    };
    
    // 1. Try with context variants
    for (int i = 0; context_variants[i]; i++) {
        char* result = find_best_icon_match(icon_name, size, context_variants[i]);
        if (result) return result;
    }
    
    // 2. Try without context preference  
    char* result = find_best_icon_match(icon_name, size, NULL);
    if (result) return result;
    
    // 3. Try generic versions for MIME types
    if (strstr(icon_name, "-")) {
        // Try application-x-generic, text-x-generic, etc.
        char* generic = strdup(icon_name);
        char* dash = strrchr(generic, '-');
        if (dash) {
            strcpy(dash, "-x-generic");
            for (int i = 0; context_variants[i]; i++) {
                result = find_best_icon_match(generic, size, context_variants[i]);
                if (result) {
                    free(generic);
                    return result;
                }
            }
            result = find_best_icon_match(generic, size, NULL);
            if (result) {
                free(generic);
                return result;
            }
        }
        free(generic);
        
        // Try just the category (e.g., "application", "text")
        char* category = strdup(icon_name);
        dash = strchr(category, '-');
        if (dash) {
            *dash = '\0';
            if (strlen(category) > 0) {
                for (int i = 0; context_variants[i]; i++) {
                    result = find_best_icon_match(category, size, context_variants[i]);
                    if (result) {
                        free(category);
                        return result;
                    }
                }
                result = find_best_icon_match(category, size, NULL);
                if (result) {
                    free(category);
                    return result;
                }
            }
        }
        free(category);
    }
    
    // 4. Try without common suffixes
    const char* suffixes[] = {"-symbolic", "-dark", "-light", "-color", NULL};
    for (int i = 0; suffixes[i]; i++) {
        if (strstr(icon_name, suffixes[i])) {
            char* base = strdup(icon_name);
            char* suffix_pos = strstr(base, suffixes[i]);
            if (suffix_pos) {
                *suffix_pos = '\0';
                for (int j = 0; context_variants[j]; j++) {
                    result = find_best_icon_match(base, size, context_variants[j]);
                    if (result) {
                        free(base);
                        return result;
                    }
                }
                result = find_best_icon_match(base, size, NULL);
                if (result) {
                    free(base);
                    return result;
                }
            }
            free(base);
        }
    }
    
    if (getenv("DEBUG_ICONS")) {
        printf("No icon found for '%s' (size: %d, context: %s)\n", 
               icon_name, size, context ? context : "none");
    }
    
    return NULL;
}

static char* find_icon_for_extension(const char* extension, int size) {
    if (!extension) return NULL;
    
    // Get MIME type for extension
    const char* mimetype = get_mimetype_for_extension(extension);
    if (mimetype) {
        char* icon_name = mimetype_to_icon_name(mimetype);
        if (icon_name) {
            // Try with MimeTypes context first, then mimetypes, then no context
            char* result = find_icon_with_fallbacks(icon_name, size, "MimeTypes");
            if (!result) {
                result = find_icon_with_fallbacks(icon_name, size, "mimetypes");
            }
            if (!result) {
                result = find_icon_with_fallbacks(icon_name, size, NULL);
            }
            
            free(icon_name);
            if (result) return result;
        }
    }
    
    // Fallback: try extension name directly
    char ext_name[64];
    snprintf(ext_name, sizeof(ext_name), "%s", extension + 1); // Skip dot
    
    // Convert to lowercase
    for (int i = 0; ext_name[i]; i++) {
        ext_name[i] = tolower(ext_name[i]);
    }
    
    return find_icon_with_fallbacks(ext_name, size, NULL);
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
            strcasecmp(extension, ".svg") == 0 ||
            strcasecmp(extension, ".webp") == 0 ||
            strcasecmp(extension, ".ico") == 0 ||
            strcasecmp(extension, ".xcf") == 0);
}

char* get_thumbnail_path(const char* filename) {
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    
    char cache_dir[MAX_PATH_LENGTH];
    snprintf(cache_dir, sizeof(cache_dir), "%s/%s", home, CACHE_DIRECTORY_PATH);
    
    // Create cache directory if it doesn't exist
    char* dir_copy = strdup(cache_dir);
    if (dir_copy) {
        char* slash = strchr(dir_copy + 1, '/');
        while (slash) {
            *slash = '\0';
            mkdir(dir_copy, 0755);
            *slash = '/';
            slash = strchr(slash + 1, '/');
        }
        mkdir(dir_copy, 0755);
        free(dir_copy);
    }
    
    char* thumbnail_path = malloc(MAX_PATH_LENGTH);
    if (!thumbnail_path) return NULL;
    
    // Create a safe filename from the original filename
    char safe_name[256];
    const char* base_name = strrchr(filename, '/');
    base_name = base_name ? base_name + 1 : filename;
    
    int j = 0;
    for (int i = 0; base_name[i] && j < 200; i++) {
        if (isalnum(base_name[i]) || base_name[i] == '.' || base_name[i] == '-' || base_name[i] == '_') {
            safe_name[j++] = base_name[i];
        } else {
            safe_name[j++] = '_';
        }
    }
    safe_name[j] = '\0';
    
    if (strlen(cache_dir) + strlen(safe_name) + 50 < MAX_PATH_LENGTH) {
        snprintf(thumbnail_path, MAX_PATH_LENGTH, "%s/thumb_%s_%dx%d.png", 
                 cache_dir, safe_name, current_icon_size, current_icon_size);
    } else {
        // Fallback for very long paths
        snprintf(thumbnail_path, MAX_PATH_LENGTH, "%s/thumb_%dx%d.png", 
                 cache_dir, current_icon_size, current_icon_size);
    }
    
    return thumbnail_path;
}

bool generate_thumbnail(const char* source_path, const char* thumbnail_path) {
    if (!source_path || !thumbnail_path) return false;
    
    // Check if thumbnail already exists and is newer than source
    struct stat source_stat, thumb_stat;
    if (stat(source_path, &source_stat) != 0) return false;
    
    if (stat(thumbnail_path, &thumb_stat) == 0) {
        if (thumb_stat.st_mtime >= source_stat.st_mtime) {
            return true; // Thumbnail is up to date
        }
    }
    
    char cmd[MAX_PATH_LENGTH * 2 + 100];
    const char* extension = get_file_extension(source_path);
    
    if (extension && strcasecmp(extension, ".svg") == 0) {
        // Use rsvg-convert for SVG files
        snprintf(cmd, sizeof(cmd), 
                "rsvg-convert \"%s\" -o \"%s\" --width=%d --height=%d 2>/dev/null", 
                source_path, thumbnail_path, current_icon_size, current_icon_size);
    } else {
        // Use ImageMagick for other formats
        snprintf(cmd, sizeof(cmd), 
                "convert \"%s[0]\" -thumbnail %dx%d \"%s\" 2>/dev/null", 
                source_path, current_icon_size, current_icon_size, thumbnail_path);
    }
    
    int result = system(cmd);
    return result == 0 && file_exists(thumbnail_path);
}

const char* get_file_extension(const char* filename) {
    if (!filename) return NULL;
    
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) {
        return NULL;
    }
    return dot;
}

// Initialize theme system
void init_theme(const char* theme_name) {
    // Clear existing state
    cleanup_theme();
    
    // Load the requested theme (this will load inherited themes first)
    bool theme_loaded = load_theme(theme_name);
    
    // Always ensure hicolor is loaded as final fallback
    if (!theme_already_loaded("hicolor")) {
        load_theme("hicolor");
    }
    
    // If primary theme failed, try common fallbacks
    if (!theme_loaded) {
        const char* fallback_themes[] = {"Adwaita", "gnome", "oxygen", "breeze", NULL};
        for (int i = 0; fallback_themes[i]; i++) {
            if (load_theme(fallback_themes[i])) {
                break;
            }
        }
    }
    
    // Find default icons
    const char* file_icon_candidates[] = {
        "text-x-generic", "text-plain", "unknown", 
        "application-x-generic", "gtk-file", "file", 
        "text-x-preview", "empty", NULL
    };
    
    default_file_icon[0] = '\0';
    for (int i = 0; file_icon_candidates[i] && !default_file_icon[0]; i++) {
        char* icon_path = find_best_icon_match(file_icon_candidates[i], current_icon_size, NULL);
        if (icon_path) {
            strncpy(default_file_icon, icon_path, MAX_PATH_LENGTH - 1);
            default_file_icon[MAX_PATH_LENGTH - 1] = '\0';
            free(icon_path);
        }
    }
    
    const char* dir_icon_candidates[] = {
        "folder", "inode-directory", "directory", "folder-open", 
        "gtk-directory", "file-manager", "places-folder", NULL
    };
    
    default_directory_icon[0] = '\0';
    for (int i = 0; dir_icon_candidates[i] && !default_directory_icon[0]; i++) {
        char* icon_path = find_best_icon_match(dir_icon_candidates[i], current_icon_size, NULL);
        if (icon_path) {
            strncpy(default_directory_icon, icon_path, MAX_PATH_LENGTH - 1);
            default_directory_icon[MAX_PATH_LENGTH - 1] = '\0';
            free(icon_path);
        }
    }
    
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
        
        // Count icons in cache
        int icon_count = 0;
        IconCacheEntry* entry = icon_cache;
        while (entry) {
            icon_count++;
            entry = entry->next;
        }
        printf("Total icons in cache: %d\n", icon_count);
    }
}

// Clean up theme system
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
    
    default_file_icon[0] = '\0';
    default_directory_icon[0] = '\0';
}

// Main function to get file logo/icon
char* get_file_logo(const char* filename, mode_t permissions, uid_t owner) {
    (void)owner; // Unused parameter
    
    if (!filename) return NULL;
    
    // Handle special custom icons
    if (strcmp(filename, "lemon") == 0) return ICON_LEMON;
    if (strcmp(filename, "sentiments") == 0) return ICON_SENTIMENTS;
    if (strcmp(filename, "memory") == 0) return ICON_MEMORY;
    if (strcmp(filename, "state") == 0) return ICON_STATE;
    if (strcmp(filename, "test") == 0) return ICON_TEST;
    
    // Handle directories
    if (S_ISDIR(permissions)) {
        // Try context-specific directory icons
        const char* base_name = strrchr(filename, '/');
        base_name = base_name ? base_name + 1 : filename;
        
        // Check for special directory names
        const char* special_dirs[][2] = {
            {"Desktop", "user-desktop"},
            {"Documents", "folder-documents"},
            {"Downloads", "folder-downloads"},
            {"Music", "folder-music"},
            {"Pictures", "folder-pictures"},
            {"Videos", "folder-videos"},
            {"Public", "folder-publicshare"},
            {"Templates", "folder-templates"},
            {".git", "folder-development"},
            {"src", "folder-development"},
            {"bin", "folder-system"},
            {"lib", "folder-library"},
            {"tmp", "folder-temp"},
            {"var", "folder-system"},
            {"etc", "folder-config"},
            {"home", "folder-home"},
            {NULL, NULL}
        };
        
        for (int i = 0; special_dirs[i][0]; i++) {
            if (strcasecmp(base_name, special_dirs[i][0]) == 0) {
                char* icon_path = find_icon_with_fallbacks(special_dirs[i][1], current_icon_size, "Places");
                if (icon_path) return icon_path;
            }
        }
        
        // Return default directory icon
        return default_directory_icon[0] ? default_directory_icon : NULL;
    }
    
    // Handle image files - try to generate thumbnail
    if (is_image_file(filename)) {
        char* thumb_path = get_thumbnail_path(filename);
        if (thumb_path) {
            if (generate_thumbnail(filename, thumb_path)) {
                return thumb_path;
            }
            free(thumb_path);
        }
    }
    
    // Handle regular files by extension
    const char* extension = get_file_extension(filename);
    if (extension) {
        char* icon_path = find_icon_for_extension(extension, current_icon_size);
        if (icon_path) {
            return icon_path;
        }
    }
    
    // Try executable detection
    if (permissions & S_IXUSR) {
        char* exec_icon = find_icon_with_fallbacks("application-x-executable", current_icon_size, "MimeTypes");
        if (exec_icon) return exec_icon;
    }
    
    // Return default file icon
    return default_file_icon[0] ? default_file_icon : NULL;
}
