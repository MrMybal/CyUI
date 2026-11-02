#pragma once
#include "cyui/modules/Canvas.h"
#include <functional>
#include <string>

namespace cyui {
struct TimelineLane {
    ItemId id = 0;
    std::string label;
    float height = 56;
};
struct TimelineClip {
    ItemId id = 0, lane = 0;
    double start = 0, duration = 1;
    std::string label;
    int colorIndex = 0;
    bool locked = false;
};
struct TimelineMarker {
    ItemId id = 0;
    double time = 0;
    std::string label;
};
struct TimelineState {
    Range view{0, 10}, selection{0, 0};
    double verticalScroll = 0;
    std::vector<ItemId> selected;
    std::vector<TimelineClip> before, preview;
    ItemId active = 0;
    double grab = 0;
    int mode = 0;
    bool focused = false, panning = false;
    std::uint64_t revision = 0;
};
struct TimelineOptions {
    double playhead = 0, snap = 0, minDuration = 1e-6;
    bool readOnly = false;
    std::uint64_t revision = 0;
    std::function<double(double)> snapTime;
    std::function<std::string(double)> formatTime;
    std::function<void(const TimelineClip &, const Rect &)> drawClip;
};
struct TimelineResult {
    Edit<std::vector<TimelineClip>> clips;
    bool seek = false, rangeChanged = false;
    double time = 0;
    std::vector<ItemId> remove;
};
TimelineResult timeline(const char *id, TimelineState &, const std::vector<TimelineLane> &,
                        const std::vector<TimelineClip> &, ArrayView<TimelineMarker>, ImVec2 size,
                        const TimelineOptions & = {});
} // namespace cyui
