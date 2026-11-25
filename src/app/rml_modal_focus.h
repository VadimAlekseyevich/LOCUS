#pragma once

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/StyleTypes.h>
#include <algorithm>
#include <vector>

namespace locus::app {

// Modal focus is scoped to the declared Rml subtree, not to a list of
// hard-coded control IDs. New buttons automatically join the tab chain.
inline bool RmlFocusWithin(const Rml::Element* panel,
                           Rml::Element* focus) noexcept {
    if (!panel) return false;
    for (; focus; focus = focus->GetParentNode())
        if (focus == panel) return true;
    return false;
}

inline void CollectModalFocus(Rml::Element* element,
                              std::vector<Rml::Element*>& controls) {
    if (!element || element->GetDisplay() == Rml::Style::Display::None ||
        element->HasAttribute("disabled")) return;
    const auto& tag = element->GetTagName();
    if (tag == "button" || tag == "input") {
        controls.push_back(element);
        return;
    }
    for (int i = 0; i < element->GetNumChildren(); ++i)
        CollectModalFocus(element->GetChild(i), controls);
}

inline bool MoveRmlModalFocus(Rml::Element* panel, Rml::Element* focused,
                             bool backwards) {
    std::vector<Rml::Element*> controls;
    CollectModalFocus(panel, controls);
    if (controls.empty()) return false;
    const auto found = std::find(controls.begin(), controls.end(), focused);
    const auto count = static_cast<int>(controls.size());
    const int first = found == controls.end() ? (backwards ? count-1 : 0) :
        (static_cast<int>(found-controls.begin())+(backwards ? count-1 : 1))%count;
    for (int step = 0; step < count; ++step) {
        const auto index = (first+(backwards ? count-step : step))%count;
        if (controls[static_cast<std::size_t>(index)]->Focus()) return true;
    }
    return false;
}

inline void ActivateRmlModalButton(Rml::Element* panel, Rml::Element* focused) {
    if (RmlFocusWithin(panel, focused) && focused->GetTagName() == "button" &&
        !focused->HasAttribute("disabled"))
        focused->Click();
    else if (!RmlFocusWithin(panel, focused))
        MoveRmlModalFocus(panel, focused, false);
}

} // namespace locus::app
