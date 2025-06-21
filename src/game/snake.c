#include "../include/game/snake.h"
// #include "../include/stdlib.h" // Для rand и srand - временно закомментировано
// #include "../include/time.h"   // Для инициализации генератора случайных чисел - временно закомментировано

// Declare the global exit flag (defined in kernel.c)
extern volatile int g_exit_program;

// Простейший генератор псевдослучайных чисел (LCG)
static uint32 next = 1; // Начальное значение (seed)

int my_rand() {
    next = next * 1103515245 + 12345;
    return (uint32)(next / 65536) % 32768;
}

void my_srand(unsigned int seed) {
    next = seed;
}

// Функция для генерации случайных координат яблока
static void generate_apple(SnakeGame* game) {
    BOOL on_snake;
    do {
        on_snake = FALSE;
        game->apple.x = my_rand() % GAME_WIDTH;
        game->apple.y = my_rand() % GAME_HEIGHT;
        // Проверка, не появилось ли яблоко на змейке
        for (int i = 0; i < game->snake.length; i++) {
            if (game->apple.x == game->snake.segments[i].x &&
                game->apple.y == game->snake.segments[i].y) {
                on_snake = TRUE;
                break;
            }
        }
    } while (on_snake);
}

// Инициализация игры
void snake_init(SnakeGame* game) {
    // Инициализация генератора случайных чисел (используйте текущее время, если доступно)
    my_srand(123); // Временно используем фиксированное начальное значение
    // srand(time(NULL)); // TODO: Implement time(NULL)

    game->snake.length = 3;
    game->snake.direction = 3; // Начинаем движение вправо

    // Начальное положение змейки по центру
    int start_x = GAME_WIDTH / 2;
    int start_y = GAME_HEIGHT / 2;

    for (int i = 0; i < game->snake.length; i++) {
        game->snake.segments[i].x = start_x - i;
        game->snake.segments[i].y = start_y;
    }

    game->score = 0;
    game->game_over = FALSE;

    // Расчет смещения для центрирования
    game->offset_x = (VGA_WIDTH - GAME_WIDTH) / 2;
    game->offset_y = (VGA_HEIGHT - GAME_HEIGHT) / 2;

    generate_apple(game);
}

// Отрисовка игры
void snake_draw(SnakeGame* game) {
    console_clear(COLOR_BLACK, COLOR_BLACK); // Очистить экран

    // Отрисовка рамки (желтый цвет)
    g_fore_color = COLOR_YELLOW;
    g_back_color = COLOR_BLACK;
    for (int x = 0; x < GAME_WIDTH + 2; x++) {
        console_gotoxy(game->offset_x + x, game->offset_y);
        console_putchar('#');
        console_gotoxy(game->offset_x + x, game->offset_y + GAME_HEIGHT + 1);
        console_putchar('#');
    }
    for (int y = 0; y < GAME_HEIGHT + 2; y++) {
        console_gotoxy(game->offset_x, game->offset_y + y);
        console_putchar('#');
        console_gotoxy(game->offset_x + GAME_WIDTH + 1, game->offset_y + y);
        console_putchar('#');
    }

    // Отрисовка змейки (зеленый цвет)
    g_fore_color = COLOR_GREEN;
    g_back_color = COLOR_BLACK;
    for (int i = 0; i < game->snake.length; i++) {
        console_gotoxy(game->offset_x + game->snake.segments[i].x + 1, game->offset_y + game->snake.segments[i].y + 1);
        console_putchar('O'); // Символ для сегмента змейки
    }

    // Стирание предыдущего яблока (если оно было)
    // При первой отрисовке это не нужно, но для последующих обновлений - да.
    // Простейший способ - всегда стирать, если координаты яблока известны.
    // TODO: Возможно, нужно хранить предыдущие координаты яблока для более точного стирания.
    g_fore_color = COLOR_BLACK; // Цвет фона
    g_back_color = COLOR_BLACK;
    console_gotoxy(game->offset_x + game->apple.x + 1, game->offset_y + game->apple.y + 1);
    console_putchar(' '); // Стереть предыдущее яблоко

    // Отрисовка нового яблока (красный цвет)
    g_fore_color = COLOR_RED;
    g_back_color = COLOR_BLACK;
    console_gotoxy(game->offset_x + game->apple.x + 1, game->offset_y + game->apple.y + 1);
    console_putchar('A'); // Символ для яблока

    // Отрисовка счета
    g_fore_color = COLOR_WHITE;
    g_back_color = COLOR_BLACK;
    console_gotoxy(game->offset_x, game->offset_y + GAME_HEIGHT + 2);
    console_printf("Score: %d", game->score);

    // TODO: Реализовать snake_handle_input, cmd_snake
}

