# Лабораторная №1 — куб

Вариант 1. Построение куба, перспективная и ортографическая проекции, перемещение, поворот и масштабирование.

Выполнены дополнительные задания: переключение проекции, управление преобразованиями, анимация с паузой и настройкой траектории, выбор цвета, процедурная окраска вершин и несколько кубов с отдельными наборами дескрипторов.

## Сборка и запуск на macOS

Нужны Xcode Command Line Tools и Homebrew.

```bash
xcode-select --install
brew install cmake vulkan-loader vulkan-headers molten-vk shaderc vulkan-validationlayers

git clone https://github.com/Couurage/computer-graphics.git
cd computer-graphics/lab1
bash run-macos.sh
```

При первой сборке CMake скачивает библиотеки. Vulkan на Mac работает через MoltenVK.

В интерфейсе можно выбрать куб и изменить его параметры. Для анимации нужно включить `Animate selected cube` и нажать `Play`. Точные значения вводятся через Command + щелчок по числу.

[Отчёт](report.pdf)

Основа — [стартовый код Владимира Бахарева](https://github.com/vladeemerr/vulkan-starter-app). В нём изменены загрузка Vulkan, обработка кадров и пересоздание swapchain; добавлено сохранение снимков.
