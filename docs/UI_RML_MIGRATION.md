# LOCUS — переход меню на RmlUi

**Цель:** сохранить raylib и геймплей без изменений, убрать фиксированные
960×540 hitbox-координаты из меню и собрать интерфейс из адаптивных компонентов.

## Этапы

1. **Интеграция.** Закрепить исходники RmlUi 6.3 и FreeType; написать тонкий
   адаптер к текущему raylib GPU-контексту. Меню рендерится в реальных
   физических размерах окна **после** старой виртуальной камеры, без изменений
   камеры, FPS, тайминга и джаджмента игрового процесса.
2. **UI-фундамент.** Перенести палитру Nightbloom в единый RCSS, ввести
   flex-layout, скролл, фокус, ввод UTF-8 и графические ассеты. RML/RCSS
   хранятся отдельно от C++, C++ отвечает только за действия и данные.
3. **PLAY — первая настоящая миграция.** Перенести выбор песен,
   поиск/список сложностей, действия PLAY/EDIT/BACK/RESCAN,
   статистику чарта и подлинную историю Results. Изменения
   размеров окна не требуют изменения hitbox-кода, так как обработку кликов
   берёт на себя RmlUi.
4. **Остальные экраны.** После проверок мигрировать Home, SET, ROUTE, AUDIO,
   SKINS, Settings и Results. Сложный editor route-preview останется
   отдельным встроенным raylib-view, но панели, кнопки, списки и поля
   будут предоставлены RmlUi. Затем удалить старую ручную UI-геометрию.

**Статус:** этапы 1–3 реализованы и проверены на Windows x64 (Release/CTest).
Этап 4 начат: HOME и PLAY теперь используют общий RmlUi-контекст и адаптивную
разметку. Headless-тесты HOME проверяют 960×540, 800×450 и 1920×810;
ручная визуальная проверка на Windows ещё нужна. Settings (Display/Audio,
binds, calibration) добавлен в общий RmlUi-контекст; Editor, SKINS и Results
пока остаются на raylib.

## Граница реализации

- Платформа рисования, экран PLAY для **выбора чарта** и его ввод
  должны быть функциональными в новой реализации.
- На данном этапе остальные экраны используют прежние raylib-виджеты.
  Никакие автоматические анимации не добавляются в скоринговый экран.
- RML/RCSS: `assets/ui/rml/home.rml`, `assets/ui/rml/home.rcss`,
  `assets/ui/rml/play.rml`, `assets/ui/rml/play.rcss`,
  `assets/ui/rml/settings.rml`, `assets/ui/rml/settings.rcss` и общий
  `assets/ui/rml/common.rcss`.
  Ресурсы копируются рядом с exe при сборке.
- Граница интеграции: \`NativeMenuUi\` — владелец Rml-документа, обрабатывает
  события и возвращает существующие \`MenuEvents\`; \`SongSelectUi\` и
  \`SongLibrary\` остаются источником правды для выбора, загрузки и результатов.
- Вызов \`RenderFrame()\` рендерит RmlUi в экранных пикселях, после
  \`EndMode2D()\`. При ошибке инициализации/загрузки шрифта остаётся
  прежний функциональный PLAY, без потери доступа к песням.
- Изменений форматов Song Set, Chart или Results здесь нет.
- Для локального отката зависимости во время разработки предусмотрено
  \`-DLOCUS_ENABLE_RMLUI=OFF\`. Это временный флаг миграции, а не
  поддержка двух постоянных UI в релизе.

## Проверки

- Windows x64 Release + CTest и существующие headless проверки.
- Тест компоновки RmlUi: PLAY 1280×720, 1920×810, 800×540;
  HOME 960×540, 800×450, 1920×810;
  SETTINGS 960×540 и 1536×960;
  старые тесты и игровые headless-проверки остались зелёными.
- Ручная визуальная проверка перед заменой остальных экранов: 16:9, 16:10,
  21:9; фокус поиска, клики по чартам, скролл и выбор сохранённых Results.
  CI без графического окна не заменяет эту проверку.

## Settings: новая граница представления

`NativeSettingsUi` — отдельный RmlUi presenter. Он не хранит копию настроек,
вместо этого редактирует `GameSettings`, сохраняет текущую страницу/группу в
`SettingsUi` и передаёт `MenuEvents` в приложение. Общий контроллер
`HandleMenuInput` обслуживает только семантику эксклюзивного ввода
(rebind, offset edit, calibration), а его фиксированные mouse hitboxes
при включённом Native Settings игнорируются. Калибровка остаётся привязана
к исходным timestamp клавиш, а не к частоте кадров или RmlUi.

## Editor shell — текущий этап миграции

- `editor.rml` / `editor.rcss` задают единые BACK, название и флаг
  несохранённых изменений, вкладки SET / ROUTE / AUDIO и нижние
  PLAY / RESTART / скорость / копирование точного времени.
- `NativeEditorUi` отправляет семантические события в существующий
  `EditorControls` и `EditorUiState`, не подменяет клики координатами.
- ROUTE preview, поле нот, верхняя линейка и общий song overview, таблица
  AUDIO и поля SET пока рендерятся raylib. Оболочка совпадает с физическим
  `ViewportTransform::Menu` и учитывает letterbox в окне любого размера.
- Следующий шаг — динамические внутренние панели SET / AUDIO, затем
  вспомогательные панели ROUTE. Из старого raylib UI их нужно удалять
  по мере перевода, не поддерживая две реализации одновременно.

## Цена новой библиотеки

Чистая сборка займёт больше времени, потому что компилируются RmlUi и
FreeType. Обычная инкрементальная сборка изменённых файлов LOCUS не должна
пересобирать эти зависимости, пока их pinned версии не изменились.

## Исправление renderer/asset path

- RmlUi solid geometry (панели, кнопки, рамки) использует белую текстуру
  raylib по умолчанию вместо GL texture ID 0; иначе фон мог пропасть.
- Все локальные UI-ассеты исполняемого файла находятся в `assets/ui/`:
  Home wordmark draws through `HomeBrandAssets` at RmlUi layout bounds;
  arbitrary RmlUi images load through `RmlUi FileInterface`.
- Disk image альфа премультиплицируется только при загрузке в RmlUi;
  скины и игровой рендер не изменены.

## Nightbloom visual parity (Home / PLAY)

- Home uses the 804dp action row, 370dp wordmark, 74dp gap and Nightbloom
  colors of `design/ui/v2`. The approved PNG is rendered by raylib at the
  RmlUi layout box, not by an RmlUi `<img>` placeholder.
- Layout scale is `min(width/960,height/540)`, independent of monitor DPI:
  an 80dp control is 128 physical pixels high in a 1536x960 window.
- PLAY restores distinct Song Set, difficulty, six metrics, saved history
  and wide library tree sections. The footer exposes wired SONG SET, IMPORT
  and RESCAN controls.
- Home retains its static atmospheric gradient. Song selection has no
  synthetic background: artwork is displayed only when the Song Set has one.
- CTest checks layout structure, relative sizing, approved asset PNG and
  keyboard focus. Real GPU visual inspection still requires Windows hardware.
