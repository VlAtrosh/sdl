# 3D Renderer на SDL2


Профессиональный 3D-рендерер с поддержкой:
- Загрузки OBJ-моделей
- PBR-текстурирования
- Фонговского освещения
- Управления камерой

## 🚀 Сборка и запуск

### Зависимости
```bash
# Ubuntu/Debian
sudo apt install libsdl2-dev libsdl2-image-dev cmake

# Windows (vcpkg)
vcpkg install sdl2 sdl2-image
```

### Сборка
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Запуск
```bash
./3d-renderer
```

## 🎮 Управление
| Клавиша       | Действие                  |
|---------------|---------------------------|
| WASD          | Движение камеры           |
| +/-           | Размер модели             |
| Колесо мыши   | Приближение/отдаление     |
| F             | Wireframe-режим           |
| R             | Сброс модели              |

## 📂 Структура проекта
```
assets/       # Ресурсы (модели, текстуры)
src/          # Исходный код
cmake/        # Вспомогательные CMake-скрипты
```

![Снимок экрана (437)](https://github.com/user-attachments/assets/1f5a4efb-4596-4faa-a7c0-eac91f9b3c61)


