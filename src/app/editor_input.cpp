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

EditorControls PollEditorControls(const GameSettings& settings) {
    EditorControls input;
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    input.shiftHeld = shift;
    const auto pressed = [&](ShortcutAction action) {
        return ShortcutPressed(settings, action);
    };
    input.copyRange = pressed(ShortcutAction::EditorCopy);
    input.cutRange = pressed(ShortcutAction::EditorCut);
    input.pasteRange = pressed(ShortcutAction::EditorPaste);
    input.undo = pressed(ShortcutAction::EditorUndo);
    input.redo = pressed(ShortcutAction::EditorRedo);
    // Navigation is independent of scoring directions: no key creates notes.
    const auto navigation = [&](int key) {
        return IsKeyPressed(key);
    };
    const auto navigationHeld = [&](int key) {
        return IsKeyDown(key);
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

    input.elapsedSeconds = GetFrameTime();

    input.mirrorHorizontal = pressed(ShortcutAction::EditorMirrorH);
    input.mirrorVertical = pressed(ShortcutAction::EditorMirrorV);
    input.zoomIn = pressed(ShortcutAction::EditorZoomIn);
    input.zoomOut = pressed(ShortcutAction::EditorZoomOut);
    input.snapMore = pressed(ShortcutAction::EditorSnapMore);
    input.snapLess = pressed(ShortcutAction::EditorSnapLess);
    input.removeTurn = IsKeyPressed(KEY_BACKSPACE) ||
                       IsKeyPressed(KEY_DELETE);
    input.headingLeft = pressed(ShortcutAction::EditorHeadingLeft);
    input.headingRight = pressed(ShortcutAction::EditorHeadingRight);
    input.previewToggle = pressed(ShortcutAction::EditorPreview);
    input.previewRestart = pressed(ShortcutAction::EditorRestart);
    input.enter = IsKeyPressed(KEY_ENTER);
    input.escape = IsKeyPressed(KEY_ESCAPE);
    input.backspace = IsKeyPressed(KEY_BACKSPACE);
    input.tab = IsKeyPressed(KEY_TAB);
    input.speedIndex = -1;
    if (!control && !shift) {
        for (int i = 0; i < 5; ++i)
            if (IsKeyPressed(settings.Key(static_cast<GameAction>(
                    static_cast<int>(GameAction::Practice100) + i)))) {
                input.speedIndex = i;
                break;
            }
    }
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
    if (pressed(ShortcutAction::EditorPaste)) {
        const char* clipboard = GetClipboardText();
        if (clipboard) input.typedUtf8 = clipboard;
    }
    return input;
}

} // namespace locus::app
