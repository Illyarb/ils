#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include "logo.h"
#include "logo.c"

#define RESET   "\x1B[0m"
#define RED     "\x1B[31m"
#define GREEN   "\x1B[32m"
#define BLUE    "\x1B[34m"
#define YELLOW  "\x1B[33m"
#define MAGENTA "\x1B[35m"
#define CYAN    "\x1B[36m"

#define move_cursor(X, Y) printf("\033[%d;%dH", Y, X)
#define go_up(N) printf("\033[%dA", N)
#define go_down(N) printf("\033[%dB", N)
#define go_right(N) printf("\033[%dC", N)
#define go_left(N) printf("\033[%dD", N)

#define MIN_COLUMN_WIDTH 5
#define COLUMN_PADDING 1
#define INITIAL_CAPACITY 256
#define OUTPUT_CHUNK_SIZE 8192

typedef struct {
    char* name;
    const char* color;
    const char* icon_path;
    mode_t permissions;
    uid_t owner;
    size_t name_length;
} FileEntry;

static const char* get_color_code(mode_t mode) {
    if (S_ISDIR(mode)) return BLUE;
    if (mode & S_IXUSR) return GREEN;
    return RESET;
}


static const char b64_table[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *base64_encode(const unsigned char *data, size_t len) {
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

void draw_svg(int x, int y, int col, int row, const char *svg_path) {
    char png_path[] = "/tmp/draw_svg_tmp_XXXXXX.png";
    char tmp_template[] = "/tmp/draw_svg_tmp_XXXXXX";
    int fd = mkstemp(tmp_template);
    if (fd == -1) {
        perror("mkstemp failed");
        return;
    }
    close(fd);

    snprintf(png_path, sizeof(png_path), "%s.png", tmp_template);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rsvg-convert \"%s\" -o \"%s\"", svg_path, png_path);
    if (system(cmd) != 0) {
        fprintf(stderr, "Error converting SVG to PNG\n");
        unlink(tmp_template);
        unlink(png_path);
        return;
    }

    FILE *fp = fopen(png_path, "rb");
    if (!fp) {
        perror("fopen on PNG failed");
        unlink(tmp_template);
        unlink(png_path);
        return;
    }

    fseek(fp, 0, SEEK_END);
    long png_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    unsigned char *png_data = malloc(png_size);
    if (!png_data) {
        fclose(fp);
        unlink(tmp_template);
        unlink(png_path);
        return;
    }

    if (fread(png_data, 1, png_size, fp) != (size_t)png_size) {
        free(png_data);
        fclose(fp);
        unlink(tmp_template);
        unlink(png_path);
        return;
    }
    fclose(fp);

    char *encoded = base64_encode(png_data, png_size);
    free(png_data);
    
    if (!encoded) {
        unlink(tmp_template);
        unlink(png_path);
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
    unlink(tmp_template);
    unlink(png_path);
}

int main() {
    DIR *dir;
    struct dirent *entry;
    struct winsize w;
    
    // Initialize theme
    init_theme("candy-icons");
    
    int capacity = INITIAL_CAPACITY;
    FileEntry* files = malloc(capacity * sizeof(FileEntry));
    int file_count = 0;
    size_t max_filename_length = 0;
    
    printf("\n");
    // Get terminal size
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
            
            // Store file information
            files[file_count].name = strdup(entry->d_name);
            files[file_count].permissions = st.st_mode;
            files[file_count].owner = st.st_uid;
            files[file_count].color = get_color_code(st.st_mode);
            files[file_count].icon_path = get_file_logo(entry->d_name, st.st_mode, st.st_uid);
            files[file_count].name_length = strlen(entry->d_name);
            
            if (files[file_count].name_length > max_filename_length) {
                max_filename_length = files[file_count].name_length;
            }
            
            file_count++;
        }
    }
    closedir(dir);

    // Calculate grid layout
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
                draw_svg(0, 0, 4, 2, files[index].icon_path);
                printf("%s%-*s%s", 
                       files[index].color,
                       (int)column_width, 
                       files[index].name,
                       RESET);
            }
        }
        printf("\n\n");
    }

    // Cleanup
    for (int i = 0; i < file_count; i++) {
        free(files[i].name);
    }
    free(files);
    cleanup_theme();
    return 0;
}

