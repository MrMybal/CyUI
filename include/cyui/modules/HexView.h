#pragma once

#include "cyui/Core.h"
#include "cyui/Interaction.h"
#include <functional>
#include <optional>
#include <string>

namespace cyui {

struct HexSource {
    std::uint64_t size = 0;
    // Called on the UI thread for the visible bytes or a bounded copy. Return bytes written.
    std::function<std::size_t(std::uint64_t offset, std::uint8_t *destination, std::size_t count)> read;
};
struct HexRange {
    std::uint64_t offset = 0, length = 0;
};
struct HexHighlight {
    HexRange range;
    int colorIndex = 0;
    std::string label;
};
struct HexViewState {
    std::uint64_t topOffset = 0;
    std::optional<std::uint64_t> anchor, caret, reveal;
    std::string offsetInput;
    bool selecting = false, scrollbar = false;
};
struct HexViewOptions {
    int bytesPerRow = 16;
    ArrayView<HexHighlight> highlights;
    std::size_t clipboardBytes = 1024 * 1024;
};
struct HexViewResult {
    std::optional<HexRange> selection;
    std::optional<std::uint64_t> hovered;
    bool selectionChanged = false;
};

// Virtual, read-only byte view. Set state.reveal to scroll to a byte next draw.
CYUI_API HexViewResult hexView(const char *id, HexViewState &, const HexSource &, ImVec2 size = {},
                               const HexViewOptions & = {});

} // namespace cyui
