#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "types.h" // Assuming types.h is needed for uint32, uint8

// File system structures
#define MAX_FILES 100
#define MAX_FILENAME 32
#define MAX_FILE_SIZE 1024
#define MAX_PATH_LENGTH 256
#define HOME_DIR "/home"

typedef struct {
    char name[MAX_FILENAME];
    char content[MAX_FILE_SIZE];
    uint32 size;
    uint8 is_directory;
    char path[MAX_PATH_LENGTH];
    uint32 permissions;  // Unix-like permissions
} FileEntry;

// Global file system variables (declared in filesystem.c)
extern FileEntry file_system[MAX_FILES];
extern int file_count;
extern char current_dir[MAX_PATH_LENGTH];
extern char home_dir[MAX_PATH_LENGTH];

// File system functions
void init_file_system();
int find_file(const char* path);
void get_full_path(const char* name, char* full_path);

#endif // FILESYSTEM_H
