#pragma once

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementText.h>
#include <string>

namespace locus::app {

// Shared escaping for generated RML text and quoted attribute values.
// Keep user-controlled UTF-8 intact; escape only markup delimiters.
inline std::string EscapeRml(const std::string& source) {
    std::string out;
    out.reserve(source.size());
    for (char c : source) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c; break;
        }
    }
    return out;
}

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
    const auto escaped = EscapeRml(value);
    if (element->GetTagName() == "button")
        element->SetInnerRML("<span class='button-label'>" +
                             escaped + "</span>");
    else element->SetInnerRML(escaped);
}

} // namespace locus::app
