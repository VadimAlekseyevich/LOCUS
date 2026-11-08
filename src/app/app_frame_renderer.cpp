#include "app/app_frame_renderer.h"
#include "app/viewport.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace locus::app {
namespace {
constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 540;
constexpr Color kBackground = {21, 23, 33, 255};
}

// The installed Windows UI font is read from the OS, not bundled or redistributed.
// A missing/unreadable TTF gracefully falls back to raylib's built-in font.
UiFont LoadUiFont() {
#ifdef _WIN32
    const char* windowsDirectory = std::getenv("WINDIR");
    const std::string fontsDirectory = (windowsDirectory && windowsDirectory[0] != '\0')
                                           ? std::string(windowsDirectory) + "\\Fonts\\"
                                           : "C:\\Windows\\Fonts\\";
    // Include Latin and Cyrillic glyphs for Unicode local track titles.
    // Windows provides these font faces; no third-party fonts are bundled.
    std::vector<int> glyphs;
    glyphs.reserve(95 + 304);
    for (int codepoint = 32; codepoint <= 126; ++codepoint) {
        glyphs.push_back(codepoint);
    }
    for (int codepoint = 0x0400; codepoint <= 0x052F; ++codepoint) {
        glyphs.push_back(codepoint);
    }
    const char* fontFiles[] = {"segoeui.ttf", "arial.ttf"};
    for (const char* filename : fontFiles) {
        const std::string path = fontsDirectory + filename;
        if (!FileExists(path.c_str())) {
            continue;
        }
        Font candidate = LoadFontEx(path.c_str(), 64, glyphs.data(),
                                    static_cast<int>(glyphs.size()));
        // LoadFontEx may return the default font if rasterization fails.
        if (IsFontValid(candidate) &&
            candidate.texture.id != GetFontDefault().texture.id) {
            SetTextureFilter(candidate.texture, TEXTURE_FILTER_BILINEAR);
            std::cout << "UI_FONT_SYSTEM=" << filename << '\n';
            return UiFont{candidate, true};
        }
    }
#endif
    std::cout << "UI_FONT_FALLBACK=raylib-default\n";
    return UiFont{GetFontDefault(), false};
}

static void DrawTextLabel(Font font, const char* message, Vector2 position,
                   float size, Color color) {
    DrawTextEx(font, message, position, size, 1.0f, color);
}

// Shared virtual hitboxes for the global quit modal. Keeping them outside
// the screen controller means pressing X from any screen is handled alike.
constexpr Rectangle kQuitConfirmButton{263.0f, 329.0f, 196.0f, 54.0f};
constexpr Rectangle kQuitCancelButton{502.0f, 329.0f, 196.0f, 54.0f};

static void DrawQuitConfirmation(Font font, const AppState& state,
                          const locus::core::ChartEditor& editor,
                          const locus::app::EditorUiState& editorUi,
                          const locus::app::SkinUi& skinUi) {
    if (!state.quitConfirmation) return;
    // The outer application does not terminate until a second explicit
    // action. Reassure users about unsaved edits before they choose.
    DrawRectangle(0, 0, kWindowWidth, kWindowHeight, Color{7, 11, 19, 220});
    const Rectangle card{177.0f, 146.0f, 606.0f, 264.0f};
    DrawRectangleRounded(card, 0.085f, 10, Color{34, 42, 59, 255});
    DrawRectangleRoundedLinesEx(card, 0.085f, 10, 2.0f,
                                Color{214, 183, 127, 255});
    DrawTextLabel(font, "EXIT LOCUS?", {309.0f, 177.0f}, 36.0f,
                  Color{246, 233, 211, 255});
    DrawTextLabel(font, "This will close the game.", {307.0f, 236.0f},
                  21.0f, Color{216, 227, 234, 255});
    if (state.screen == Screen::Editor &&
        (editor.Dirty() || editorUi.EditingText()))
        DrawTextLabel(font, "UNSAVED CHART CHANGES WILL BE LOST",
                      {266.0f, 272.0f}, 17.0f,
                      Color{251, 188, 164, 255});
    else if (state.screen == Screen::Skin &&
             (skinUi.dirty || skinUi.editingName || skinUi.editingNumberRow >= 0))
        DrawTextLabel(font, "UNSAVED SKIN CHANGES WILL BE LOST",
                      {273.0f, 272.0f}, 17.0f,
                      Color{251, 188, 164, 255});
    DrawRectangleRounded(kQuitConfirmButton, 0.16f, 5,
                         Color{91, 50, 54, 255});
    DrawRectangleRounded(kQuitCancelButton, 0.16f, 5,
                         Color{38, 91, 82, 255});
    DrawTextLabel(font, "Y  -  QUIT", {305.0f, 343.0f}, 23.0f,
                  Color{251, 226, 217, 255});
    DrawTextLabel(font, "N  -  STAY", {545.0f, 343.0f}, 23.0f,
                  Color{216, 251, 237, 255});
    DrawTextLabel(font, "ESC ALSO CANCELS", {394.0f, 394.0f}, 13.0f,
                  Color{170, 189, 205, 255});
}

