# LOCUS — инженерная приёмка и оставшиеся задачи

Актуальный чеклист, а не журнал разработки. Номера T01–T16 сохранены для
[tracking issue #3](https://github.com/VadimAlekseyevich/LOCUS/issues/3).
Реализованный код не равен полной приёмке: галочка означает завершение всех
критериев, а не только зелёный headless тест. T02 принят в tracking issue;
остальные карточки не закрываются этой редакцией документации.

Приоритет: физический тайминг и сохранность данных, реальная адаптивность,
измеряемая производительность, затем релиз. Архитектуру описывает
[ARCHITECTURE](ARCHITECTURE.md), продуктовые задачи — [ROADMAP](ROADMAP.md).

## Карточки

<a id="t01"></a>

### T01. Запуск CI и доставка UI-ресурсов

**Состояние.** Автоматизировано: триггеры assets/examples, общий манифест ресурсов, зависимое копирование, проверка ZIP вне checkout и из Unicode-пути.

**Приёмка / оставшееся.** Вручную открыть HOME, PLAY, EDIT, SETTINGS, SKINS и RESULTS из распакованного ZIP. Проверить сборку после изменения, добавления и удаления ресурса; отсутствующий ресурс должен быть явно диагностирован.

[Windows CI](../.github/workflows/windows_ci.yml), [manifest](../cmake/ui_assets_manifest.cmake), [UI](UI_RML_MIGRATION.md).

<a id="t02"></a>

### T02. Сквозные сценарии на реальных контроллерах

**Состояние.** Сквозной сценарий на реальных контроллерах с изолированным устройством вывода принят; тесты остаются регрессионной базой.

**Приёмка / оставшееся.** Сохранять контракт: Home → Edit — чистый черновик, Library → Edit — выбранная сложность; две dirty сложности → TEST → пауза → возврат сохраняют историю и время; нормальное завершение создаёт ровно одну попытку; отмена не теряет данные. Повторно запускать при изменении переходов.

[App journey](../tests/app_journey_tests.cpp), [session transition](../tests/session_transition_tests.cpp).

<a id="t03"></a>

### T03. Точные события ввода до входа в TurnJudge

**Состояние.** Win32 очередь сохраняет timestamp и порядок отдельных keydown/keyup, подавляет автоповтор; логическая FPS-инвариантность и раскладка клавиш проверяются автоматически.

**Приёмка / оставшееся.** Измерить реальную физическую точность на Windows. Воспроизвести диагональ с разницей 7 мс, два свежих нажатия между кадрами, потерю фокуса, паузу/retry. Сопоставить grades и signed errors при 30/60/144/240 FPS и длинном кадре. Единица timestamp не равна доказанной точности.

[Timing contract](TIMING_CONTRACT.md), [native capture](../src/app/native_key_capture.cpp).

<a id="t04"></a>

### T04. Контракт часов и скоростей воспроизведения

**Состояние.** Единое преобразование монотонного времени нажатия в song time, snapshot raw/visual, silent preroll/tail, rate/pause/seek; SoundTouch и один retained PCM-буфер.

**Приёмка / оставшееся.** Loopback или внешняя запись на 100/50/125% в PLAY и 10% в редакторе: startup, нота на 0, intro skip, pause/resume, rate change на паузе, seek, конец аудио, отсутствие устройства. Указать устройство, буфер, sample rate, median/P95/max ошибки, задержку первой декомпрессии и peak RSS. SoundTouch rate clock не считать подтверждённым hardware cursor.

[Timing contract](TIMING_CONTRACT.md), [clock tests](../tests/clock_contract_tests.cpp).

<a id="t05"></a>

### T05. Точные ноты и локальная сетка редактора

**Состояние.** Chart v4 допускает отдельные timeMs в одном тике; exact selection/placement, copy/paste, snap/tempo reprojection и undo сохраняют их. Локальная сильная доля начинается на BPM-маркере.

**Приёмка / оставшееся.** Пройти полный пользовательский цикл двух близких нот: выбор каждой, изменение, save/reopen/PLAY. Маркер 750 мс при 120 BPM создаёт новый downbeat; масштаб px/с и нижний обзор сохраняются. Проверить сетку, внешние правки, undo и переключение snap. Не менять timestamp без авторской команды.

[Chart format](CHART_FORMAT.md), [editor tests](../tests/chart_editor_tests.cpp).

<a id="t06"></a>

### T06. Стабильная идентичность сложности и версии результата

**Состояние.** Постоянный difficultyId в chart v4, gameplayRevision и judgementVersion в Results v2; rename сохраняет ID, copy/new создают новый. Пакеты объединяются по setId/difficultyId.

**Приёмка / оставшееся.** Полная приёмка rename → перезапуск → история, copy → независимая история, смена содержимого/правил → отметка старой ревизии, collision/import/restore. TEST и незавершённый заход не публикуются; фиксированные HT/DT, моды и seed RD сохраняются; старый variable rate обозначен. Реплей проверяется тем же судьёй; AT не сохраняет результат, перемотка разрешена только зрителю.

[Results](RESULTS.md), [Song Sets](SONG_SETS.md).

<a id="t07"></a>

### T07. Запись, частичный успех и восстановление

**Состояние.** Общие эксклюзивные staging/flush/atomic replace, chart backup, root/set lock, пакетная валидация/rollback, мультивыбор импорта с одним обновлением индекса, экспорт актуальных сохранённых файлов без зависимости от кеша, частичный Save All и сохранение dirty-состояния при ошибке.

**Приёмка / оставшееся.** Физические denied write, full disk, прерывание между записью/rename/index refresh и одновременный save двух процессов. Повтор не дублирует сложность, старые данные или backup доступны, ошибка соответствует факту; оставшиеся черновики открыты. Не удалять неизвестные staging-файлы.

[Recovery](SONG_SETS.md), [atomic writer](../src/app/atomic_text_file.h).

<a id="t08"></a>

### T08. Библиотека больших коллекций и поиск

**Состояние.** Компактный индекс без резидентных note vectors, проверка size/mtime/fingerprint, lazy payload, отменяемые scans/history, Unicode search и виртуальные списки. Тестовые каталоги 8193 charts и плотные 786432 notes.

**Приёмка / оставшееся.** Cold/warm scan на 100/1000/8193+ файлов, p95 поиска и смены выбора, пик памяти, прочитанные байты, количество живых DOM nodes. Same-size/same-mtime внешняя правка, повреждённый set, rename/delete, быстрый поиск «Ёлка»/«ёлка»; каталог полон, старый worker не перезаписывает новый.

[Architecture](ARCHITECTURE.md), [library tests](../tests/song_library_tests.cpp).

<a id="t09"></a>

### T09. Главный цикл, переходы и модальные действия

**Состояние.** Screen flow/session transition, editor/play actions, menu controllers и presentation разделены; main координирует ресурсы и порядок кадра.

**Приёмка / оставшееся.** Пройти T02, быстрые переходы во время fade, единственную команду на жест, возврат фокуса и playback при отмене. Сужать зависимости только при реальном конфликте владельцев или новом требовании; не дробить main ради числа строк.

[Architecture](ARCHITECTURE.md), [screen flow](../src/app/screen_flow.cpp).

<a id="t10"></a>

### T10. Сеанс редактора и общие данные песни

**Состояние.** EditorFileActions/EditorDocument владеют открытыми черновиками, ComparisonCache компилирует по ревизии; общий transport, кэш SET view и direction counts. Song Setup объединяет metadata/audio/tempo/difficulties.

**Приёмка / оставшееся.** Три разные snap, две dirty, один точный курсор; смена фокуса не пересекает истории. Save focused/Save All failure, внешняя правка с прежним size/mtime, refresh comparison. Стабильный кадр не пересобирает список/маршруты; проверить область undo общей tempo map.

[Editor workflow tests](../tests/editor_workflow_tests.cpp), [comparison](../src/app/editor_comparison.h).

<a id="t11"></a>

### T11. Владение UI, фокус и настройки управления

**Состояние.** RmlUi host и отдельные presenter, semantic actions, фокус/модалки, remappable PLAY/MENUS/EDITOR, виртуальные списки. Esc в PLAY пауза, выход с подтверждением. Fallback сохраняется.

**Приёмка / оставшееся.** Tab/Shift+Tab, Enter, Esc, колесо, мышь на всех нативных экранах. Text/rebind/calibration приоритетны; dismiss popup не ставит ноту; Unicode и символы разметки буквальны. Подсказки соответствуют биндам. Для удаления fallback сначала доказать паритет и проверить запуск без RmlUi.

[UI contract](UI_RML_MIGRATION.md), [design review](../design/ui/v2/REVIEW.md).

<a id="t12"></a>

### T12. Адаптивный редактор и согласование геометрии

**Состояние.** EditorLayoutSnapshot передаёт измеренные прямоугольники RmlUi в raylib draw/hit-test, flex-layout; максимальные равные квадраты без 280/380 cap. Нет постоянной шапки Back. SKINS name только на первом tab, preview одинаков между tabs.

**Приёмка / оставшееся.** Реальные окна 800×450, 960×540, 1280×720, 1536×960, 1920×810, 900×1200; Windows DPI 100/125/150/200%. Достижимость controls, ruler click/drag после resize, кириллица, 1/2/3 panes, wheel и focus. Не принимать только по headless geometry.

[UI contract](UI_RML_MIGRATION.md), [layout tests](../tests/rml_ui_layout_tests.cpp).

<a id="t13"></a>

### T13. Удобство игрока, маппера и редактора скина

**Состояние.** Основные пользовательские пути и кнопки есть; пауза, скрытие курсора, единая preview и чистый Home → Edit реализованы.

**Приёмка / оставшееся.** Прогнать [review](../design/ui/v2/REVIEW.md) на чистом профиле и заполненной библиотеке. Проверить PLAY→RESULTS→retry, MODS (ползунки/конфликты), replay/AT (перемотка вперёд/назад и на паузе), 50% при 200–600 px/s, диагонали/реверсы/повторы, Song Setup→Chart Editor→TEST, offset/bind conflicts, HSV↔HEX↔preview, failed save и dirty guard. Фиксировать кадры, DPI, данные, шаг дефекта. Новые gameplay cues — отдельное дизайнерское решение.

[GDD](GDD.md), [design review](../design/ui/v2/REVIEW.md).

<a id="t14"></a>

### T14. Профилирование полного кадра, аудио и калькулятора

**Состояние.** CPU route/mesh и editor benchmarks, reusable PathRibbonWorkspace, batched paste, TempoProjection, cached editor SET/COMPARE; Difficulty V4 — независимая нагрузка четырёх клавиш.

**Приёмка / оставшееся.** На одной машине/build сравнить p50/p95/p99 полного кадра, CPU/GPU отдельно, allocation/peak RSS, decode/startup, cold/warm scan, открытие/seek/paste/undo на 65k событий/256 BPM, смену artwork. Сохранять checksum и exact timestamp. Для V4 калибровать реальные попытки на jack/trill/roll/chord, rhythm и rests, не геометрии. Оптимизировать только доказанный bottleneck.

[Benchmarks](../benchmarks/route_preview_bench.cpp), [editor bench](../benchmarks/editor_bench.cpp), [difficulty](CHART_DIFFICULTY.md).

<a id="t15"></a>

### T15. Границы сборки, тестовые цели и документация

**Состояние.** Общие core/storage/runtime/editor/application targets, portable core CI, single source ownership check/CSV, clean/incremental build timings; актуальные contracts в docs.

**Приёмка / оставшееся.** Проверить чистую/повторную Windows Release + CTest/BAT, Linux core-only, список CTest, build-profile.csv и locus-source-ownership.csv. Синхронизировать docs/examples с изменяемыми контрактами в каждом коммите; design reference не является runtime.

[CMake](../CMakeLists.txt), [Architecture](ARCHITECTURE.md).

<a id="t16"></a>

### T16. Публичная сборка и критерий завершения

**Состояние.** Portable ZIP, assets/examples, launcher, проверки staging и ZIP вне checkout в Windows CI уже существуют.

**Приёмка / оставшееся.** Чистая Windows без VS/CMake: запуск из Unicode и read-only install path, demo, edit/save/restart, skin, import/export, delete/restore, history. Проверить dependency notices (включая SoundTouch), version, checksum, список файлов, сообщения об отсутствии ресурсов. Только после реального прогона — tagged release.

[README](../README.md#run), [Windows CI](../.github/workflows/windows_ci.yml).

## Подготовка к вебу

Отдельный план — [ONLINE_ARCHITECTURE.md](ONLINE_ARCHITECTURE.md). Не смешивать статус серверной разработки с приёмкой локального клиента. Linux core-only не подтверждает переносимость Windows-файловых адаптеров; серверные проверки должны запускаться без desktop targets.

## Правила проверки

1. Начинать с актуального main, точки входа и наблюдаемого дефекта или критерия.
2. Зафиксировать размер окна/DPI/данные, устройство, сборку и условия измерения.
   Тесты должны проверять конкретный контракт, не заменять физический прогон.
3. Проверять срез через production-код. Изменения форматов до первого релиза
   допустимы; указать версии, последствия, обновить примеры и документацию.
4. Вливать в main; в issue фиксировать SHA, фактические проверки и пробелы.
   Не заявлять полную приёмку по CI, который не проверяет GPU/устройство.

```powershell
cmake -S . -B build/windows -G "Visual Studio 17 2022" -A x64 -DLOCUS_BUILD_TESTS=ON -DLOCUS_BUILD_BENCHMARKS=ON
cmake --build build/windows --config Release --parallel 2
ctest --test-dir build/windows -C Release --output-on-failure
& build/windows/Release/locus.exe --loop-check
& build/windows/Release/locus.exe --screen-check
& build/windows/Release/locus.exe --session-check
& build/windows/Release/locus_route_preview_bench.exe
```

Проверить BAT launcher и portable package отдельно. Для core-only сборки
см. [README](../README.md#portable-core-development-linux-windows).
