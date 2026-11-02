# Лабораторная работа №3

Текстурирование куба из предыдущей работы, вариант 1. C++20, Vulkan, GLFW и ImGUI.

Реализованы загрузка PNG с диска, UV-координаты граней, три материала с независимым выбором для куба и пола, анимированное сэмплирование через push constants, карты блеска и свечения. Для отсутствующих карт используется чёрная текстура 1×1. Геометрия и текстуры загружаются через staging-буфер в device-local память; модели используют общий динамический uniform-буфер.

## Сборка и запуск

Нужны CMake 3.20+, компилятор C++20 и Vulkan SDK с `glslc`. Зависимости GLFW, GLM, ImGUI, vk-bootstrap и VMA загружаются CMake при первой сборке. Для загрузки PNG используется [stb_image](https://github.com/nothings/stb/tree/d14d46f6b44456eb88beac58cba833a4135d4c89), включённый в `third_party` с лицензией.

```sh
cmake -S lab3 -B lab3/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build lab3/build --parallel 6
./lab3/build/vulkan-texturing-app
```

На macOS с Homebrew:

```sh
brew install cmake vulkan-loader vulkan-headers molten-vk shaderc vulkan-validationlayers
bash lab3/run-macos.sh
```

Пути к PNG и шейдерам задаются при сборке. Если переместить исходники или каталог сборки, нужно заново выполнить CMake. Не запускайте две сборки в одном каталоге одновременно.

## Управление

- `Cube material` и `Floor material` выбирают материал каждого объекта.
- `Animated sampling` включает искажение UV, две выборки альбедо и цветовую модуляцию на кубе.
- `Play`, `Speed` и `Effect strength` управляют временем и силой эффекта.
- `Specular map`, `Emissive map` и `Emission` управляют бликом и свечением. У `Tiles` обе карты чёрные; у `Metal` отсутствует свечение.
- В разделах ниже изменяются камера, преобразования куба, материал и освещение.
- `Reset scene` восстанавливает параметры. `Save image` сохраняет текущий кадр в `texturing.ppm` в рабочем каталоге.

## Проверки

```sh
ctest --test-dir lab3/build --output-on-failure
cmake -S lab3 -B lab3/build -DRENDER_TEST=ON
cmake --build lab3/build --parallel 6
./lab3/build/render_test
```

На macOS графический тест требует тех же переменных загрузчика, которые задаёт `run-macos.sh`:

```sh
export VK_ICD_FILENAMES="$(brew --prefix molten-vk)/etc/vulkan/icd.d/MoltenVK_icd.json"
export VK_LAYER_PATH="$(brew --prefix vulkan-validationlayers)/share/vulkan/explicit_layer.d"
export DYLD_LIBRARY_PATH="$(brew --prefix vulkan-validationlayers)/lib:${DYLD_LIBRARY_PATH:-}"
./lab3/build/render_test
```

Проверены UV, матрицы, настоящая отрисовка MoltenVK, выбор материалов через интерфейс, анимация и пауза, карты блеска и свечения, чёрные заглушки, device-local память, управление источниками и изменение размеров окна. В выполненном графическом тесте ошибок валидации Vulkan нет. При сборке есть предупреждения стороннего заголовка VMA.

[Отчёт PDF](report.pdf) · [Отчёт Word](report.docx)
