# Лабораторная №1 — куб

Вариант 1. Основы 3D-графики: построение куба, проекции и аффинные преобразования.

## Что реализовано

- Перспективная и ортографическая проекции.
- Перемещение, поворот и масштаб по трём осям.
- Движение и вращение по замкнутой пространственной траектории.
- Пауза, продолжение и перезапуск анимации, настройка скорости, радиуса, высоты и фазы.
- Выбор цвета куба и процедурные цвета вершин.
- До трёх кубов с отдельными uniform-буферами и наборами дескрипторов.

## Запуск на macOS

Нужны инструменты командной строки Xcode и Homebrew. Если компилятор ещё не установлен:

```bash
xcode-select --install
```

Установка зависимостей:

```bash
brew install cmake vulkan-loader vulkan-headers molten-vk shaderc vulkan-validationlayers
```

Сборка и запуск:

```bash
git clone https://github.com/Couurage/computer-graphics.git
cd computer-graphics/lab1
bash run-macos.sh
```

При первой сборке CMake скачает GLFW, ImGUI, GLM, vk-bootstrap и Vulkan Memory Allocator. На macOS Vulkan работает через MoltenVK поверх Metal. Скрипт настраивает пути к драйверу и слою валидации только для запускаемого процесса.

## Управление

`Perspective` переключает проекцию. `Field of view` меняет угол обзора, `View size` — размер ортографического объёма. `Camera distance` задаёт расстояние камеры.

`Cubes` задаёт число объектов. Кнопки `Cube 1`, `Cube 2`, `Cube 3` выбирают объект, к которому относятся положение, поворот, масштаб, цвет и параметры траектории. Точные значения можно вводить через Command + щелчок по числу на macOS (Ctrl + щелчок на Windows/Linux).

Для анимации нужно включить `Animate selected cube`, затем нажать `Play`. `Pause` останавливает время, `Restart` возвращает его к нулю. Скорость общая, а радиус, высота и фаза задаются отдельно каждому кубу. `Reset all` восстанавливает исходную сцену.

При включённом `Vertex colors` выбранный цвет умножается на цвет каждой вершины. Без него куб одноцветный.

`Save image` сохраняет текущий кадр в `cube.ppm` в папке лабы. На macOS его можно преобразовать в PNG:

```bash
sips -s format png cube.ppm --out cube.png
```

## Файлы

- `source/application.cpp` — ресурсы Vulkan, интерфейс и отрисовка.
- `source/scene.cpp` — вершины, индексы и матрицы преобразований.
- `shaders/` — вершинный и фрагментный шейдеры.
- `source/graphics_internal.cpp`, `source/main.cpp` — инициализация, окно и цикл кадров.
- [Отчёт](report.pdf).
- [Подготовка к защите](defense.md).

## Проверка

```bash
ctest --test-dir build --output-on-failure
```

Тесты проверяют ориентацию граней, объём куба, преобразования, диапазон глубины Vulkan и время анимации.

Дополнительная проверка с настоящим окном и Vulkan:

```bash
cmake -S . -B build -DRENDER_TEST=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build --parallel 6
export VK_ICD_FILENAMES="$(brew --prefix molten-vk)/etc/vulkan/icd.d/MoltenVK_icd.json"
export VK_LAYER_PATH="$(brew --prefix vulkan-validationlayers)/share/vulkan/explicit_layer.d"
export DYLD_LIBRARY_PATH="$(brew --prefix vulkan-validationlayers)/lib:${DYLD_LIBRARY_PATH:-}"
./build/render_test
```

Этот тест нажимает элементы ImGUI, вводит значения преобразований, проверяет анимацию и изменение размера окна, сохраняет кадры в `screenshots/`.

## Основа

Использован [vulkan-starter-app](https://github.com/vladeemerr/vulkan-starter-app) Владимира Бахарева. Добавлены сцена, шейдеры, интерфейс, тесты и запуск на macOS. В стартовых файлах изменены загрузка Vulkan, обработка кадров, пересоздание swapchain и сохранение снимков. Исходные LICENSE и NOTICE сохранены.
