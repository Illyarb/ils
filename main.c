#define _GNU_SOURCE
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pwd.h>
#include "config.h"
#include "logo.h"

#define move_cursor(X, Y) printf("\033[%d;%dH", Y, X)
#define go_up(N) printf("\033[%dA", N)
#define go_down(N) printf("\033[%dB", N)
#define go_right(N) printf("\033[%dC", N)
#define go_left(N) printf("\033[%dD", N)

static char CACHE_PATH[MAX_PATH_LENGTH];
int current_icon_size = DEFAULT_ICON_SIZE;

typedef struct {
    char* name;
    const char* color;
    char* icon_path;
    char* cached_png_path;
    mode_t permissions;
    uid_t owner;
    size_t name_length;
    bool is_thumbnail;
} FileEntry;

static const char* get_color_code(mode_t mode) {
    if (S_ISDIR(mode)) return BLUE;
    if (mode & S_IXUSR) return GREEN;
    return RESET;
}

static const char b64_table[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *base64_encode(const unsigned char *data, size_t len) {
    size_t out_len = 4 * ((len + 2) / 3);
    char *out = malloc(out_len + 1);
    if (!out) return NULL;

    size_t i, j;
    for (i = 0, j = 0; i < len; ) {
        unsigned octet_a = i < len ? data[i++] : 0;
        unsigned octet_b = i < len ? data[i++] : 0;
        unsigned octet_c = i < len ? data[i++] : 0;

        unsigned triple = (octet_a << 16) | (octet_b << 8) | (octet_c);

        out[j++] = b64_table[(triple >> 18) & 0x3F];
        out[j++] = b64_table[(triple >> 12) & 0x3F];
        out[j++] = (i > (len + 1)) ? '=' : b64_table[(triple >> 6) & 0x3F];
        out[j++] = (i > len)      ? '=' : b64_table[triple & 0x3F];
    }
    out[j] = '\0';
    return out;
}

static void init_cache_path(void) {
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        home = pw->pw_dir;
    }
    snprintf(CACHE_PATH, sizeof(CACHE_PATH), "%s/%s", home, CACHE_DIRECTORY_PATH);
}

static char* get_cached_png_path(const char* svg_path) {
    if (!svg_path) return NULL;
    
    char* filename = strrchr(svg_path, '/');
    if (!filename) filename = (char*)svg_path;
    else filename++;
    
    char* dot = strrchr(filename, '.');
    size_t base_len = dot ? (size_t)(dot - filename) : strlen(filename);
    
    char* cached_path = malloc(strlen(CACHE_PATH) + base_len + 20);
    if (!cached_path) return NULL;
    
    snprintf(cached_path, strlen(CACHE_PATH) + base_len + 20, "%s/%.*s_%dx%d.png", 
             CACHE_PATH, (int)base_len, filename, current_icon_size, current_icon_size);
    
    return cached_path;
}

static bool cache_svg(const char* svg_path, const char* png_path) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rsvg-convert \"%s\" -o \"%s\" --width=%d --height=%d 2>/dev/null", 
             svg_path, png_path, current_icon_size, current_icon_size);
    return system(cmd) == 0;
}

static void ensure_cache_directory(void) {
    struct stat st = {0};
    
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        home = pw->pw_dir;
    }
    
    char local_path[MAX_PATH_LENGTH];
    snprintf(local_path, sizeof(local_path), "%s/.local", home);
    if (stat(local_path, &st) == -1) {
        mkdir(local_path, 0755);
    }
    
    char share_path[MAX_PATH_LENGTH];
    snprintf(share_path, sizeof(share_path), "%s/.local/share", home);
    if (stat(share_path, &st) == -1) {
        mkdir(share_path, 0755);
    }
    
    char ils_path[MAX_PATH_LENGTH];
    snprintf(ils_path, sizeof(ils_path), "%s/.local/share/ils", home);
    if (stat(ils_path, &st) == -1) {
        mkdir(ils_path, 0755);
    }
    
    if (stat(CACHE_PATH, &st) == -1) {
        mkdir(CACHE_PATH, 0755);
    }
}

static bool ensure_png_exists(const char* icon_path, const char* cached_png_path, bool is_thumbnail) {
    if (!icon_path || !cached_png_path) return false;
    
    if (is_thumbnail) {
        struct stat st;
        return stat(icon_path, &st) == 0;
    }
    
    struct stat st;
    if (stat(cached_png_path, &st) == 0) {
        return true;
    }
    
    struct stat svg_st;
    if (stat(icon_path, &svg_st) == 0) {
        return cache_svg(icon_path, cached_png_path);
    }
    
    return false;
}

static void cache_all_icons(FileEntry* files, int file_count) {
    ensure_cache_directory();
    
    for (int i = 0; i < file_count; i++) {
        if (!files[i].icon_path) continue;
        
        if (files[i].is_thumbnail) {
            continue;
        }
        
        if (!files[i].cached_png_path) continue;
        
        struct stat st;
        if (stat(files[i].cached_png_path, &st) != 0) {
            if (!cache_svg(files[i].icon_path, files[i].cached_png_path)) {
                struct stat svg_st;
                if (stat(files[i].icon_path, &svg_st) != 0) {
                    free(files[i].cached_png_path);
                    files[i].cached_png_path = NULL;
                }
            }
        }
    }
}

