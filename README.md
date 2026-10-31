<p align="center">
  <img src="assets/lunoryx-logo.png" alt="Lunoryx Studio" width="180" />
  <br/><strong>LUNORYX STUDIO</strong>
</p>

> **0.5.0-beta.1: игра в одном EXE.**
> Общий runtime, камера от третьего лица, ввод/HUD/счёт из Lua и встроенные ресурсы.
> Tools → Character → Game / Build: создайте пример, сохраните проект и соберите Game.exe.
> [Отчёт версии](docs/release-notes-v0.5.0-beta.1.md).


### Управление дополнениями 0.2.1

- Scene: Q — выделение, W — Move, E — Rotate, R — Scale. Local/Snap рядом.
  Тяните оси или плоскости манипулятора; Ctrl+click — мультивыбор.
- Edit → Undo/Redo объектов, либо Ctrl+Z/Ctrl+Y в viewport. История объектов
  сохраняет также геометрию и свойства; текст скриптов и звуки в эту историю не входят.
- Settings: камера, FOV, clipping, сетка, snapping, VSync, тема, master volume.
  Настройки сохраняются между запусками. Исходная тема и докинг остаются по умолчанию.
- Audio: укажите путь или перетащите WAV/WAVE/W64/RF64, MP3, FLAC, OGG/OGA в окно.
  Preview/Stop, volume, pitch, loop, autoplay, 3D position. Ресурсы хранятся по путям:
  держите звуковые файлы с проектом. Lua: `add_sound(path,name)`, `play_sound(index)`,
  `stop_sound(index)`. Индексы звуков — отдельные, с нуля.
- MCP: `Lunoryx.exe --mcp`, stdio JSON-RPC. Сервер создаёт собственную headless-сцену
  на прежнем движке; сохраните её в .lunoryx и откройте в редакторе.
  Tools: scene_list, add_object, set_transform, remove_object, project_save/load,
  export_glb, run_lua, add_sound, undo_objects, redo_objects. MCP обращается к
  стабильным ID объектов; Lua сохраняет прежние индексы. Lua по-прежнему доверенный.

```json
{"mcpServers":{"lunoryx":{"command":"C:\\Tools\\Lunoryx.exe","args":["--mcp"]}}}
```



<p align="center">
  <a href="https://github.com/kittkanev/Lunoryx/actions/workflows/tests.yml"><img src="https://github.com/kittkanev/Lunoryx/actions/workflows/tests.yml/badge.svg?branch=main" alt="Build and tests" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-11-608cff" alt="C++11" />
  <img src="https://img.shields.io/badge/Lua-5.4-75dada" alt="Lua 5.4" />
  <img src="https://img.shields.io/badge/export-GLB%202.0-a6b7ec" alt="GLB 2.0" />
</p>

<p align="center"><b>Создавай сцену кодом. Меняй её в редакторе. Экспортируй в GLB.</b></p>
<p align="center">
  <a href="#быстрый-старт">Быстрый старт</a> ·
  <a href="#lua-api">Lua API</a> ·
  <a href="#проверки-качества">Тесты</a> ·
  <a href="#архитектура">Архитектура</a>
</p>

