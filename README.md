# IntrenOS

Операционная система IntrenOS, разработанная myster_gaif.

## Описание
IntrenOS - это операционная система, созданная на основе модулей из проекта pritamzope/OS. Система включает в себя следующие компоненты:

- Загрузчик (Bootloader)
- Драйверы устройств:
  - Клавиатура
  - Мышь
  - Терминал
- Ядро
- Библиотеки

## Структура проекта
```
IntrenOS/
├── src/
│   ├── bootloader/    # Загрузчик системы
│   ├── drivers/       # Драйверы устройств
│   ├── terminal/      # Терминальный интерфейс
│   ├── kernel/        # Ядро системы
│   └── lib/          # Библиотеки
└── README.md
```

## Требования для сборки
- GNU/Linux
- make
- NASM (ассемблер)
- GCC (компилятор C)
- xorriso
- grub-pc-bin
- mtools
- QEMU (для тестирования)

## Установка зависимостей
```bash
sudo apt-get install make nasm gcc grub-pc-bin mtools xorriso qemu qemu-system
```

## Установка кросскомпилятора x86_64-elf-g++
Для сборки 64-битных ELF-программ и поддержки C++ выполните:
```bash
sudo apt-get install gcc g++ make
wget https://ftp.gnu.org/gnu/binutils/binutils-2.40.tar.gz
wget https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.gz
# Распакуйте архивы
# Далее выполните (пример для Ubuntu/Debian):
sudo apt-get install build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo
mkdir -p ~/cross && cd ~/cross
# Сборка binutils
mkdir build-binutils && cd build-binutils
../../binutils-2.40/configure --target=x86_64-elf --prefix=/usr/local/cross --with-sysroot --disable-nls --disable-werror
make -j$(nproc)
sudo make install
cd ..
# Сборка gcc
mkdir build-gcc && cd build-gcc
../../gcc-13.2.0/configure --target=x86_64-elf --prefix=/usr/local/cross --disable-nls --enable-languages=c,c++ --without-headers
make all-gcc -j$(nproc)
make all-target-libgcc -j$(nproc)
sudo make install-gcc
sudo make install-target-libgcc
```
После установки кросскомпилятор будет доступен как `x86_64-elf-g++` и `x86_64-elf-gcc`.

## Сборка

1.  Убедитесь, что у вас установлены все необходимые зависимости (см. раздел "Требования для сборки").
2.  Перейдите в корневой каталог проекта `IntrenOS/`.
3.  Выполните команду `make` для сборки ядра:
    ```bash
    make
    ```
    Собранное ядро `kernel.bin` будет помещено в каталог `iso/boot/`.
4.  Выполните команду `make iso` для создания загрузочного ISO-образа:
    ```bash
    make iso
    ```
    ISO-образ `IntrenOS.iso` будет создан в корневом каталоге проекта.
5.  Для запуска ОС в эмуляторе QEMU выполните:
    ```bash
    qemu-system-i386 -cdrom IntrenOS.iso
    ```
6.  Для очистки собранных файлов выполните:
    ```bash
    make clean
    ```

## Лицензия

## Лицензия
(Информация о лицензии будет добавлена позже)
