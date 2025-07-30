#ifndef CONFIG_H
#define CONFIG_H

#define MAX_PATH_LENGTH 1024
#define MAX_ICON_SIZES 64
#define MAX_DIRECTORIES 64
#define INITIAL_CAPACITY 256
#define OUTPUT_CHUNK_SIZE 8192
#define MIN_COLUMN_WIDTH 5
#define COLUMN_PADDING 1

#define ICON_SIZE_16 16
#define ICON_SIZE_32 32
#define ICON_SIZE_64 64
#define DEFAULT_ICON_SIZE ICON_SIZE_64

#define ICON_BASE_PATH "/usr/share/icons"
// #define DEFAULT_THEME "breeze-dark"

// #define DEFAULT_THEME "Vortex-Dark-Icons"
// #define DEFAULT_THEME "Hey-purple"
// #define DEFAULT_THEME "Sweet-Purple"
// #define DEFAULT_THEME "Sweet-folders"
// #define DEFAULT_THEME "breeze"
// #define DEFAULT_THEME "ubuntu-mono-dark"
# define DEFAULT_THEME "candy-icons"
// # define DEFAULT_THEME "Sweet-Rainbow"
// # define DEFAULT_THEME "CRUSHED-Limes-Suru"
// # define DEFAULT_THEME "Infinity-Dark-Icons"
// # define DEFAULT_THEME "Adwaita"



#define CUSTOM_ICON_BASE_PATH "$HOME/.local/share/customLogos"
#define ICON_LEMON CUSTOM_ICON_BASE_PATH "/lemon.svg"
#define ICON_SENTIMENTS CUSTOM_ICON_BASE_PATH "/sentiments.svg"
#define ICON_MEMORY CUSTOM_ICON_BASE_PATH "/memory.svg"
#define ICON_STATE CUSTOM_ICON_BASE_PATH "/state.svg"
#define ICON_TEST CUSTOM_ICON_BASE_PATH "/test.svg"

#define CACHE_DIRECTORY_PATH ".local/share/ils/icons"

#define RESET   "\x1B[0m"
#define RED     "\x1B[31m"
#define GREEN   "\x1B[32m"
#define BLUE    "\x1B[34m"
#define YELLOW  "\x1B[33m"
#define MAGENTA "\x1B[35m"
#define CYAN    "\x1B[36m"

typedef enum {
    ICON_SIZE_SMALL = ICON_SIZE_16,
    ICON_SIZE_MEDIUM = ICON_SIZE_32,
    ICON_SIZE_LARGE = ICON_SIZE_64
} IconSize;

#endif
