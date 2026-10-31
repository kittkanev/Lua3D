# Lunoryx C Studio · v0.3.0-alpha.1

**Ранняя тестовая версия. Возможны баги и потеря несохранённых изменений.**

Новая C11-реализация редактора и игрового runtime. Один Windows x64 EXE,
без консоли, SDL2.dll и Lua DLL. Шрифты, справка и лицензии внутри.

- Панели Home / Avatar / UI / Script / Model / Plugins с рабочими командами.
- Перемещение по осям и в плоскости камеры, rotate/scale/pivot, snapping,
  local/world, мультивыбор, Focus. Блокировка защищает от перетаскивания.
- Сетка по видимой плоскости мира с адаптивными делениями, без маленькой площадки.
- BSP Union/Difference/Intersect, Negate/Separate, Join, скульптинг и история Undo/Redo.
- GUI canvas: Frame/Label/Input/Button/Image, layout, anchors и constraints.
- Lua breakpoints, continue/step, call stack, local/global watch, syntax analysis и CPU activity.
- Скелетный персонаж, walk cycle, импорт первого linear/step animation clip.
- WAV/Wave64/RF64, MP3, FLAC, Ogg Vorbis, PCM/float AIFF/AU; 3D sound, loop, pitch и preview.
- PBR-факторы, albedo/normal textures, fragment shaders, panorama/HDR и cubemap.
- Сохраняемые настройки камеры, сетки, snapping, звука, физики, интерфейса и autosave.
- MCP stdio: `Lunoryx.exe --mcp`, 13 инструментов и ресурс сцены.
- Build standalone game: проект и ресурсы включаются в отдельный игровой EXE.
- Миграция проектов schema 1 из v0.2; schema 2 сохраняет новые компоненты.

MCP работает со своей headless-сценой, не с уже открытым окном. Для переноса
используйте project_save/Load. Физика — базовый OBB/SAT и static triangle solver,
без CCD/полной angular dynamics. HDR отображается с тональным преобразованием;
нет HDR IBL и shadow maps. Встроенные в GLB изображения не извлекаются; экспорт
GLB не переносит анимацию, скрипты, GUI и звук — используйте проект или игровой EXE.
AAC/M4A/Opus/WMA/MIDI и форматы проектов Blender/Roblox не поддерживаются.

Список возможностей, управление и конфигурация MCP: README и встроенные Docs.
