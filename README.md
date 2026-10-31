# Lunoryx C Studio · 0.3.0-alpha.1

Нативный 3D-редактор и игровой runtime на **C11**, с Lua 5.4 для логики игр.
Windows x64: один `Lunoryx.exe`, без консоли и дополнительных DLL.

**Ранняя тестовая версия. Возможны ошибки и потеря несохранённых изменений.**
Этот выпуск не объявляется стабильным или эквивалентом Blender / Roblox Studio.

## Запуск

Скачайте EXE из [Releases](https://github.com/kittkanev/Lunoryx/releases).
Нужен драйвер OpenGL 2.1 с GLSL и framebuffer objects. Шрифт, логотип,
документация и лицензии встроены. Настройки хранятся в пользовательском каталоге
Lunoryx/Studio, а не рядом с EXE.

## Панели и инструменты

| Панель | Реализованное поведение |
|---|---|
| Home | Select, Move, Scale, Rotate, Transform, local/world, snapping, примитивы, terrain, character, GUI, script, import, material/color, group, lock, anchor, Explorer, Properties, Toolbox, Assets |
| Model | Pivot/reset, effects, attachments, distance constraint, weld, texture, align X/Y/Z, BSP Union/Difference/Intersect, Negate, Separate, Join, sculpt |
| UI | Frame, Label, Input, Button, Image; свойства оформления, родительские элементы, вертикальная/горизонтальная раскладка, anchors, scale, min/max/aspect constraints, drag/resize на canvas |
| Script | Вкладки документов, back/forward, format, find, go to line, command/output, breakpoints, call stack, local/global watch, syntax analysis, CPU activity, run/continue/step |
| Avatar | Создание персонажа с шестью костями и анимацией ходьбы; изменение костей, пропорций через трансформации и цветов; контроллер WASD/Space |
| Plugins | Lua-инструменты через `register_tool(name, function() ... end)`; зарегистрированные кнопки появляются на панели |
| Settings | Камера, FOV/clipping, скорость и чувствительность, сетка/оси, snapping, local/world, звук, частота физики, VSync, тема, масштаб UI, autosave, доверенный Lua |

### Перемещение и сетка

- **W** — Move, **E** — Rotate, **R** — Scale, **Q** — Select.
- Перетаскивайте цветные оси. За центр или поверхность выбранного объекта
  можно двигать его в плоскости камеры. Ctrl+click — мультивыбор.
- **F** — сфокусироваться на объекте. RMB — orbit, MMB — pan, wheel — zoom.
  RMB + WASD/QE — полёт, Shift — ускорение.
- Сетка покрывает видимую плоскость мира, следует за камерой и сохраняет
  мировые координаты. Крупные деления идут через десять ячеек. При удалении
  камеры шаг автоматически увеличивается. Параметры доступны в Settings.
- Locked объекты выбираются через Explorer и защищены от манипуляторов.
- **Ctrl+Z / Ctrl+Y** — undo/redo; **Ctrl+D** — duplicate; **Delete** — delete.
  История хранит геометрию, иерархию, скрипты, GUI, материалы и окружение:
  до 64 состояний / 64 MiB, без сохранения самой истории в файл проекта.

### Моделирование и физика

Union, Difference и Intersect выполняют BSP-разрезание замкнутых треугольных
солидов с интерполяцией UV, цветов и нормалей. Это геометрические булевы
операции. Join объединяет треугольники. Исходники результата сохраняются
скрытыми дочерними объектами; Separate восстанавливает их. Negate помечает
вычитаемый операнд. Скульптинг: Inflate/Shift-Deflate, Smooth, Flatten, Subdivide.

Play использует фиксированный шаг, гравитацию, массы, отскок и трение.
Столкновения: ориентированные боксы с SAT; статические terrain/mesh/CSG имеют
треугольную узкую фазу. Есть weld и distance constraints. Stop восстанавливает
полный снимок редактора, включая изменения Lua, GUI и добавленные объекты.
Это базовый решатель: нет CCD, физики мягких тел или полноценной angular dynamics.

### Звуки

В Assets выберите файл и нажмите **Import mesh / sound**, либо перетащите файл
в окно. Создаётся Sound object. В Properties доступны Preview/Stop, autoplay,
loop, volume, pitch/speed и позиционное затухание по расстоянию.

| Формат | Расширения / кодирование |
|---|---|
| RIFF / Wave64 / RF64 | `.wav`, `.wave`, `.w64`, `.rf64` |
| MPEG audio | `.mp3` |
| FLAC | `.flac` |
| Ogg Vorbis | `.ogg`, `.oga` |
| AIFF / AIFC | `.aiff`, `.aif`, `.aifc`; PCM, sowt, float32 |
| Sun / NeXT audio | `.au`, `.snd`; PCM, float32/64 |

AAC/M4A, Opus, WMA и MIDI в этом выпуске не декодируются.

### Материалы, окружение и импорт

Cook–Torrance shading с metallic/roughness, albedo/normal textures, emission
и редактируемым GLSL fragment shader. Ошибки компиляции попадают в Output;
при ошибке используется стандартный материал. Assets / Reload images обновляет кэш.
Процедурное небо, panorama (включая HDR с тональным преобразованием), cubemap
из шести изображений. Полноценного HDR image-based lighting и shadow maps пока нет.

Импорт OBJ и glTF/GLB, включая rig, веса и первый linear/step animation clip.
Cubic-spline channels пока пропускаются. Поддерживаются внешние URI текстур;
встроенные в GLB изображения пока не извлекаются. GLB export сохраняет видимую
геометрию, UV, нормали, цвета и PBR-факторы, но не Lua, GUI, звук или анимацию.
Проект `.lunoryx` сохраняет все эти компоненты; ресурсы хранятся по файловым путям.

## Проекты и отдельная игра

Путь к проекту и Save/Load находятся в Toolbox. Есть Ctrl+S и отдельный autosave.
Загрузка проверяет данные до замены текущей сцены и не запускает код.
Проекты schema 1 из v0.2 мигрируются, включая пользовательские свойства;
новый формат — schema 2. Новые Lua API используют стабильные положительные ID,
а не индексы массива. Старые скрипты, хранящие жёсткие индексы, нужно адаптировать.

**Build standalone game** в Toolbox создаёт копию текущего EXE с проектом,
текстурами, skybox, GUI-картинками и звуками внутри. Запуск такого файла открывает
игровое окно и запускает скрипты; редакторские панели отсутствуют. Ресурсы
распаковываются в пользовательский кэш Lunoryx/Player. Формат игры соответствует
платформе, на которой выполняется сборка. На Windows получается один игровой EXE.
Esc закрывает игровое окно.

```sh
./engine --validate project.lunoryx
./engine --play project.lunoryx
./engine --build-game project.lunoryx MyGame
# Windows:
Lunoryx.exe --build-game project.lunoryx MyGame.exe
```

## Lua и отладка

```lua
local part = add_object('My part', 'cube')
set_position(part, 0, 2, 0)
set_dynamic(part, true)
set_property(part, 'Health', 100)

function on_update(dt)
    local x, y, z = get_rotation(part)
    set_rotation(part, x, y + 30 * dt, z)
end
```

**F5** run/continue, **F9** breakpoint на выбранной строке, **F10** step,
**F11** полный экран кода, **F6** Stop. Watch читает локальную переменную
при остановке или глобальную по имени; произвольные выражения не исполняются.
Analysis проверяет синтаксис Lua. Activity показывает количество вызовов и CPU time.
Format меняет только отступы; документы с long string literals оставляет без изменений.

Основные API: `add_object`, `remove_object`, `find_object`, `clear_scene`,
`get_object_count`, `duplicate_object`, `set_name`, `set_parent`,
`set/get_position`, `set/get_rotation`, `set/get_scale`, `set/get_color`,
`set/get_velocity`, `set/get_material`, `set_visible`, `set_dynamic`,
`set_anchored`, `set_texture`, `set/get_property`, `add_character`,
`set_skybox`, `add_sound`, `set_sound`, `add_gui`, `set_gui_text`, `play_sound`, `stop_sound`, `register_tool`.
Встроенная документация содержит сигнатуры.

По умолчанию Lua не получает `os`, `io`, `package`, `debug`, `dofile` и `loadfile`.
Включение Trusted Lua в настройках и новый Play открывает стандартные библиотеки.
Память ограничена 64 MiB, выполнение — 1 млн инструкций на host resume.
Это защита от ошибок, а не изоляция недоверенного нативного кода.

## MCP-сервер

Один EXE умеет запускаться как локальный MCP server по stdio:

```json
{
  "mcpServers": {
    "lunoryx": {
      "command": "C:\\Tools\\Lunoryx.exe",
      "args": ["--mcp"]
    }
  }
}
```

Сервер работает со своей headless-сценой. Для продолжения работы в редакторе
сохраните её через `project_save` и откройте проект. Это не подключение к уже
открытому окну редактора. Сокеты и внешний сервис не требуются. stdout содержит
только newline-delimited JSON-RPC. Поддерживаются protocol versions 2024-11-05,
2025-03-26, 2025-06-18.

Инструменты: `scene_list`, `scene_create`, `scene_transform`, `scene_delete`,
`scene_boolean`, `project_save`, `project_load`, `asset_import`, `asset_export`,
`script_execute`, `simulation_step`, `scene_undo`, `scene_redo`.
Ресурс: `lunoryx://scene`. Неудачные изменения сцены откатываются.

## Сборка и проверки

```sh
# Ubuntu / Debian:
sudo apt-get install gcc make pkg-config libsdl2-dev liblua5.4-dev libgl1-mesa-dev
make -j2
make test test-integration test-editor
python tools/test_mcp.py ./engine

# Windows x64 из Linux:
sudo apt-get install gcc-mingw-w64-x86-64
bash tools/prepare_mingw.sh
make -j2 -f Makefile.win package
python tools/verify_windows.py build/win/engine.exe
```

Код приложения и ядра находится в `native/`; C-зависимости — в `vendor/`.
C++/ImGui/tinygltf больше не участвуют в сборке. Исходники v0.2 доступны в Git history.
`docs/architecture.md` описывает модули и бюджеты. `docs/verification.md` — проверки.

[Сообщить об ошибке](https://github.com/kittkanev/Lunoryx/issues): версия, GPU/драйвер,
шаги воспроизведения и минимальный проект.
