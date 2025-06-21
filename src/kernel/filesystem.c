#include "filesystem.h"
#include "console.h" // Assuming console_putstr and console_printf are used
#include "string.h"  // Assuming strcpy, strcmp, strcat, strrchr, strncpy, memcpy are used
#include "types.h"   // Assuming uint32, uint8 are used
#include "ide.h"

#define FS_DISK_DRIVE 0 // Используем первый диск
#define FS_START_SECTOR 10 // С какого сектора сохранять файловую систему
#define FS_SECTOR_COUNT ((sizeof(FileEntry) * MAX_FILES + 511) / 512) // Количество секторов для всей ФС

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

    save_file_system();
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

void save_file_system() {
    int res = ide_write_sectors(FS_DISK_DRIVE, FS_SECTOR_COUNT, FS_START_SECTOR, (uint32)file_system);
    if (res != 0) {
        console_putstr("[FS] Ошибка сохранения файловой системы!\n");
    } else {
        console_putstr("[FS] Файловая система сохранена.\n");
    }
}

void load_file_system() {
    int res = ide_read_sectors(FS_DISK_DRIVE, FS_SECTOR_COUNT, FS_START_SECTOR, (uint32)file_system);
    if (res != 0) {
        console_putstr("[FS] Ошибка загрузки файловой системы! Используется новая ФС.\n");
        init_file_system();
    } else {
        console_putstr("[FS] Файловая система загружена.\n");
    }
}

void file_system_startup() {
    load_file_system();
}

// После операций создания/удаления файлов также вызывать save_file_system()
