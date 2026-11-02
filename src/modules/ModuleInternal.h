#pragma once
#include "cyui/Core.h"
#include <string>
#include <cctype>

namespace cyui::detail {
inline int resizeText(ImGuiInputTextCallbackData *data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto *text = static_cast<std::string *>(data->UserData);
        text->resize(std::size_t(data->BufTextLen));
        data->Buf = text->data();
    }
    return 0;
}
inline bool inputText(const char *id, std::string &text, ImGuiInputTextFlags flags = 0,
                      ImVec2 multiline = {}) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    if (multiline.y > 0)
        return ImGui::InputTextMultiline(id, text.data(), text.capacity() + 1, multiline, flags, resizeText,
                                         &text);
    return ImGui::InputText(id, text.data(), text.capacity() + 1, flags, resizeText, &text);
}
inline std::string folded(std::string value) {
    for (auto &c : value)
        if (static_cast<unsigned char>(c) < 128)
            c = char(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
inline bool matches(const std::string &text, const std::string &query) {
    return folded(text).find(folded(query)) != std::string::npos;
}
} // namespace cyui::detail