> **Ранняя тестовая версия · v0.5.0-beta.1**
> Возможны баги, нестабильная работа и потеря несохранённых изменений.
> Выпуск предназначен для знакомства и тестирования. Сохраняйте резервные копии важных файлов.
>
> **[Скачать первый Windows-релиз](https://github.com/kittkanev/Lunoryx/releases/tag/v0.5.0-beta.1)** — самостоятельный `Lunoryx.exe`, без отдельной DLL и консоли.

**Lunoryx** (ранее Lua3D) — небольшой настольный 3D-редактор на C++ с Lua 5.4, OpenGL и Dear ImGui.
Сцена редактируется через инспектор или скрипт; `on_update(dt)` позволяет анимировать объекты без пересборки приложения.

| Создание сцены | Редактирование | Экспорт |
| :--- | :--- | :--- |
| Куб, пирамида, сфера, цилиндр, плоскость | Outliner, фильтр, трансформации, дублирование | Бинарный GLB 2.0 |
| Lua API и процедурная генерация | Докинг, консоль, пресеты материалов | Позиции, вращения, масштаб, PBR-факторы |
| Анимация через `on_update(dt)` | Орбитальная камера и wireframe | Один выровненный BIN-буфер |

## Быстрый старт

### Linux · Ubuntu / Debian

```bash
sudo apt-get update
sudo apt-get install -y build-essential liblua5.4-dev libsdl2-dev libgl1-mesa-dev

git clone https://github.com/kittkanev/Lunoryx.git
cd Lua3D
make -j2
./engine
```

Заголовки tinygltf, nlohmann/json и stb уже находятся в `win_deps/`.
Требуется графическая среда с **OpenGL 2.1 compatibility + framebuffer objects (EXT/core)** и GLSL 1.20.

### Windows · готовая сборка

Скачай **`Lunoryx-v0.5.0-beta.1-windows-x64.zip`** со страницы
[Releases](https://github.com/kittkanev/Lunoryx/releases/tag/v0.5.0-beta.1).
Запусти **`Lunoryx.exe`** напрямую. Дополнительные DLL и файлы для работы не требуются; логотип, справка и лицензии встроены в приложение. ZIP добавляет примеры и текстовые инструкции.
В архив включены пример Lua, инструкция и лицензии сторонних компонентов.

Актуальные сборки из `main` также доступны как артефакт `lua3d-windows-x64`
в [Build and tests](https://github.com/kittkanev/Lunoryx/actions/workflows/tests.yml?query=branch%3Amain).

### Windows · кросс-компиляция на Linux

```bash
sudo apt-get install -y g++-mingw-w64-x86-64 curl make
bash tools/prepare_mingw.sh
make -j2 -f Makefile.win package
```

Результат: самостоятельный `build/win/engine.exe` и тесты. Приложение линкуется со статической SDL2 и использует Windows GUI subsystem.
Скрипт загружает Lua 5.4.6 и SDL2 2.30.0, проверяет SHA-256 и собирает Lua для MinGW.
Для собственных зависимостей можно передать `LUA_DIR=... SDL_DIR=...` в `make`.

## Первая анимация

Вставь код в вкладку **Main.lua** и нажми **Run Script**:

```lua
clear_scene()
local planet = add_object('Planet', 'sphere')
set_color(planet, 0.22, 0.65, 1.0)
set_material(planet, 0.25, 0.4)

local t = 0
function on_update(dt)
    t = t + dt
    set_position(planet, math.cos(t) * 3, 1, math.sin(t) * 3)
    set_rotation(planet, 0, t * 45, 0)
end
```

Готовый пример: [`examples/orbit.lua`](examples/orbit.lua).
F5 запускает активную вкладку; F11 разворачивает редактор кода. Кнопка + Script создаёт новую вкладку. Закрытая вкладка остаётся доступной в Explorer. Пауза и продолжение анимации доступны в верхней панели. **File → Export GLB** сохраняет `export.glb` в рабочий каталог.

## Управление

| Действие | Управление |
| :--- | :--- |
| Вращение камеры | Правая кнопка мыши + перетаскивание |
| Перемещение камеры | Средняя кнопка мыши + перетаскивание |
| Приближение | Колесо мыши |
| Выбор объекта | Explorer |
| Трансформации и материал | Properties |
| Дублирование / фокус / удаление | Кнопки в Properties |
| Сетка, оси, wireframe | Меню View |

Мышь управляет камерой, когда её не перехватывают панели ImGui.

## Инструменты Studio 0.2

| Инструмент | Как использовать |
| :--- | :--- |
| Explorer | Выбор объектов и скриптов; Ctrl+click для нескольких объектов |
| Build | Примитивы, привязка к сетке и Join Selected Meshes |
| Sculpt | Выбрать объект, Subdivide, включить Surface Brush и вести ЛКМ по поверхности |
| Character | Задать пропорции и цвета, Create Character; Play Simulation → WASD / Space |
| Environment | Day / Sunset / Night, цвета неба, гравитация |
| Properties | Трансформации, материал, физика и типизированные пользовательские свойства |
| API Docs | Справка и примеры прямо внутри EXE |

**File → Save Project / Load Project** сохраняет `.lunoryx`: геометрию, свойства,
скрипты и окружение. Загруженные скрипты нужно запускать явно. Повреждённый проект
отклоняется до изменения текущей сцены. GLB служит отдельным экспортом для других приложений.

### Новые обращения из Lua

```lua
local player = add_character('Player', 2, 0.65, 0.45)
set_physics(player, true, 3, 0) -- dynamic, mass, restitution
set_property(player, 'Health', 100)
print(get_property(player, 'Health'))
set_skybox('sunset')

local clay = add_object('Clay', 'sphere')
make_editable(clay, 2)
sculpt(clay, 0, 0.5, 0, 0.8, 0.15, 0)
-- index, local x/y/z, radius, strength, mode (0 inflate, 1 smooth, 2 flatten)
local joined = merge_objects({player, clay}, 'Joined')
```

`set_velocity(index, x, y, z)` и `get_velocity(index)` управляют скоростью.
`set_character(index, true, speed, jump)` добавляет контроллер и к собственной модели. Числовые входы проверяются до изменения объекта. Примеры, диапазоны и семантика ошибок
находятся в **Window → API Docs**.

## Lua API

**Индексы начинаются с нуля.** После удаления последующие индексы сдвигаются.
Повторно вызывай `find_object(name)`, если структура сцены изменилась. Имена могут содержать до 255 байт и не обязаны быть уникальными;
поиск возвращает первое совпадение. Пустые имена допустимы.

| Функция | Результат / семантика |
| :--- | :--- |
| `add_object(name, type?)` | Новый индекс; тип по умолчанию `cube` |
| `find_object(name)` | Первый индекс или `-1` |
| `remove_object(index)` | `true` / `false` |
| `clear_scene()` | Удалить все объекты |
| `get_object_count()` | Число объектов |
| `duplicate_object(index, name?)` | Новый индекс или `-1`; копирует свойства без смещения |
| `set_name(index, name)` | `true` / `false` |
| `set_position(index, x, y, z)` | `true` / `false` |
| `get_position(index)` | `x, y, z` |
| `set_rotation(index, x, y, z)` | Углы в **градусах**; `true` / `false` |
| `get_rotation(index)` | Три угла в градусах |
| `set_scale(index, x, y, z)` | `true` / `false` |
| `get_scale(index)` | Три компонента масштаба |
| `set_color(index, r, g, b)` | Компоненты ограничиваются `[0, 1]`; `true` / `false` |
| `get_color(index)` | `r, g, b` |
| `set_material(index, metallic, roughness)` | Факторы ограничиваются `[0, 1]`; `true` / `false` |
| `get_material(index)` | `metallic, roughness` |
| `set_visible(index, value)` | Lua truthiness; `true` / `false` |
| `set_wireframe(index, value)` | Lua truthiness; `true` / `false` |
| `get_time()` | Секунды от инициализации SDL |
| `export_glb(filename)` | `true` при успешной записи |

Геттеры свойств при неверном индексе не возвращают значений. Сеттеры трансформаций, цвета и материала
проверяют все компоненты **до изменения объекта**: `NaN`, `Inf` и числа за пределами `float` вызывают Lua-ошибку.
Неподдерживаемое строковое имя меша совместимо с прежним API и выбирает куб.

### Жизненный цикл скрипта

- Новый запуск очищает предыдущий `on_update`; глобальные Lua-переменные остаются в том же состоянии.
- Ошибка `on_update` приостанавливает симуляцию и записывается в консоль.
- Вызовы движка восстанавливают высоту Lua-стека; ошибки метаметодов обрабатываются защищённо.
- На запуск и обновление действует лимит в **1 000 000 Lua-инструкций**, чтобы прерывать случайные бесконечные циклы.

Скрипты считаются доверенными: стандартные библиотеки Lua открыты. Это не песочница;
скрипт может работать с файлами и процессами, а `debug.sethook` способен изменить ограничитель инструкций.

## Проверки качества

```bash
make test                 # C++-ядро, геометрия, кватернионы, timeout
make test-integration     # Lua API, ошибки, стек и GLB round-trip
```

CI проверяет:

| Проверка | Что покрывает |
| :--- | :--- |
| Linux `g++`, `-Wall -Wextra -Werror` | Сборка редактора и тестов |
| ASan + UBSan + LeakSanitizer | Ошибки памяти, неопределённое поведение, повторное создание Lua-state |
| Khronos glTF Validator | Независимая проверка экспортированного GLB; ошибки блокируют CI |
| Xvfb + Mesa | Запуск редактора и штатное завершение по `TEST_TIMEOUT` |
| MinGW-w64 | Кросс-компиляция редактора и тестов с `-Werror` |
| Windows | Запуск собранных MinGW-тестов на Windows |

Vendored-заголовки подключаются как системные: предупреждения в собственном коде остаются ошибками сборки.
В отчёте glTF отдельно сохраняются предупреждения и информационные замечания.

Для локальной диагностики памяти:

```bash
make clean
make -j2 test-integration \
  EXTRA_CXXFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  EXTRA_LDFLAGS='-fsanitize=address,undefined'
```

### Автоматическое завершение

```bash
TEST_TIMEOUT=5 ./engine
# Без монитора, при наличии Xvfb и Mesa:
xvfb-run -a env LIBGL_ALWAYS_SOFTWARE=1 TEST_TIMEOUT=2 ./engine
```

`TEST_TIMEOUT` принимает целые десятичные секунды. Пустое, нулевое или некорректное значение отключает таймер;
слишком большое ограничивается 3 600 000 секундами. `make clean` сохраняет экспортированные GLB.

## Архитектура

| Путь | Назначение |
| :--- | :--- |
| `main.cpp` | Главный цикл, SDL/OpenGL, GLB |
| `scene_tools.h` | Скульптинг, Join, персонажи и физика |
| `editor_ui.h` / `editor_project.h` | Вкладки, Explorer и файлы проектов |
| `scene_bindings.h` / `editor_docs.h` | Lua API и встроенная справка |
| `viewport_target.h` | OpenGL framebuffer сцены |
| `scene_core.h` | Объекты, примитивы, кватернионы, timeout |
| `imgui/` | Dear ImGui и платформенные бэкенды |
| `win_deps/` | tinygltf, json и stb |
| `examples/` | Скрипты для редактора |
| `tests/` | Регрессионные и интеграционные тесты |
| `tools/` | Подготовка MinGW и валидация GLB |

## Границы текущей версии

Это развивающийся редактор, а не замена Blender или Roblox Studio.
Join соединяет геометрию и цвета; булевы CSG-операции не реализованы, а metallic/roughness
берутся у первого исходного объекта. Симуляция использует AABB-коллайдеры; персонаж пока
без скелетного рига и анимационного графа. Окружение процедурное, без импорта HDRI/cubemap.

Есть отмена последнего мазка, но пока нет общего undo/redo, текстурного и шейдерного редактора.
Viewport использует простое затенение. GLB сохраняет геометрию и материалы;
для свойств, скриптов и окружения используйте `.lunoryx`.

Тесты не заменяют испытания на целевых GPU. Графический smoke-тест работает на Linux/Mesa;
Windows CI запускает тесты и проверяет GUI subsystem и отсутствие внешних SDL/GCC DLL.

<details>
<summary><b>Зависимости и лицензии</b></summary>

- [SDL2](https://github.com/libsdl-org/SDL) — zlib.
- [Lua](https://www.lua.org/license.html) — MIT.
- [Dear ImGui](https://github.com/ocornut/imgui) — MIT.
- [tinygltf](https://github.com/syoyo/tinygltf) — MIT.
- [nlohmann/json](https://github.com/nlohmann/json) — MIT.
- [stb](https://github.com/nothings/stb) — MIT или public domain.

Лицензии сторонних компонентов действуют отдельно. Лицензия на собственный код Lunoryx
пока не указана владельцем репозитория.

</details>
