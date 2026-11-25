#pragma once
#include <RmlUi/Core/Element.h>
#include "raylib.h"

namespace locus::app {
// Reusable bridge for any RmlUi-hosted custom rendering surface.
// Read the layout engine's actual border box, never duplicate RCSS rules.
inline Rectangle RmlElementBounds(Rml::Element* element) noexcept {
    if (!element) return {};
    return {element->GetAbsoluteLeft(), element->GetAbsoluteTop(),
            static_cast<float>(element->GetOffsetWidth()),
            static_cast<float>(element->GetOffsetHeight())};
}
} // namespace locus::app
