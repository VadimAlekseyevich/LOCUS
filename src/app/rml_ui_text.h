#pragma once

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementText.h>
#include <string>

namespace locus::app {

// Updating a text node must not parse and replace the complete RML subtree.
// This matters for the editor's millisecond readout, which changes every
// frame, and also preserves button focus, hover and stable child identity.
inline void UpdateRmlText(Rml::Element* element, const std::string& value) {
    if (!element) return;
    Rml::Element* content = element;
    if (element->GetTagName() == "button" &&
        element->GetNumChildren() == 1) {
        auto* child = element->GetChild(0);
        if (child && child->IsClassSet("button-label"))
            content = child;
    }
    if (content->GetNumChildren() == 1) {
        if (auto* text = dynamic_cast<Rml::ElementText*>(content->GetChild(0))) {
            text->SetText(value);
            return;
        }
    }
    // Defensive path for a missing/changed RML template. Only reparse when
    // there is no usable text node. All flex buttons still get an explicit
    // child, since anonymous text children disappear in RmlUi flex layout.
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped += ch; break;
        }
    }
    if (element->GetTagName() == "button")
        element->SetInnerRML("<span class='button-label'>" +
                             escaped + "</span>");
    else element->SetInnerRML(escaped);
}

} // namespace locus::app
