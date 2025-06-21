#ifndef SNAKE_H
#define SNAKE_H

#include "../types.h"
#include "../vga.h" // Для VGA_COLOR_TYPE
#include "../console.h" // Добавлено
#include "../keyboard.h" // Добавлено

// Размеры игрового поля (в символах)
#define GAME_WIDTH 40
#define GAME_HEIGHT 20

// Максимальная длина змейки
#define MAX_SNAKE_LENGTH (GAME_WIDTH * GAME_HEIGHT)

// Структура для представления сегмента змейки или яблока
typedef struct {
    int x;
    int y;
} Point;

// Структура для представления змейки
typedef struct {
    Point segments[MAX_SNAKE_LENGTH];
    int length;
    int direction; // 0: Up, 1: Down, 2: Left, 3: Right
} Snake;

// Структура для представления игры
typedef struct {
    Snake snake;
    Point apple;
    int score;
    BOOL game_over;
    int offset_x; // Смещение для центрирования
    int offset_y; // Смещение для центрирования
} SnakeGame;

// Объявления функций
void snake_init(SnakeGame* game);
void snake_draw(SnakeGame* game);
void snake_update(SnakeGame* game);
void snake_handle_input(SnakeGame* game, char key);
void cmd_snake(); // Команда для запуска игры

#endif