static void DrawHomeScreen(Font font, const locus::app::SongLibrary& library,
                    const std::string& message) {
    locus::app::DrawMainMenu(font, library, message);
}

static void DrawLocalMusicScreen(Font font, const locus::app::LocalMusic& song,
                          double now, const std::string& musicError) {
    DrawTextLabel(font, "YOUR MUSIC  /  LISTEN MODE", Vector2{88.0f, 112.0f},
                  35.0f, Color{163, 233, 228, 255});
    DrawTextLabel(font, song.DisplayName().c_str(), Vector2{89.0f, 187.0f},
                  25.0f, Color{231, 232, 246, 255});

    const double duration = song.DurationSeconds();
    const double elapsed = std::clamp(song.SongSeconds(now), 0.0, duration);
    DrawRectangle(89, 255, 782, 9, Color{55, 66, 84, 255});
    if (duration > 0.0) {
        DrawRectangle(89, 255,
                      static_cast<int>(782.0 * (elapsed / duration)), 9,
                      Color{121, 223, 213, 255});
    }
    const int currentSeconds = static_cast<int>(elapsed);
    const int totalSeconds = static_cast<int>(duration);
    DrawTextLabel(font,
                  TextFormat("%02d:%02d / %02d:%02d",
                             currentSeconds / 60, currentSeconds % 60,
                             totalSeconds / 60, totalSeconds % 60),
                  Vector2{89.0f, 284.0f}, 22.0f,
                  Color{210, 223, 234, 255});

    const char* status = song.Finished() ? "TRACK COMPLETE"
                         : song.Paused() ? "PAUSED"
                         : song.Playing() ? "PLAYING" : "READY";
    DrawTextLabel(font, status, Vector2{89.0f, 332.0f}, 23.0f,
                  Color{147, 226, 222, 255});
    DrawTextLabel(font, "SPACE  PAUSE / RESUME     R / ENTER  REPLAY",
                  Vector2{89.0f, 378.0f}, 19.0f,
                  Color{237, 219, 163, 255});
    DrawTextLabel(font, "O  CHOOSE ANOTHER SONG      H  HOME",
                  Vector2{89.0f, 411.0f}, 19.0f,
                  Color{210, 222, 233, 255});
    DrawTextLabel(font,
                  musicError.empty()
                      ? "NO TURN CHART YET - SONG PREVIEW ONLY"
                      : musicError.c_str(),
                  Vector2{89.0f, 446.0f}, 17.0f,
                  musicError.empty() ? Color{173, 183, 207, 255}
                                     : Color{241, 150, 150, 255});
}