static void draw_png(int x, int y, int col, int row, const char *png_path) {
    FILE *fp = fopen(png_path, "rb");
    if (!fp) {
        return;
    }

    fseek(fp, 0, SEEK_END);
    long png_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    unsigned char *png_data = malloc(png_size);
    if (!png_data) {
        fclose(fp);
        return;
    }

    if (fread(png_data, 1, png_size, fp) != (size_t)png_size) {
        free(png_data);
        fclose(fp);
        return;
    }
    fclose(fp);

    char *encoded = base64_encode(png_data, png_size);
    free(png_data);
    
    if (!encoded) {
        return;
    }

    size_t chunk_size = 4096;
    size_t len_encoded = strlen(encoded);
    size_t pos = 0;

    printf("\033_G");
    printf("f=100,a=T,x=%d,y=%d,c=%d,r=%d,", x, y, col, row);
    
    while (pos < len_encoded) {
        size_t remaining = len_encoded - pos;
        size_t this_chunk = (remaining > chunk_size) ? chunk_size : remaining;
        
        if (pos == 0 && this_chunk < len_encoded) {
            printf("m=1;");
        } else if (pos == 0 && this_chunk == len_encoded) {
            printf("m=0;");
        } else if (pos + this_chunk < len_encoded) {
            printf("\033\\\033_Gm=1;");
        } else {
            printf("\033\\\033_Gm=0;");
        }
        
        fwrite(&encoded[pos], 1, this_chunk, stdout);
        pos += this_chunk;
    }

    printf("\033\\");
    fflush(stdout);

    free(encoded);
}

static void parse_arguments(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--icon-size") == 0 && i + 1 < argc) {
            int size = atoi(argv[i + 1]);
            if (size == ICON_SIZE_16 || size == ICON_SIZE_32 || size == ICON_SIZE_64) {
                current_icon_size = size;
            }
            i++;
        }
    }
}

int main(int argc, char* argv[]) {
    DIR *dir;
    struct dirent *entry;
    struct winsize w;
    
    parse_arguments(argc, argv);
    
    init_cache_path();
    init_theme(DEFAULT_THEME);
    
    int capacity = INITIAL_CAPACITY;
    FileEntry* files = malloc(capacity * sizeof(FileEntry));
    int file_count = 0;
    size_t max_filename_length = 0;
    
    printf("\n");
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    
    dir = opendir(".");
    if (!dir) {
        fprintf(stderr, "Error opening directory\n");
        return 1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            struct stat st;
            if (stat(entry->d_name, &st) == -1) {
                continue;
            }

            if (file_count >= capacity) {
                capacity *= 2;
                FileEntry* tmp = realloc(files, capacity * sizeof(FileEntry));
                if (!tmp) {
                    fprintf(stderr, "Memory allocation failed\n");
                    free(files);
                    closedir(dir);
                    return 1;
                }
                files = tmp;
            }
            
            files[file_count].name = strdup(entry->d_name);
            files[file_count].permissions = st.st_mode;
            files[file_count].owner = st.st_uid;
            files[file_count].color = get_color_code(st.st_mode);
            
            if (is_image_file(entry->d_name)) {
                files[file_count].icon_path = get_thumbnail_path(entry->d_name);
                files[file_count].is_thumbnail = true;
                files[file_count].cached_png_path = strdup(files[file_count].icon_path);
                
                struct stat thumb_st;
                if (stat(files[file_count].icon_path, &thumb_st) != 0) {
                    generate_thumbnail(entry->d_name, files[file_count].icon_path);
                }
            } else {
                files[file_count].icon_path = get_file_logo(entry->d_name, st.st_mode, st.st_uid);
                files[file_count].is_thumbnail = false;
                files[file_count].cached_png_path = get_cached_png_path(files[file_count].icon_path);
            }
            
            files[file_count].name_length = strlen(entry->d_name);
            
            if (files[file_count].name_length > max_filename_length) {
                max_filename_length = files[file_count].name_length;
            }
            
            file_count++;
        }
    }
    closedir(dir);

    cache_all_icons(files, file_count);

    size_t column_width = max_filename_length + COLUMN_PADDING;
    if (column_width < MIN_COLUMN_WIDTH) column_width = MIN_COLUMN_WIDTH;
    
    int num_columns = w.ws_col / column_width;
    if (num_columns == 0) num_columns = 1;
    
    int num_rows = (file_count + num_columns - 1) / num_columns;

    for (int row = 0; row < num_rows; row++) {
        for (int col = 0; col < num_columns; col++) {
            int index = col * num_rows + row;
            if (index < file_count) {
                go_up(1);
                if (files[index].cached_png_path) {
                    if (ensure_png_exists(files[index].icon_path, files[index].cached_png_path, files[index].is_thumbnail)) {
                        draw_png(0, 0, 4, 2, files[index].cached_png_path);
                    }
                }
                printf("%s%-*s%s", 
                       files[index].color,
                       (int)column_width, 
                       files[index].name,
                       RESET);
            }
        }
        printf("\n\n");
    }

    for (int i = 0; i < file_count; i++) {
        free(files[i].name);
        if (files[i].is_thumbnail) {
            free(files[i].icon_path);
        }
        free(files[i].cached_png_path);
    }
    free(files);
    cleanup_theme();
    return 0;
