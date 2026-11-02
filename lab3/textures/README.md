# Текстуры

Растровые изображения 256×256 подготовлены для этой работы. Они загружаются программой с диска через stb_image.

| Материал | Альбедо | Блеск | Свечение |
|---|---|---|---|
| Circuit | `circuit-albedo.png` | `circuit-specular.png` | `circuit-emissive.png` |
| Metal | `metal-albedo.png` | `metal-specular.png` | `black.png` |
| Tiles | `tiles-albedo.png` | `black.png` | `black.png` |

`black.png` содержит один чёрный пиксель. Альбедо и цвет свечения читаются как sRGB, маска блеска — как линейные данные. Используются линейная фильтрация и повторение текстуры. Mip-уровень один.
