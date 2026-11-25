#pragma once

#include "app/app_input.h"

namespace locus::app {

// Native RESULTS resolves RmlUi pointer/focus actions first. Keep the
// already-collected configured shortcuts separate until their owner is known.
struct ResultsKeyboardIntent {
    bool activate = false;
    bool retry = false;
    bool edit = false;
    bool back = false;
};

inline ResultsKeyboardIntent TakeResultsKeyboard(FrameInput& input) noexcept {
    const ResultsKeyboardIntent intent{input.startPressed,input.restartPressed,
        input.openEditorPressed,input.backPressed || input.homePressed};
    SuppressGlobalMenuShortcuts(input);
    input.restartPressed = false;
    return intent;
}

inline void ApplyResultsKeyboard(FrameInput& input, ResultsKeyboardIntent intent,
                                 bool historical, bool consumed) noexcept {
    if (consumed) return;
    if (intent.back) input.backPressed = true;
    else if (!historical) {
        if (intent.edit) input.openEditorPressed = true;
        else if (intent.retry) input.restartPressed = true;
        else if (intent.activate) input.startPressed = true;
    }
}

} // namespace locus::app
