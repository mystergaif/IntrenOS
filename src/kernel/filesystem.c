#include "filesystem.h"
#include "console.h" // Assuming console_putstr and console_printf are used
#include "string.h"  // Assuming strcpy, strcmp, strcat, strrchr, strncpy, memcpy are used
#include "types.h"   // Assuming uint32, uint8 are used

// Simple file system
FileEntry file_system[MAX_FILES];
int file_count = 0;
char current_dir[MAX_PATH_LENGTH] = HOME_DIR;
char home_dir[MAX_PATH_LENGTH] = HOME_DIR;

// File system functions
void init_file_system() {
    // Create root directory
    strcpy(file_system[0].name, "/");
    file_system[0].is_directory = 1;
    file_system[0].size = 0;
    strcpy(file_system[0].path, "/");
    file_system[0].permissions = 0755;

    // Create home directory
    strcpy(file_system[1].name, "home");
    file_system[1].is_directory = 1;
    file_system[1].size = 0;
    strcpy(file_system[1].path, "/home");
    file_system[1].permissions = 0755;

    file_count = 2;
}

int find_file(const char* path) {
    for (int i = 0; i < file_count; i++) {
        if (strcmp(file_system[i].path, path) == 0) {
            return i;
        }
    }
    return -1;
}

void get_full_path(const char* name, char* full_path) {
    if (name[0] == '/') {
        strcpy(full_path, name);
    } else {
        strcpy(full_path, current_dir);
        if (strcmp(current_dir, "/") != 0) {
            strcat(full_path, "/");
        }
        strcat(full_path, name);
    }
}