// Обновление состояния игры
void snake_update(SnakeGame* game) {
    if (game->game_over) {
        return;
    }

    // Перемещение сегментов змейки
    for (int i = game->snake.length - 1; i > 0; i--) {
        game->snake.segments[i] = game->snake.segments[i-1];
    }

    // Перемещение головы змейки
    switch (game->snake.direction) {
        case 0: // Up
            game->snake.segments[0].y--;
            break;
        case 1: // Down
            game->snake.segments[0].y++;
            break;
        case 2: // Left
            game->snake.segments[0].x--;
            break;
        case 3: // Right
            game->snake.segments[0].x++;
            break;
    }

    // Проверка столкновений со стенами
    if (game->snake.segments[0].x < 0 || game->snake.segments[0].x >= GAME_WIDTH ||
        game->snake.segments[0].y < 0 || game->snake.segments[0].y >= GAME_HEIGHT) {
        game->game_over = TRUE;
        return;
    }

    // Проверка столкновений с телом змейки
    for (int i = 1; i < game->snake.length; i++) {
        if (game->snake.segments[0].x == game->snake.segments[i].x &&
            game->snake.segments[0].y == game->snake.segments[i].y) {
            game->game_over = TRUE;
            return;
        }
    }

    // Проверка поедания яблока
    if (game->snake.segments[0].x == game->apple.x &&
        game->snake.segments[0].y == game->apple.y) {
        game->score++;
        game->snake.length++; // Увеличиваем длину змейки
        // TODO: Сгенерировать новое яблоко, убедившись, что оно не появляется на змейке
        generate_apple(game);
    }
}

// Команда для запуска игры
void cmd_snake() {
    SnakeGame game;
    snake_init(&game);

    while (!game.game_over) {
        snake_draw(&game);
        
        // Обработка ввода (неблокирующая, если возможно, или с таймаутом)
        // TODO: Реализовать неблокирующий ввод или ввод с таймаутом
        // Обработка ввода (неблокирующая, если возможно, или с таймаутом)
        snake_update(&game);

        // Небольшая задержка для управления скоростью игры
        // Небольшая задержка для управления скоростью игры
        for (volatile int i = 0; i < 50000000; i++) { // Увеличена задержка для замедления игры
            // Обработка ввода (неблокирующая)
            if (g_ch != 0) {
                if (g_ch == 0x1B) { // Check for ESC
                    g_ch = 0; // Reset g_ch
                    g_exit_program = 0; // Reset exit flag (in case it was set by handler)
                    game.game_over = TRUE; // Set game over to exit main loop
                    break; // Exit delay loop
                }
                snake_handle_input(&game, g_ch); // Process other input
                g_ch = 0; // Reset g_ch after processing
            }
        }
        // No need for the second check after the loop if game_over is set inside
        // if (g_exit_program) {
        //     g_exit_program = 0;
        //     break; // Exit game loop on ESC
        // }
    }

    // Игра окончена
    console_clear(COLOR_BLACK, COLOR_BLACK);
    console_gotoxy(game.offset_x + GAME_WIDTH / 2 - 5, game.offset_y + GAME_HEIGHT / 2);
    console_putstr("Game Over!");
    console_gotoxy(game.offset_x + GAME_WIDTH / 2 - 7, game.offset_y + GAME_HEIGHT / 2 + 1);
    console_printf("Final Score: %d", game.score);

    // TODO: Вернуться в командную оболочку после нажатия клавиши
}

// Обработка ввода с клавиатуры
void snake_handle_input(SnakeGame* game, char key) {
    // TODO: Использовать правильные скан-коды или символы для стрелок
    switch (key) {
        case 'w': // Up
        case 'W':
            if (game->snake.direction != 1) game->snake.direction = 0;
            break;
        case 's': // Down
        case 'S':
            if (game->snake.direction != 0) game->snake.direction = 1;
            break;
        case 'a': // Left
        case 'A':
            if (game->snake.direction != 3) game->snake.direction = 2;
            break;
        case 'd': // Right
        case 'D':
            if (game->snake.direction != 2) game->snake.direction = 3;
            break;
        // TODO: Добавить обработку стрелок
    }
}