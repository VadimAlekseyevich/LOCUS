#include "app/chart_editor_ui.h"
#include "app/viewport.h"

#include <algorithm>
#include <array>

namespace locus::app {

// Native input adapter: no document mutations, no editing decisions.
// The pure ApplyEditorControls function consumes only EditorControls.
Vector2 EditorVirtualMouse(Vector2 physical, int screenWidth, int screenHeight) {
    const auto transform = ViewportTransform::Menu(screenWidth, screenHeight);
    return transform.scale > 0.0f ? transform.ToVirtual(physical)
                                  : Vector2{-1.0f, -1.0f};
}

EditorControls PollEditorControls(const GameSettings& settings,
                                  const std::array<bool, 4>& gameplayHeld) {
    EditorControls input;
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    input.shiftHeld = shift;
    input.copyRange = control && IsKeyPressed(KEY_C);
    input.cutRange = control && IsKeyPressed(KEY_X);
    input.pasteRange = control && IsKeyPressed(KEY_V);
    input.undo = control && !shift && IsKeyPressed(KEY_Z);
    input.redo = (control && IsKeyPressed(KEY_Y)) ||
                 (control && shift && IsKeyPressed(KEY_Z));

    // A configured gameplay direction always wins over a bare editor shortcut.
    // Modifier shortcuts can still use Left/Right or PgUp/PgDn deliberately,
    // because modifiers suppress direction placement.
    const auto available = [&](int key) {
        return !IsGameDirectionKey(settings, key);
    };
    const auto plain = [&](int key) {
        return !control && !shift && available(key) && IsKeyPressed(key);
    };
    const auto navigation = [&](int key) {
        return (control || shift || available(key)) && IsKeyPressed(key);
    };
    const auto navigationHeld = [&](int key) {
        return (control || shift || available(key)) && IsKeyDown(key);
    };
    const bool previous = navigation(KEY_LEFT) || navigation(KEY_PAGE_UP);
    const bool next = navigation(KEY_RIGHT) || navigation(KEY_PAGE_DOWN);
    input.previousBeat = !control && !shift && previous;
    input.nextBeat = !control && !shift && next;
    input.previousBar = control && !shift && previous;
    input.nextBar = control && !shift && next;
    input.shiftRangePrevious = control && shift && previous;
    input.shiftRangeNext = control && shift && next;
    input.movePrevious = shift && !control && previous;
    input.moveNext = shift && !control && next;
    input.heldBarDirection = control && !shift
        ? (static_cast<int>(navigationHeld(KEY_RIGHT) || navigationHeld(KEY_PAGE_DOWN)) -
           static_cast<int>(navigationHeld(KEY_LEFT) || navigationHeld(KEY_PAGE_UP)))
        : 0;

    // The exact four game-direction key states are reused, not polled from a
    // hard-coded DFJK set. Modifier-based chart actions never place notes.
    input.directionKeysSuppressed = control || shift;
    input.directionKeysHeld = input.directionKeysSuppressed
        ? 0u : EditorDirectionMask(gameplayHeld);
    input.directionChordWindowSeconds = settings.chordWindowMs / 1000.0f;
    input.elapsedSeconds = GetFrameTime();

    input.mirrorHorizontal = plain(KEY_I);
    input.mirrorVertical = shift && !control && available(KEY_I) &&
                           IsKeyPressed(KEY_I);
    input.zoomIn = (available(KEY_EQUAL) && IsKeyPressed(KEY_EQUAL)) ||
                   (available(KEY_KP_ADD) && IsKeyPressed(KEY_KP_ADD)) ||
                   (available(KEY_RIGHT_BRACKET) && IsKeyPressed(KEY_RIGHT_BRACKET));
    input.zoomOut = (available(KEY_MINUS) && IsKeyPressed(KEY_MINUS)) ||
                    (available(KEY_KP_SUBTRACT) && IsKeyPressed(KEY_KP_SUBTRACT)) ||
                    (available(KEY_LEFT_BRACKET) && IsKeyPressed(KEY_LEFT_BRACKET));
    // With the new DFJK default, vertical arrows adjust placement
    // density while horizontal arrows move between timeline grid lines.
    // Rebinding an arrow to gameplay suppresses this editor shortcut.
    input.snapMore = plain(KEY_PERIOD) || plain(KEY_UP);
    input.snapLess = plain(KEY_COMMA) || plain(KEY_DOWN);
    input.removeTurn = IsKeyPressed(KEY_BACKSPACE) ||
                       (available(KEY_DELETE) && IsKeyPressed(KEY_DELETE));
    input.headingLeft = plain(KEY_Q);
    input.headingRight = plain(KEY_E);
    input.previewToggle = plain(KEY_SPACE);
    input.previewRestart = plain(KEY_R);
    input.enter = IsKeyPressed(KEY_ENTER);
    input.escape = IsKeyPressed(KEY_ESCAPE);
    input.backspace = IsKeyPressed(KEY_BACKSPACE);
    input.tab = IsKeyPressed(KEY_TAB);
    input.speedIndex = plain(KEY_ONE) ? 0
                     : plain(KEY_TWO) ? 1
                     : plain(KEY_THREE) ? 2
                     : plain(KEY_FOUR) ? 3
                     : plain(KEY_FIVE) ? 4 : -1;
    input.mouseClick = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    input.mouseHeld = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    input.mouseReleased = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    input.mouse = EditorVirtualMouse(GetMousePosition(), GetScreenWidth(),
                                     GetScreenHeight());
    input.mouseWheel = GetMouseWheelMove();
    if (control && input.mouseWheel != 0.0f) {
        input.zoomIn |= input.mouseWheel > 0;
        input.zoomOut |= input.mouseWheel < 0;
        input.mouseWheel = 0.0f;
    }
    int codepoint = GetCharPressed();
    while (codepoint > 0) {
        int bytes = 0;
        const char* utf8 = CodepointToUTF8(codepoint, &bytes);
        if (utf8 && bytes > 0) input.typedUtf8.append(utf8, bytes);
        codepoint = GetCharPressed();
    }
    if (control && IsKeyPressed(KEY_V)) {
        const char* clipboard = GetClipboardText();
        if (clipboard) input.typedUtf8 = clipboard;
    }
    return input;
}

} // namespace locus::app
