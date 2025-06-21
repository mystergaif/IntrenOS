#!/bin/bash

set -e

default_device="/dev/sdb"
echo "Введите путь к устройству для записи (по умолчанию: $default_device):"
read -r device
if [ -z "$device" ]; then
    device="$default_device"
fi

echo "Выбранное устройство: $device"
echo "ВНИМАНИЕ: Все данные на $device будут уничтожены! Продолжить? (yes/no)"
read -r confirm
if [ "$confirm" != "yes" ]; then
    echo "Отмена."
    exit 1
fi

# Размонтировать все разделы устройства
sudo umount ${device}?* || true

# Сборка ядра и ISO
mkdir -p iso/boot/grub
make all
make iso

# Запись ISO на устройство
sudo dd if=IntrenOS.iso of=$device bs=4M status=progress conv=fsync
sync

echo "ISO успешно записан на $device!"

echo "Создание раздела для пользовательских данных..."
# Пример: создаём второй раздел для данных (100M)
echo -e "n\n\n\n+100M\nw" | sudo fdisk $device
sudo mkfs.ext2 ${device}2

echo "Раздел для данных создан и отформатирован (${device}2)."
echo "Готово! Теперь IntrenOS сможет сохранять файлы на отдельный раздел."