void RenderFrame(const AppState& state, bool debugScreens, Font font,
                 const locus::core::Chart& demo,
                 const GameplayView& gameplay,
                 const locus::app::LocalMusic& localSong,
                 const locus::core::ChartEditor& editor,
                 const locus::app::EditorUiState& editorUi,
                 const locus::app::ChartPlayback& editorTransport,
                 const std::string& editorStatus, EditorPending editorPending,
                 const locus::app::EditorSetView& editorSetView,
                 const locus::app::EditorSetRequest* editorSetPending,
                 const std::string& musicError, double now,
                  const locus::app::SongLibrary& library,
                  const locus::app::SongSelectUi& libraryUi,
                  const locus::app::GameSettings& settings,
                  const locus::app::SettingsUi& settingsUi,
                  const locus::app::Skin& draftSkin,
                  const locus::app::SkinUi& skinUi,
                  const std::string& menuStatus) {
    BeginDrawing();
    ClearBackground(state.screen == Screen::Play
                        ? gameplay.skin.background : kBackground);

    // Menus/editor retain their original 960x540 16:9 virtual viewport.
    // Gameplay instead scales its 540x540 playfield to the actual window
    // height, even when the window is not 16:9 (landscape assumption).
    // For portrait windows, width limits the square so no side disappears.
    const auto transform = state.screen == Screen::Play
        ? locus::app::ViewportTransform::Gameplay(GetScreenWidth(), GetScreenHeight())
        : locus::app::ViewportTransform::Menu(GetScreenWidth(), GetScreenHeight());
    Camera2D camera{};
    camera.offset = Vector2{transform.offsetX, transform.offsetY};
    camera.target = Vector2{0.0f, 0.0f};
    camera.zoom = transform.scale;
    BeginMode2D(camera);

    // No wordmark or global help text during scoring: only field + hits.
    if (state.screen != Screen::Play) {
        DrawTextLabel(font, "LOCUS", Vector2{50.0f, 38.0f}, 30.0f,
                      Color{223, 225, 238, 255});
    }

    if (state.screen == Screen::Home) {
        DrawHomeScreen(font, library, menuStatus);
    } else if (state.screen == Screen::SongSelect) {
        locus::app::DrawSongSelectMenu(font, library, libraryUi);
    } else if (state.screen == Screen::Skin) {
        locus::app::DrawSkinMenu(font, draftSkin, skinUi);
    } else if (state.screen == Screen::Settings) {
        locus::app::DrawSettingsMenu(font, settings, settingsUi);
    } else if (state.screen == Screen::Listen) {
        DrawLocalMusicScreen(font, localSong, now, musicError);
    } else if (state.screen == Screen::Play) {
        DrawPlayGrid(font, gameplay, now);
        if (gameplay.paused) {
            DrawRectangle(0, 0, kWindowWidth, kWindowHeight,
                          Color{12, 14, 22, 204});
            DrawTextLabel(font, "PAUSED", Vector2{355.0f, 198.0f},
                          59.0f, Color{240, 246, 247, 255});
            const std::string resume = locus::app::KeyboardKeyName(
                gameplay.bindings.Key(locus::app::GameAction::Pause)) + "  -  RESUME";
            DrawTextLabel(font, resume.c_str(),
                          Vector2{325.0f, 287.0f}, 28.0f,
                          Color{239, 219, 164, 255});
            const std::string controls = locus::app::KeyboardKeyName(
                gameplay.bindings.Key(locus::app::GameAction::Retry)) +
                "  RETRY      " + locus::app::KeyboardKeyName(
                gameplay.bindings.Key(locus::app::GameAction::Home)) + "  HOME";
            DrawTextLabel(font, controls.c_str(),
                          Vector2{277.0f, 335.0f}, 21.0f,
                          Color{204, 213, 232, 255});
        }
    } else if (state.screen == Screen::Results) {
        DrawResultsScreen(font, gameplay);
    } else if (state.screen == Screen::Editor) {
        const double selectedBeat = editorTransport.Playing()
            ? editorTransport.Beat(now) : static_cast<double>(editor.SelectedBeat());
        locus::app::RenderChartEditor(font, editor, editorUi,
                                     localSong.DisplayName(), localSong.Ready(),
                                     editorTransport.Playing(), editorTransport.Paused(),
                                     selectedBeat, editorTransport.Speed(),
                                     gameplay.bindings, gameplay.skin,
                                     editorSetView);
        const std::string statusLine = editorStatus.substr(0, 68);
        DrawTextLabel(font, statusLine.c_str(), Vector2{50.0f, 525.0f},
                      12.0f, Color{175, 212, 213, 255});
        if (editorSetPending && editorPending == EditorPending::None) {
            DrawRectangle(0, 0, kWindowWidth, kWindowHeight,
                          Color{10, 13, 21, 225});
            const bool wholeSet =
                editorSetPending->action == locus::app::EditorSetAction::DeleteSongSet;
            const bool deleteDifficulty =
                editorSetPending->action == locus::app::EditorSetAction::DeleteDifficulty;
            const char* heading = wholeSet ? "DELETE ENTIRE SONG SET?"
                                : deleteDifficulty ? "DELETE THIS DIFFICULTY?"
                                : "UNSAVED CHART CHANGES";
            DrawTextLabel(font, heading, {170.0f, 175.0f}, 29.0f,
                          Color{244, 206, 143, 255});
            DrawTextLabel(font, wholeSet
                          ? "SONG, AUDIO AND ALL CHARTS GO TO TRASH"
                          : deleteDifficulty
                          ? "SELECTED CHART GOES TO RECOVERABLE TRASH"
                          : "THE CURRENT UNSAVED DRAFT WILL BE DISCARDED",
                          {160.0f, 240.0f}, 18.0f,
                          Color{217, 229, 235, 255});
            if (editorSetPending->discardsDraft)
                DrawTextLabel(font, "WARNING: UNSAVED EDITS WILL BE LOST",
                              {195.0f, 281.0f}, 18.0f,
                              Color{239, 161, 146, 255});
            DrawTextLabel(font, "Y  CONFIRM        N / ESC  CANCEL",
                          {222.0f, 341.0f}, 21.0f,
                          Color{221, 236, 234, 255});
        }
        if (editorPending != EditorPending::None) {
            DrawRectangle(0, 0, kWindowWidth, kWindowHeight, Color{10, 13, 21, 218});
            DrawTextLabel(font, "UNSAVED CHART CHANGES", {193.0f, 187.0f},
                          32.0f, Color{245, 216, 165, 255});
            DrawTextLabel(font, "Y  DISCARD CHANGES", {245.0f, 268.0f},
                          23.0f, Color{235, 156, 156, 255});
            DrawTextLabel(font, "N  CANCEL AND KEEP EDITING", {206.0f, 315.0f},
                          21.0f, Color{201, 226, 226, 255});
            DrawTextLabel(font, "CTRL+S AFTER CANCEL TO SAVE", {218.0f, 364.0f},
                          18.0f, Color{192, 203, 223, 255});
        }
    } else {
        const char* screen = ScreenName(state.screen);
        constexpr float kScreenFontSize = 54.0f;
        const Vector2 textSize = MeasureTextEx(font, screen, kScreenFontSize, 1.0f);
        DrawTextLabel(font, screen,
                      Vector2{(static_cast<float>(kWindowWidth) - textSize.x) / 2.0f,
                              static_cast<float>(kWindowHeight) / 2.0f - 38.0f},
                      kScreenFontSize, Color{211, 239, 241, 255});
        DrawTextLabel(font, "Unexpected screen state - return Home or restart LOCUS",
                      Vector2{50.0f, static_cast<float>(kWindowHeight - 112)},
                      22.0f, Color{208, 211, 225, 255});
    }
    // F11 still works; the fullscreen hint no longer floats across menu
    // buttons or the editor. Debug overlays only belong to explicit diagnostics.
    if (debugScreens && state.screen != Screen::Editor &&
        state.screen != Screen::Settings && state.screen != Screen::Play) {
        DrawTextLabel(font, "DEBUG: F1 Home   F2 Play   F3 Editor   F4 Results",
                      Vector2{50.0f, static_cast<float>(kWindowHeight - 65)},
                      22.0f, Color{232, 213, 160, 255});
    }
    DrawQuitConfirmation(font, state, editor, editorUi, skinUi);
    EndMode2D();
    if (settings.showFps) {
        // Screen-pixel overlay, separate from the gameplay camera.
        DrawRectangleRounded({8.0f, 8.0f, 100.0f, 30.0f}, 0.18f, 4,
                             Color{17, 22, 33, 224});
        DrawText(TextFormat("FPS %d", GetFPS()), 16, 14, 18,
                 Color{199, 244, 228, 255});
    }
    EndDrawing();
}



} // namespace locus::app
