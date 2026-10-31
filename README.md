<p align="center">
  <img src="assets/lunoryx-logo.png" alt="Lunoryx Studio" width="160" />
  <br/><strong>LUNORYX STUDIO</strong>
</p>

**1.0.0-rc.1 — кандидат первого стабильного выпуска.** Создавай 3D-сцены, редактируй модели, пиши Lua и собирай игру в один EXE. Сохранены прежняя тема Studio, докинг, Explorer, Properties, Tools и вкладки кода.

[Скачать Lunoryx.exe](https://github.com/kittkanev/Lunoryx/releases/download/v1.0.0-rc.1/Lunoryx.exe) · [Все файлы и визуальный отчёт](https://github.com/kittkanev/Lunoryx/releases/tag/v1.0.0-rc.1) · [Сообщить об ошибке](https://github.com/kittkanev/Lunoryx/issues)

Нужны Windows x64 и OpenGL 2.1 compatibility с framebuffer objects. Редактор работает без отдельной SDL/Lua/audio DLL и без окна консоли. ZIP добавляет примеры, инструкции и лицензии. `GemTrail.exe` в релизе — самостоятельный игровой пример.

## Возможности

| Подсистема | Что работает |
|---|---|
| Сцена | Примитивы, Explorer, мультивыбор, свойства, Move/Rotate/Scale, local/world и snapping |
| Моделирование | Вершины, рёбра, грани, Extrude, удаление, Merge by Distance, нормали, Join и скульптинг |
| Модификаторы | Недеструктивный стек Mirror, Array, Loop Subdivision, Bevel; порядок, отключение, применение |
| Булевы операции | Настоящие Union / Difference / Intersection через Manifold; исходные операнды сохраняются скрытыми |
| UV и материалы | xatlas-развёртка и упаковка, швы, острова, перемещение/вращение/масштаб UV, материалы по граням, PNG/JPEG |
| Обмен | Статический GLB: геометрия, трансформации, нормали, UV, цвета и базовые материалы; автономный экспорт с текстурами |
| Игры | Lua 5.4, AABB-физика, персонаж, камера от третьего лица, ввод, счёт, HUD, звук и перезапуск |
| Проекты | Schema 2 с чтением schema 1, атомарное сохранение, Save As, защита несохранённых изменений, история операций |
| Автоматизация | MCP stdio: сцена, стабильные ID, проекты, GLB, Lua, аудио и моделирование |

<img src="https://github.com/kittkanev/Lunoryx/releases/download/v1.0.0-rc.1/uv.png" alt="Работающий UV-редактор Lunoryx" width="900" />

## Первая игра

1. Запусти `Lunoryx.exe` и открой **Tools → Character → Game / Build**.
2. Нажми **New third-person example**. При замене несохранённого проекта появится Save / Discard / Cancel.
3. Нажми **Play Simulation**: WASD — движение, Space — прыжок, R — перезапуск. Собери три предмета.
4. **Stop Simulation** восстановит сцену, окружение, звуки и игровые настройки редактора.
5. Сохрани `.lunoryx`, укажи путь в **Game EXE path** и нажми **Build standalone Game.exe**.
6. Перенеси полученный EXE в другую папку или на другой компьютер. Исходный проект, ресурсы, редактор, Python и компилятор для запуска не нужны. Escape закрывает игру.

Для собственного проекта выбери стартовый скрипт и объект камеры. По умолчанию стартовый скрипт не назначен. В сборку входят ресурсы, импортированные в проект; произвольные обращения Lua к файлам и процессам не упаковываются автоматически.

<img src="https://github.com/kittkanev/Lunoryx/releases/download/v1.0.0-rc.1/game-windows.png" alt="Автоматически пройденный уровень в Windows" width="900" />

## Работа в редакторе

- **Scene:** Q/W/E/R — Select/Move/Rotate/Scale; ЛКМ — выбор; Ctrl+click — мультивыбор; ПКМ — орбита; СКМ — сдвиг; колесо — приближение.
- **Tools → Build → Mesh Edit / Modifiers:** включи Edit mesh, выбери вершины/рёбра/грани и используй маркеры в viewport. Операции над тяжёлой геометрией выполняются в фоне; устаревший результат отклоняется.
- **Sculpt:** сначала применить стек модификаторов; подразделить меш; рисовать ЛКМ, Shift меняет знак Inflate.
- **UV:** применить модификаторы, отметить рёбра как швы, Auto unwrap and pack. Click выбирает остров, Ctrl добавляет; drag перемещает. Доступны числовые offset/rotation/scale.
- **Properties → Materials / Texture:** создать слот, назначить PNG/JPEG и при необходимости применить слот к выбранным граням.
- **File:** Save Project, Save As, Load Project, Import static GLB и Export GLB. GLB также можно перетащить в окно.
- **Код:** + Script, F5 — Run, F11 — полный экран; закрытые вкладки доступны в Explorer. Текст имеет собственную отмену; Edit → Undo/Redo управляет операциями сцены, звука и окружения.
- **Audio:** WAV/WAVE/W64/RF64, MP3, FLAC, OGG/OGA Vorbis; preview, volume, pitch, loop, autoplay и 3D-позиция. В редактируемом проекте внешние пути относительны файлу `.lunoryx`; отсутствующий файл можно заменить.

Сохранение `.lunoryx` хранит сцену, меши, UV, материалы, модификаторы, свойства, скрипты, окружение и игровые настройки. Загрузка редакторского проекта не запускает скрипты автоматически.

## Lua и MCP

Индексы Lua начинаются с нуля и сдвигаются после удаления объекта. Повторно вызывай `find_object(name)` после изменения состава сцены. Существующие обращения `add_object`, `set_position`, `set_rotation`, `set_scale`, `set_color`, `set_material`, `set_property`, `add_sound`, `play_sound`, `export_glb` сохранены.

```lua
local player = find_object('Player')
follow_camera(player, 7, 24)
game_score(0)
hud_text('WASD / Space')
function on_update(dt)
    if input_key('R') then restart_game() end
end
```

```lua
local box = add_object('Box', 'cube')
mesh_operation(box, 'modifier', '{"type":"bevel","amount":0.08}')
mesh_operation(box, 'apply')
mesh_operation(box, 'unwrap')
```

`mesh_operation` также поддерживает `polygonize`, `extrude`, `weld`, `delete`, `normals`, `boolean`. Параметры передаются JSON-строкой. Полная справка и лицензии находятся в **Window → API Docs**. Пример игры — [examples/collect.lua](examples/collect.lua).

```json
{"mcpServers":{"lunoryx":{"command":"C:\\Tools\\Lunoryx.exe","args":["--mcp"]}}}
```

MCP обращается к объектам по стабильным ID; инструмент `mesh_operation` принимает `id`, `action`, `options`. Его boolean-опция `other` — индекс второго объекта в текущей сцене. Сервер имеет собственную headless-сцену: сохрани `.lunoryx`, чтобы открыть результат в редакторе.

Lua считается доверенным и имеет доступ к стандартным библиотекам. Ограничитель инструкций прерывает случайные бесконечные циклы; ошибки и стек обрабатываются на границе движка.

## Сборка и проверки

Linux, Debian/Ubuntu:

```bash
sudo apt-get install -y g++ make cmake curl libsdl2-dev liblua5.4-dev libgl1-mesa-dev
make -j2
./engine
make test test-integration
```

Зависимость Manifold 3.5.4 подготавливается автоматически с проверкой SHA-256. xatlas закреплён на `f700c7790aaa030e794b52ba7791a05c085faf0c` и включён в исходники.

Windows-кросс-сборка:

```bash
sudo apt-get install -y cmake g++-mingw-w64-x86-64
bash tools/prepare_mingw.sh
make -j2 -f Makefile.win package
```

Используется POSIX-вариант MinGW для фоновых задач. Собственный редактор остаётся на C++11; адаптер Manifold собирается как C++17.

Нативная упаковка из командной строки:

```text
Lunoryx.exe --create-example example.lunoryx
Lunoryx.exe --build-game example.lunoryx Game.exe
Game.exe --verify-package
```

CI проверяет Linux/MinGW с `-Werror`, нативное выполнение на Windows, ASan/UBSan/LeakSanitizer, Lua/MCP, шесть аудиоформатов из файлов и памяти, GLB валидатором Khronos, историю/совместимость проектов и графические сценарии. Игровой тест использует реальный контроллер с детерминированным вводом: удаляет исходные файлы, собирает все предметы, проверяет звук и перезапуск. Windows-графика проверяется через Mesa; этот тестовый драйвер в релиз не входит.

## Границы выпуска

Lunoryx 1.0 — игровой редактор с указанным набором моделирования. Физика использует AABB, viewport — простое освещение, импорт GLB — статический. Риг, скелетная анимация, нодовые редакторы, офлайн-рендер и полная совместимость с Blender отложены.

Bevel требует замкнутой ориентированной топологии; новые поверхности используют первый слот материала. Булев результат требует новой UV-развёртки и настройки материала. Mirror копирует весь меш относительно локального начала; подразделение использует Loop. GLB-импорт сообщает о пропущенных анимациях/морфах/нетреугольных примитивах и отклоняет обязательные неподдерживаемые расширения и sparse-accessors.

Лимиты: 10 000 объектов; 200 000 вершин на меш; 1 000 000 вершин на проект; 16 модификаторов; 64 материала на объект; 64 звука; текстуры до 4096×4096; документ проекта до 32 MiB; игровые ресурсы до 512 MiB. Проверки Windows/Mesa не охватывают все пользовательские GPU.
