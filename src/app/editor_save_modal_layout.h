#pragma once

#include "raylib.h"

namespace locus::app {
// Shared virtual geometry for save/discard/cancel on editor drafts. Hit tests
// convert physical pointer coordinates with the same viewport as rendering.
inline constexpr Rectangle kEditorSaveDraftButton{191.0f, 331.0f, 178.0f, 53.0f};
inline constexpr Rectangle kEditorDiscardDraftButton{391.0f, 331.0f, 178.0f, 53.0f};
inline constexpr Rectangle kEditorCancelDraftButton{591.0f, 331.0f, 178.0f, 53.0f};
inline constexpr Rectangle kEditorQuitSaveButton{382.0f, 386.0f, 196.0f, 42.0f};
} // namespace locus::app
