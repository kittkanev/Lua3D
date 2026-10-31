LUNORYX STUDIO 1.0.0-rc.1 — RELEASE CANDIDATE

Запустите Lunoryx.exe. Отдельная SDL/Lua/audio DLL и консоль не нужны.
Требуются Windows x64, OpenGL 2.1 compatibility и framebuffer objects.

БЫСТРЫЙ СТАРТ
Tools > Character > Game / Build > New third-person example.
Play Simulation: WASD — движение, Space — прыжок, R — перезапуск.
Stop Simulation восстанавливает сцену редактора.
File > Save Project / Save Project As сохраняет редактируемый .lunoryx.
Build standalone Game.exe упаковывает проект и импортированные ресурсы.
Готовый Game.exe не требует редактора, компилятора или файлов проекта.

МОДЕЛИРОВАНИЕ
Tools > Build > Mesh Edit / Modifiers: вершины, рёбра, грани, Extrude,
Merge by Distance, Mirror, Array, Loop Subdivision, Bevel и булевы операции.
Примените модификаторы перед скульптингом и UV-редактированием.
UV: швы, Auto unwrap and pack, выбор/перемещение/вращение/масштаб островов.
Properties > Materials / Texture: слоты, текстуры PNG/JPEG, назначение граням.

Сцена: Q/W/E/R; Ctrl+click — мультивыбор; ПКМ — орбита; СКМ — панорама.
F5 — запуск скрипта; F11 — полный экран кода.
Edit > Undo/Redo — операции сцены. Текст скрипта имеет собственную отмену.
Window > API Docs — справка и лицензии, встроенные в приложение.

Старые проекты schema 1 читаются; новые сохраняются как schema 2.
Загрузка проекта в редактор не запускает скрипты автоматически.
Lua-скрипты доверенные и имеют доступ к файлам и процессам.

ГРАНИЦЫ
Физика AABB; статический GLB; простой освещённый viewport.
Нет скелетной анимации, нодовых систем, офлайн-рендера и полной совместимости
с Blender. Подробнее: README.md и описание релиза.

Каждый релиз содержит SHA256SUMS.txt, визуальный отчёт и снимки из тестов.
Сообщить об ошибке: https://github.com/kittkanev/Lunoryx/issues
