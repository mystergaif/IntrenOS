#include <stdio.h>
#include <string.h>
#include "kernel.h"
#include "console.h"
#include "string.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "mouse.h" // Explicitly include mouse.h for g_status
#include "vesa.h"
#include "io_ports.h"
#include "filesystem.h" // Include the new filesystem header
#include "vga.h" // Explicitly include vga.h for color constants and VGA_WIDTH
#include "game/snake.h" // Include snake game header

// Global flag to signal program exit
volatile int g_exit_program = 0;

// Forward declaration for the mouse mode function
void enter_mouse_mode();

// RTC ports
#define RTC_PORT 0x70
#define RTC_DATA 0x71

// CMOS registers
#define RTC_SECONDS 0x00
#define RTC_MINUTES 0x02
#define RTC_HOURS 0x04
#define RTC_DAY 0x07
#define RTC_MONTH 0x08
#define RTC_YEAR 0x09

// Function to read from RTC
uint8 read_rtc(uint8 reg) {
    outportb(RTC_PORT, reg);
    return inportb(RTC_DATA);
}

// Convert BCD to binary
uint8 bcd_to_bin(uint8 bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

// Nano editor structures and defines
#define MAX_LINE_LENGTH 80
#define MAX_LINES 100
#define MAX_EDITOR_SIZE (MAX_LINE_LENGTH * MAX_LINES)

// File system structures and defines (Disk based)
#define MAX_FILES 100
#define MAX_FILENAME 32
#define MAX_PATH_LENGTH 256
#define DISK_SECTOR_SIZE 512
#define FS_METADATA_LBA DISK_STORAGE_START_LBA // LBA for file system metadata
#define FS_METADATA_SECTORS 64 // Number of sectors for file system metadata (approx 100 FileEntry structs)
#define DISK_STORAGE_START_LBA 1024 // Start writing files after the first 1024 sectors (bootloader, kernel, etc.)

typedef struct {
    char name[MAX_FILENAME];
    char path[MAX_PATH_LENGTH];
    int is_directory;
    int size; // Size in bytes
    uint32 start_lba; // Starting Logical Block Address on disk
    uint32 num_sectors; // Number of sectors occupied on disk
    int permissions; // Simple permissions (e.g., 0755)
} FileEntry;

FileEntry file_system[MAX_FILES];
int file_count = 0;
char current_dir[MAX_PATH_LENGTH] = "/";
char home_dir[MAX_PATH_LENGTH] = "/";

// Function to save the file system metadata to disk
void save_file_system_metadata() {
    // Calculate the size of the file_system array in bytes
    uint32 metadata_size = file_count * sizeof(FileEntry);
    // Calculate the number of sectors needed (round up)
    uint8 num_sectors = (metadata_size + DISK_SECTOR_SIZE - 1) / DISK_SECTOR_SIZE;

    // Ensure we don't exceed the allocated sectors
    if (num_sectors > FS_METADATA_SECTORS) {
        console_putstr("Error: File system metadata exceeds allocated space on disk!\n");
        return;
    }

    // Write the file_system array to disk
    write_to_disk(FS_METADATA_LBA, num_sectors, (uint32)file_system);
}

// Function to initialize the disk-based file system
void init_file_system() {
    // Attempt to load file system structure from disk
    uint8 read_status = ide_read_sectors(0, FS_METADATA_SECTORS, FS_METADATA_LBA, (uint32)file_system);

    // Check if loading was successful and data seems valid (e.g., file_count is reasonable)
    // A more robust check would involve a magic number or checksum
    if (read_status == 0 && file_system[0].is_directory == 1 && strcmp(file_system[0].path, "/") == 0) {
        // Assuming file_count is stored implicitly by the number of valid entries read
        // Need to determine the actual file_count from the loaded data
        file_count = 0;
        for(int i = 0; i < MAX_FILES; i++) {
            // Simple check: if path is not empty, consider it a valid entry
            if(file_system[i].path[0] != '\0') {
                file_count++;
            } else {
                break; // Stop if we find an empty entry
            }
        }
        console_putstr("File system loaded from disk.\n");
    } else {
        // If loading failed or data is invalid, initialize default file system
        console_putstr("Initializing new file system.\n");
        memset(file_system, 0, sizeof(file_system)); // Clear the array

        // Create root directory
        strcpy(file_system[0].name, "/");
        strcpy(file_system[0].path, "/");
        file_system[0].is_directory = 1;
        file_system[0].size = 0;
        file_system[0].start_lba = 0; // Root directory doesn't have data sectors
        file_system[0].num_sectors = 0;
        file_system[0].permissions = 0755;
        file_count = 1;

        // Create home directory
        strcpy(file_system[1].name, "home");
        strcpy(file_system[1].path, "/home");
        file_system[1].is_directory = 1;
        file_system[1].size = 0;
        file_system[1].start_lba = 0; // Home directory doesn't have data sectors
        file_system[1].num_sectors = 0;
        file_system[1].permissions = 0755;
        file_count = 2;

        // Save the newly initialized file system to disk
        save_file_system_metadata();
    }
    // Set initial current directory
    strcpy(current_dir, "/");
    strcpy(home_dir, "/home");
}

// Function to find a file or directory by its full path
int find_file(const char* path) {
    for (int i = 0; i < file_count; i++) {
        if (strcmp(file_system[i].path, path) == 0) {
            return i;
        }
    }
    return -1;
}

// Function to get the full path of a file or directory
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

// Disk storage functions
// TODO: Implement actual disk read/write using the IDE driver
void read_from_disk(uint32 lba, uint8 num_sectors, uint32 buffer) {
    ide_read_sectors(0, num_sectors, lba, buffer); // Assuming drive 0
}

void write_to_disk(uint32 lba, uint8 num_sectors, uint32 buffer) {
    ide_write_sectors(0, num_sectors, lba, buffer); // Assuming drive 0
}

// Nano editor structures and defines
#define MAX_LINE_LENGTH 80
#define MAX_LINES 100
#define MAX_EDITOR_SIZE (MAX_LINE_LENGTH * MAX_LINES)

typedef struct {
    char lines[MAX_LINES][MAX_LINE_LENGTH];
    int line_count;
    int cursor_x;
    int cursor_y;
    char filename[MAX_PATH_LENGTH]; // Use MAX_PATH_LENGTH for filename in editor
    int modified;
} Editor;

// Editor functions
void editor_init(Editor* editor, const char* filename) {
    memset(editor, 0, sizeof(Editor));
    strcpy(editor->filename, filename);
    editor->cursor_x = 0;
    editor->cursor_y = 0;
    editor->line_count = 1;
    editor->modified = 0;
}

void editor_load_file(Editor* editor) {
    int file_index = find_file(editor->filename);
    if (file_index == -1) {
        return; // New file
    }

    // TODO: Implement loading from disk
    // This will need to read sectors from disk based on file_system[file_index].start_lba and num_sectors
    // and then parse the content into editor lines.
    if (file_system[file_index].size > 0 && file_system[file_index].num_sectors > 0) {
        char file_content_buffer[file_system[file_index].size + 1]; // Temporary buffer
        read_from_disk(file_system[file_index].start_lba, file_system[file_index].num_sectors, (uint32)file_content_buffer);
        file_content_buffer[file_system[file_index].size] = '\0'; // Null-terminate

        int line = 0;
        int col = 0;
        int i = 0;

        while (file_content_buffer[i] != '\0' && line < MAX_LINES) {
            if (file_content_buffer[i] == '\n') {
                editor->lines[line][col] = '\0';
                line++;
                col = 0;
            } else if (col < MAX_LINE_LENGTH - 1) {
                editor->lines[line][col] = file_content_buffer[i];
                col++;
            }
            i++;
        }
        editor->lines[line][col] = '\0';
        editor->line_count = line + 1;
    } else {
        // File is empty or directory
        editor->line_count = 1;
        editor->lines[0][0] = '\0';
    }
}

void editor_save_file(Editor* editor) {
    int file_index = find_file(editor->filename);
    if (file_index == -1) {
        // Create new file
        if (file_count >= MAX_FILES) {
            console_putstr("Error: File system is full\n");
            return;
        }
        file_index = file_count;
        strcpy(file_system[file_index].name, editor->filename);
        file_system[file_index].is_directory = 0;
        // Set the full path for the new file
        get_full_path(editor->filename, file_system[file_index].path);
        file_count++;
    }

    // TODO: Implement saving to disk
    // This will need to allocate sectors on disk, write the content,
    // and update the file_system entry (size, start_lba, num_sectors).
    // For now, simulate saving by printing a message.
    console_putstr("\nSaving file to disk (simulated)...\n");
    
    // In a real implementation:
    // 1. Combine all lines from editor->lines into a single buffer.
    // 2. Calculate required sectors based on buffer size.
    // 3. If file already exists, free old sectors.
    // 4. Find free sectors on disk for the new content.
    // 5. Write buffer content to disk sectors using write_to_disk.
    // 6. Update file_system[file_index] with new size, start_lba, num_sectors.
    // 7. Save the updated file system structure to a known location on disk.

    editor->modified = 0;
}

void editor_draw_status(Editor* editor) {
    console_gotoxy(0, 24);
    console_printf("File: %s | Lines: %d | Modified: %s | Ctrl+X to exit, Ctrl+O to save",
           editor->filename,
           editor->line_count,
           editor->modified ? "Yes" : "No");
}

void editor_draw_content(Editor* editor) {
    console_clear(COLOR_WHITE, COLOR_BLACK);
    for (int i = 0; i < editor->line_count; i++) {
        console_gotoxy(0, i);
        console_putstr(editor->lines[i]);
    }
    editor_draw_status(editor);
    console_gotoxy(editor->cursor_x, editor->cursor_y);
}

void editor_insert_char(Editor* editor, char c) {
    if (editor->cursor_x >= MAX_LINE_LENGTH - 1) {
        return;
    }

    // Shift characters to make space
    for (int i = MAX_LINE_LENGTH - 2; i > editor->cursor_x; i--) {
        editor->lines[editor->cursor_y][i] = editor->lines[editor->cursor_y][i-1];
    }
    editor->lines[editor->cursor_y][editor->cursor_x] = c;
    editor->cursor_x++;
    editor->modified = 1;
}

void editor_delete_char(Editor* editor) {
    if (editor->cursor_x == 0) {
        return;
    }

    // Shift characters to remove space
    for (int i = editor->cursor_x - 1; i < MAX_LINE_LENGTH - 1; i++) {
        editor->lines[editor->cursor_y][i] = editor->lines[editor->cursor_y][i+1];
    }
    editor->cursor_x--;
    editor->modified = 1;
}

void editor_new_line(Editor* editor) {
    if (editor->line_count >= MAX_LINES) {
        return;
    }

    // Move all lines down
    for (int i = MAX_LINES - 1; i > editor->cursor_y; i--) {
        strcpy(editor->lines[i], editor->lines[i-1]);
    }

    // Split current line
    strcpy(editor->lines[editor->cursor_y + 1], &editor->lines[editor->cursor_y][editor->cursor_x]);
    editor->lines[editor->cursor_y][editor->cursor_x] = '\0';
    
    editor->cursor_y++;
    editor->cursor_x = 0;
    editor->line_count++;
    editor->modified = 1;
}

void cmd_nano(char* args) {
    if (args[0] == '\0') {
        console_putstr("Usage: nano <filename>\n");
        return;
    }

    char full_path[MAX_PATH_LENGTH];
    get_full_path(args, full_path); // Get the full path

    Editor editor;
    editor_init(&editor, full_path); // Use full path
    editor_load_file(&editor); // Use full path (editor.filename is already set)
    editor_draw_content(&editor);

    while (1) {
        char c = term_getchar();
        
        // Handle special keys
        if (c == 0x1B) { // ESC
            editor_save_file(&editor); // Save automatically on ESC
            g_exit_program = 0; // Reset exit flag (in case it was set by handler)
            break; // Exit editor loop immediately
        }
        // Handle control keys
        else if (c == 0x0F) { // Ctrl+O
            editor_save_file(&editor);
            console_putstr("\nFile saved\n");
        }
        else if (c == 0x0A) { // Enter
            editor_new_line(&editor);
        }
        else if (c == 0x7F) { // Backspace
            editor_delete_char(&editor);
        }
        else if (c >= 32 && c <= 126) { // Printable characters
            editor_insert_char(&editor, c);
        }

        editor_draw_content(&editor);
    }

    console_clear(COLOR_WHITE, COLOR_BLACK);
}

// Command functions
void cmd_help() {
    // Draw top border of the box in green
    uint8 original_fore = g_fore_color;
    uint8 original_back = g_back_color;
    g_fore_color = COLOR_BRIGHT_GREEN;
    g_back_color = COLOR_BLACK;
    for (int i = 0; i < VGA_WIDTH; i++) {
        console_putchar('=');
    }
    console_newline();

    // Print help content with green color
    console_putstr("!Commands:\n");
    console_putstr("! help     - get list of the commands\n");
    console_putstr("! clear    - clear terminal\n");
    console_putstr("! gui      - Show mouse logs\n");
    console_putstr("! echo     - Print text to screen\n");
    console_putstr("! date     - Show current date and time\n");
    console_putstr("! version  - Show OS version\n");
    console_putstr("! exit     - Exit the OS\n");
    console_putstr("! reboot   - Reboot the system\n");
    console_putstr("! ls       - List files and directories\n");
    console_putstr("! mkdir    - Create directory\n");
    console_putstr("! cd       - Change directory\n");
    console_putstr("! touch    - Create empty file\n");
    console_putstr("! cat      - Display file contents\n");
    console_putstr("! rm       - Remove file or directory\n");
    console_putstr("! pwd      - Show current directory\n");
    console_putstr("! nano     - Simple text editor\n");
    console_putstr("! snake    - Play the Snake game\n");
    console_putstr("! mouse-test - Run mouse functionality test\n");
    console_putstr("! ls -l    - List files with permissions\n");
    console_putstr("\n");

    // Draw bottom border of the box in green
    for (int i = 0; i < VGA_WIDTH; i++) {
        console_putchar('=');
    }
    console_newline();

    // Reset color to original
    g_fore_color = original_fore;
    g_back_color = original_back;
}

void cmd_echo(char* args) {
    if (args[0] == '\0') {
        console_putstr("\n");
        return;
    }
    console_putstr(args);
    console_putstr("\n");
}

void cmd_date() {
    uint8 seconds = bcd_to_bin(read_rtc(RTC_SECONDS));
    uint8 minutes = bcd_to_bin(read_rtc(RTC_MINUTES));
    uint8 hours = bcd_to_bin(read_rtc(RTC_HOURS));
    uint8 day = bcd_to_bin(read_rtc(RTC_DAY));
    uint8 month = bcd_to_bin(read_rtc(RTC_MONTH));
    uint8 year = bcd_to_bin(read_rtc(RTC_YEAR));

    // Add 2000 to year (RTC stores years since 2000)
    year += 2000;

    console_printf("Current date and time: %02d/%02d/%04d %02d:%02d:%02d\n",
           day, month, year, hours, minutes, seconds);
}

void cmd_version() {
    console_putstr("IntrenOS v0.5\n");
    console_putstr("A simple operating system for learning purposes\n");
}

void cmd_clear() {
    console_clear(COLOR_WHITE, COLOR_BLACK);
}

void cmd_exit() {
    console_putstr("Shutting down...\n");
    
    // Try to use ACPI shutdown
    outportw(0x604, 0x2000);  // Try ACPI shutdown
    
    // If ACPI shutdown fails, try keyboard controller shutdown
    outportb(0x64, 0xFE);     // System reset
    
    // If both fail, halt the CPU
        while(1) {
        __asm__("hlt");
    }
}

void cmd_reboot() {
    console_putstr("Rebooting...\n");
    
    // Try keyboard controller reset
    uint8 temp = inportb(0x64);
    while(temp & 0x02) {
        temp = inportb(0x64);
    }
    outportb(0x64, 0xFE);
    
    // If keyboard controller reset fails, try triple fault
    __asm__("int $0x19");  // BIOS reboot
    
    // If all fails, halt the CPU
    while(1) {
        __asm__("hlt");
    }
}

void draw_pseudo_gui() {
    // Нарисовать простую рамку и надпись в центре экрана
    int x0 = 20, y0 = 5, w = 40, h = 10;
    for (int x = x0; x < x0 + w; ++x) {
        console_gotoxy(x, y0); console_putchar('-');
        console_gotoxy(x, y0 + h - 1); console_putchar('-');
    }
    for (int y = y0; y < y0 + h; ++y) {
        console_gotoxy(x0, y); console_putchar('|');
        console_gotoxy(x0 + w - 1, y); console_putchar('|');
    }
    console_gotoxy(x0, y0); console_putchar('+');
    console_gotoxy(x0 + w - 1, y0); console_putchar('+');
    console_gotoxy(x0, y0 + h - 1); console_putchar('+');
    console_gotoxy(x0 + w - 1, y0 + h - 1); console_putchar('+');
    // Надпись
    console_gotoxy(x0 + 10, y0 + h / 2);
    console_putstr("[ PSEUDO GUI WINDOW ]");
}

void cmd_ls() {
    console_printf("Contents of %s:\n", current_dir);
    for (int i = 0; i < file_count; i++) {
        // Check if file is in current directory
        char* last_slash = strrchr(file_system[i].path, '/');
        char dir_path[MAX_PATH_LENGTH];
        
        if (last_slash) {
            strncpy(dir_path, file_system[i].path, last_slash - file_system[i].path);
            dir_path[last_slash - file_system[i].path] = '\0';
        } else {
            // Handle root directory case
            dir_path[0] = '\0';
        }

        // Special case for root directory listing
        if (strcmp(current_dir, "/") == 0 && last_slash == NULL) {
             if (file_system[i].is_directory) {
                console_printf("[DIR] %s\n", file_system[i].name);
            } else {
                console_printf("%s (%d bytes)\n", file_system[i].name, file_system[i].size);
            }
        } else if (strcmp(dir_path, current_dir) == 0 && last_slash != NULL) {
            if (file_system[i].is_directory) {
                console_printf("[DIR] %s\n", last_slash + 1);
            } else {
                console_printf("%s (%d bytes)\n", last_slash + 1, file_system[i].size);
            }
        }
    }
}

void cmd_mkdir(char* args) {
    if (args[0] == '\0') {
        console_putstr("Usage: mkdir <directory_name>\n");
        return;
    }

    if (file_count >= MAX_FILES) {
        console_putstr("Error: File system is full\n");
        return;
    }

    char full_path[MAX_PATH_LENGTH];
    get_full_path(args, full_path);

    if (find_file(full_path) != -1) {
        console_putstr("Error: File or directory already exists\n");
        return;
    }

    strcpy(file_system[file_count].name, args);
    file_system[file_count].is_directory = 1;
    file_system[file_count].size = 0;
    strcpy(file_system[file_count].path, full_path);
    file_system[file_count].permissions = 0755;
    file_system[file_count].start_lba = 0; // Directories don't have data sectors
    file_system[file_count].num_sectors = 0;
    file_count++;
    console_printf("Directory '%s' created\n", args);
}

void cmd_cd(char* args) {
    if (args[0] == '\0') {
        strcpy(current_dir, home_dir);
        return;
    }

    char full_path[MAX_PATH_LENGTH];
    get_full_path(args, full_path);

    int file_index = find_file(full_path);
    if (file_index == -1) {
        console_putstr("Error: Directory not found\n");
        return;
    }

    if (!file_system[file_index].is_directory) {
        console_putstr("Error: Not a directory\n");
        return;
    }

    strcpy(current_dir, full_path);
    console_printf("Changed directory to %s\n", current_dir);
}

void cmd_touch(char* args) {
    if (args[0] == '\0') {
        console_putstr("Usage: touch <filename>\n");
        return;
    }

    if (file_count >= MAX_FILES) {
        console_putstr("Error: File system is full\n");
        return;
    }

    char full_path[MAX_PATH_LENGTH];
    get_full_path(args, full_path);

    if (find_file(full_path) != -1) {
        console_putstr("Error: File already exists\n");
        return;
    }

    strcpy(file_system[file_count].name, args);
    file_system[file_count].is_directory = 0;
    file_system[file_count].size = 0;
    strcpy(file_system[file_count].path, full_path);
    file_system[file_count].permissions = 0644;
    file_system[file_count].start_lba = 0; // New files are empty, no sectors allocated yet
    file_system[file_count].num_sectors = 0;
    file_count++;
    console_printf("File '%s' created\n", args);
}

void cmd_cat(char* args) {
    if (args[0] == '\0') {
        console_putstr("Usage: cat <filename>\n");
        return;
    }

    char full_path[MAX_PATH_LENGTH];
    get_full_path(args, full_path);

    int file_index = find_file(full_path);
    if (file_index == -1) {
        console_putstr("Error: File not found\n");
        return;
    }

    if (file_system[file_index].is_directory) {
        console_putstr("Error: Cannot display directory contents\n");
        return;
    }

    if (file_system[file_index].size == 0) {
        console_putstr("File is empty\n");
        return;
    }

    // TODO: Implement reading file content from disk
    // For now, simulate reading by printing a message and size
    console_printf("Reading file '%s' (%d bytes) from disk (simulated)...\n", file_system[file_index].name, file_system[file_index].size);
    // In a real implementation:
    // 1. Allocate a buffer for the file content.
    // 2. Read sectors from disk based on file_system[file_index].start_lba and num_sectors using read_from_disk.
    // 3. Print the content of the buffer.
}

void cmd_rm(char* args) {
    if (args[0] == '\0') {
        console_putstr("Usage: rm <filename or directory>\n");
        return;
    }

    char full_path[MAX_PATH_LENGTH];
    get_full_path(args, full_path);

    int file_index = find_file(full_path);
    if (file_index == -1) {
        console_putstr("Error: File or directory not found\n");
        return;
    }

    // Don't allow removing root or home directories
    if (strcmp(full_path, "/") == 0 || strcmp(full_path, "/home") == 0) {
        console_putstr("Error: Cannot remove system directories\n");
        return;
    }

    // TODO: Implement freeing disk sectors
    // For now, just remove the entry from the in-memory representation
    // In a real implementation:
    // 1. Free the disk sectors allocated to the file/directory.
    // 2. Update the file system structure on disk.

    // Move the last file to the deleted position
    file_count--;
    if (file_index != file_count) {
        memcpy(&file_system[file_index], &file_system[file_count], sizeof(FileEntry));
    }

    console_printf("Removed '%s'\n", args);
    save_file_system_metadata(); // Save changes to disk
}

void cmd_pwd() {
    console_printf("Current directory: %s\n", current_dir);
}

// Add new command to show file permissions
void cmd_ls_l() {
    console_printf("Contents of %s:\n", current_dir);
    for (int i = 0; i < file_count; i++) {
        char* last_slash = strrchr(file_system[i].path, '/');
        char dir_path[MAX_PATH_LENGTH];

        if (last_slash) {
            strncpy(dir_path, file_system[i].path, last_slash - file_system[i].path);
            dir_path[last_slash - file_system[i].path] = '\0';
        } else {
            // Handle root directory case
            dir_path[0] = '\0';
        }

        // Special case for root directory listing
        if (strcmp(current_dir, "/") == 0 && last_slash == NULL) {
            // Convert permissions to string
            char perms[10];
            sprintf(perms, "%c%c%c%c%c%c%c%c%c",
                file_system[i].is_directory ? 'd' : '-',
                (file_system[i].permissions & 0400) ? 'r' : '-',
                (file_system[i].permissions & 0200) ? 'w' : '-',
                (file_system[i].permissions & 0100) ? 'x' : '-',
                (file_system[i].permissions & 0040) ? 'r' : '-',
                (file_system[i].permissions & 0020) ? 'w' : '-',
                (file_system[i].permissions & 0010) ? 'x' : '-',
                (file_system[i].permissions & 0004) ? 'r' : '-',
                (file_system[i].permissions & 0002) ? 'w' : '-',
                (file_system[i].permissions & 0001) ? 'x' : '-');
            
            console_printf("%s %d %s\n", perms, file_system[i].size, file_system[i].name);
        } else if (strcmp(dir_path, current_dir) == 0 && last_slash != NULL) {
            // Convert permissions to string
            char perms[10];
            sprintf(perms, "%c%c%c%c%c%c%c%c%c",
                file_system[i].is_directory ? 'd' : '-',
                (file_system[i].permissions & 0400) ? 'r' : '-',
                (file_system[i].permissions & 0200) ? 'w' : '-',
                (file_system[i].permissions & 0100) ? 'x' : '-',
                (file_system[i].permissions & 0040) ? 'r' : '-',
                (file_system[i].permissions & 0020) ? 'w' : '-',
                (file_system[i].permissions & 0010) ? 'x' : '-',
                (file_system[i].permissions & 0004) ? 'r' : '-',
                (file_system[i].permissions & 0002) ? 'w' : '-',
                (file_system[i].permissions & 0001) ? 'x' : '-');
            
            console_printf("%s %d %s\n", perms, file_system[i].size, last_slash + 1);
        }
    }
}

// Mouse test structures
#define MOUSE_TEST_DURATION 8  // seconds
#define MOUSE_TEST_INTERVAL 1  // seconds
#define MOUSE_TEST_THRESHOLD 5 // minimum movements per second

typedef struct {
    uint32 timestamp;
    int x;
    int y;
    uint8 buttons;
    uint8 is_movement;
} MouseEvent;

typedef struct {
    MouseEvent events[1000];  // Store last 1000 events
    int event_count;
    int movement_count;
    int button_press_count;
    uint32 last_movement_time;
    uint32 test_start_time;
    uint8 test_running;
} MouseTest;

MouseTest mouse_test;

// Disk storage structures
#define DISK_SECTOR_SIZE 512
#define DISK_STORAGE_START_LBA 1024 // Start writing files after the first 1024 sectors (bootloader, kernel, etc.)

// File system structures and defines (Disk based)
// #define MAX_FILES 100 // Already defined
// #define MAX_FILENAME 32 // Already defined
// #define MAX_PATH_LENGTH 256 // Already defined

typedef struct {
    char name[MAX_FILENAME];
    char path[MAX_PATH_LENGTH];
    int is_directory;
    int size; // Size in bytes
    uint32 start_lba; // Starting Logical Block Address on disk
    uint32 num_sectors; // Number of sectors occupied on disk
    int permissions; // Simple permissions (e.g., 0755)
} FileEntry_Disk;

// Use disk-based file system
FileEntry_Disk file_system_disk[MAX_FILES];
int file_count_disk = 0;
char current_dir_disk[MAX_PATH_LENGTH] = "/";
char home_dir_disk[MAX_PATH_LENGTH] = "/";

// Function to initialize the disk-based file system
void init_file_system_disk() {
    // TODO: Implement loading file system structure from disk
    // For now, create root and home directories in memory representation
    strcpy(file_system_disk[0].name, "/");
    strcpy(file_system_disk[0].path, "/");
    file_system_disk[0].is_directory = 1;
    file_system_disk[0].size = 0;
    file_system_disk[0].start_lba = 0; // Root directory doesn't have data sectors
    file_system_disk[0].num_sectors = 0;
    file_system_disk[0].permissions = 0755;
    file_count_disk = 1;

    strcpy(file_system_disk[1].name, "home");
    strcpy(file_system_disk[1].path, "/home");
    file_system_disk[1].is_directory = 1;
    file_system_disk[1].size = 0;
    file_system_disk[1].start_lba = 0; // Home directory doesn't have data sectors
    file_system_disk[1].num_sectors = 0;
    file_system_disk[1].permissions = 0755;
    file_count_disk = 2;
}

// Function to find a file or directory by its full path (Disk based)
int find_file_disk(const char* path) {
    for (int i = 0; i < file_count_disk; i++) {
        if (strcmp(file_system_disk[i].path, path) == 0) {
            return i;
        }
    }
    return -1;
}

// Function to get the full path of a file or directory (Disk based)
void get_full_path_disk(const char* name, char* full_path) {
    if (name[0] == '/') {
        strcpy(full_path, name);
    } else {
        strcpy(full_path, current_dir_disk);
        if (strcmp(current_dir_disk, "/") != 0) {
            strcat(full_path, "/");
        }
        strcat(full_path, name);
    }
}

// USB storage structures
#define USB_SECTOR_SIZE 512
#define USB_STORAGE_START 0x100000  // 1MB offset in memory

typedef struct {
    char magic[4];  // "MOUS"
    uint32 timestamp;
    uint32 event_count;
    uint32 movement_count;
    uint32 button_press_count;
    uint32 test_duration;
    uint8 test_result;  // 0 = failed, 1 = passed
} MouseTestHeader;

// Mouse test functions
void init_mouse_test() {
    memset(&mouse_test, 0, sizeof(MouseTest));
    mouse_test.test_running = 0;
}

void record_mouse_event(int x, int y, uint8 buttons, uint8 is_movement) {
    if (!mouse_test.test_running) return;

    uint32 current_time = 0; // TODO: Implement proper time function
    
    // Record event
    if (mouse_test.event_count < 1000) {
        mouse_test.events[mouse_test.event_count].timestamp = current_time;
        mouse_test.events[mouse_test.event_count].x = x;
        mouse_test.events[mouse_test.event_count].y = y;
        mouse_test.events[mouse_test.event_count].buttons = buttons;
        mouse_test.events[mouse_test.event_count].is_movement = is_movement;
        mouse_test.event_count++;
    }

    // Update counters
    if (is_movement) {
        mouse_test.movement_count++;
        mouse_test.last_movement_time = current_time;
    }
    if (buttons) {
        mouse_test.button_press_count++;
    }
}

uint8 check_mouse_test() {
    if (!mouse_test.test_running) return 0;

    uint32 current_time = 0; // TODO: Implement proper time function
    uint32 test_duration = current_time - mouse_test.test_start_time;

    // Check if test duration exceeded
    if (test_duration >= MOUSE_TEST_DURATION) {
        mouse_test.test_running = 0;
        return 1;
    }

    // Check for no movement for too long
    if (current_time - mouse_test.last_movement_time > MOUSE_TEST_INTERVAL) {
        console_putstr("Warning: No mouse movement detected for more than 1 second\n");
    }

    return 0;
}

// Disk storage functions
// TODO: Implement actual disk read/write using the IDE driver
void read_from_disk(uint32 lba, uint8 num_sectors, uint32 buffer) {
    // For now, simulate reading from memory (replace with IDE driver calls)
    // memcpy((void*)buffer, (void*)(DISK_STORAGE_START_LBA * DISK_SECTOR_SIZE + lba * DISK_SECTOR_SIZE), num_sectors * DISK_SECTOR_SIZE);
    ide_read_sectors(0, num_sectors, lba, buffer); // Assuming drive 0
}

void write_to_disk(uint32 lba, uint8 num_sectors, uint32 buffer) {
    // For now, simulate writing to memory (replace with IDE driver calls)
    // memcpy((void*)(DISK_STORAGE_START_LBA * DISK_SECTOR_SIZE + lba * DISK_SECTOR_SIZE), (void*)buffer, num_sectors * DISK_SECTOR_SIZE);
    ide_write_sectors(0, num_sectors, lba, buffer); // Assuming drive 0
}

// USB storage functions
void write_to_usb(void* data, uint32 size) {
    // TODO: Implement proper USB storage write
    // For now, just write to memory
    memcpy((void*)USB_STORAGE_START, data, size);
}

void save_mouse_test_results() {
    MouseTestHeader header;
    strcpy(header.magic, "MOUS");
    header.timestamp = 0; // TODO: Get current time
    header.event_count = mouse_test.event_count;
    header.movement_count = mouse_test.movement_count;
    header.button_press_count = mouse_test.button_press_count;
    header.test_duration = MOUSE_TEST_DURATION;
    header.test_result = (mouse_test.movement_count >= MOUSE_TEST_THRESHOLD * MOUSE_TEST_DURATION);

    // Write header
    write_to_usb(&header, sizeof(MouseTestHeader));

    // Write events
    write_to_usb(mouse_test.events, mouse_test.event_count * sizeof(MouseEvent));
}

// Add mouse test command
void cmd_mouse_test() {
    console_clear(COLOR_WHITE, COLOR_BLACK);
    console_putstr("Starting mouse test...\n");
    console_putstr("Please move the mouse and click buttons for 8 seconds.\n");
    console_putstr("Test will fail if there is no movement for more than 1 second.\n\n");

    init_mouse_test();
    mouse_test.test_running = 1;
    mouse_test.test_start_time = 0; // TODO: Get current time

    // Wait for test to complete
    while (mouse_test.test_running) {
        // Sleep for a short time
        for (volatile int i = 0; i < 1000000; i++);
    }

    // Display results
    console_putstr("\nMouse Test Results:\n");
    console_putstr("-------------------\n");
    console_printf("Total movements: %d\n", mouse_test.movement_count);
    console_printf("Total button presses: %d\n", mouse_test.button_press_count);
    console_printf("Test duration: %d seconds\n", MOUSE_TEST_DURATION);
    console_printf("Test result: %s\n", 
           (mouse_test.movement_count >= MOUSE_TEST_THRESHOLD * MOUSE_TEST_DURATION) ? 
           "PASSED" : "FAILED");
}

// Global variables for console colors (defined in console.c or vga.c)
extern uint8 g_fore_color;
extern uint8 g_back_color;

// Global variable for mouse status (defined in mouse.c)
extern MOUSE_STATUS g_status;

// VGA constants (defined in vga.h)
extern const uint8 COLOR_WHITE;
extern const uint8 COLOR_BLACK;
extern const uint8 COLOR_BRIGHT_GREEN;
extern const int VGA_WIDTH;

void kmain() {
    gdt_init();
    idt_init();
    
    // Initialize PIC
    outportb(0x20, 0x11);  // ICW1: start initialization sequence
    outportb(0xA0, 0x11);
    outportb(0x21, 0x20);  // ICW2: remap IRQ table
    outportb(0xA1, 0x28);
    outportb(0x21, 0x04);  // ICW3: tell Master PIC about Slave
    outportb(0xA1, 0x02);
    outportb(0x21, 0x01);  // ICW4: set 8086 mode
    outportb(0xA1, 0x01);
    
    // Mask all interrupts except keyboard and mouse
    outportb(0x21, 0xFC);  // Enable IRQ0 (timer) and IRQ1 (keyboard)
    outportb(0xA1, 0xFF);  // Disable all slave IRQs
    
    // Enable interrupts
    asm volatile("sti");
    
    console_init(COLOR_WHITE, COLOR_BLACK); // Initial console setup
    vga_disable_cursor();
    keyboard_init();
    mouse_init();
    ata_init(); // Initialize the ATA driver
    init_file_system(); // Initialize the disk-based file system (renamed function)

    console_putstr("Welcome to IntrenOS!\n");
    console_putstr("Type 'help' for available commands.\n\n");

    char command[256];
    char args[256];
    while(1) {
        // Set foreground color to white for prompt and user input
        g_fore_color = COLOR_WHITE;
        g_back_color = COLOR_BLACK; // Ensure background is black
        console_putstr("IntrenOS> ");
        getstr(command);
        
        // Reset foreground color to white for output
        g_fore_color = COLOR_WHITE;
        g_back_color = COLOR_BLACK; // Ensure background is black

        // Split command and arguments
        int i = 0;
        while (command[i] != ' ' && command[i] != '\0') {
            i++;
        }
        if (command[i] == ' ') {
            command[i] = '\0';
            strcpy(args, &command[i + 1]);
        } else {
            args[0] = '\0';
        }

        if (strcmp(command, "help") == 0) {
            cmd_help();
        } else if (strcmp(command, "clear") == 0) {
            cmd_clear();
        } else if (strcmp(command, "gui") == 0) {
            enter_mouse_mode();
        } else if (strcmp(command, "echo") == 0) {
            cmd_echo(args);
        } else if (strcmp(command, "date") == 0) {
            cmd_date();
        } else if (strcmp(command, "version") == 0) {
            cmd_version();
        } else if (strcmp(command, "exit") == 0) {
            cmd_exit();
        } else if (strcmp(command, "reboot") == 0) {
            cmd_reboot();
        } else if (strcmp(command, "ls") == 0) {
            cmd_ls();
        } else if (strcmp(command, "mkdir") == 0) {
            cmd_mkdir(args);
        } else if (strcmp(command, "cd") == 0) {
            cmd_cd(args);
        } else if (strcmp(command, "touch") == 0) {
            cmd_touch(args);
        } else if (strcmp(command, "cat") == 0) {
            cmd_cat(args);
        } else if (strcmp(command, "rm") == 0) {
            cmd_rm(args);
        } else if (strcmp(command, "pwd") == 0) {
            cmd_pwd();
        } else if (strcmp(command, "nano") == 0) {
            cmd_nano(args);
        } else if (strcmp(command, "snake") == 0) { // Добавлена команда snake
            cmd_snake();
        } else if (strcmp(command, "mouse-test") == 0) {
            cmd_mouse_test();
        } else if (strcmp(command, "ls") == 0 && strcmp(args, "-l") == 0) {
            cmd_ls_l();
        } else {
            console_putstr("Unknown command: ");
            console_putstr(command);
            console_putstr("\nType 'help' for available commands.\n");
        }
    }
}

void enter_mouse_mode() {
    console_clear(COLOR_WHITE, COLOR_BLACK);
    console_putstr("Mouse Log Mode - Press any key to exit\n");
    int mouse_x = 0;
    int mouse_y = 0;

    while(1) {
        mouse_x = mouse_getx();
        mouse_y = mouse_gety();

        console_gotoxy(0, 0);
        console_printf("Mouse Position: X:%d Y:%d\n", mouse_x, mouse_y);
        console_printf("Mouse Buttons:\n");
        console_printf("Left: %s\n", g_status.left_button ? "Pressed" : "Released");
        console_printf("Right: %s\n", g_status.right_button ? "Pressed" : "Released");
        console_printf("Middle: %s\n", g_status.middle_button ? "Pressed" : "Released");
        
        // Check if any key is pressed to exit
        if (inportb(0x64) & 1) { // Check if keyboard buffer is not empty
            uint8 scancode = inportb(0x60); // Read scancode
            if (scancode == 0x01) { // Check if it's the ESC key
                g_exit_program = 0; // Reset exit flag (in case it was set by handler)
                break; // Exit mouse mode on ESC
            }
            // Keep original exit condition for other keys if needed, or remove it
            // if (scancode != 0) { // Exit on any other key press
            //     break;
            // }
        }
    }
    console_clear(COLOR_WHITE, COLOR_BLACK);
}
