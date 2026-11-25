#pragma once
#include "cyui/modules/Canvas.h"
#include <functional>
#include <string>
#include <optional>

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
struct TimelineInterval {
    ItemId id = 0;
    Range time;
    std::string label;
    int colorIndex = 0;
};
struct TimelineNavigatorState {
    bool dragging = false;
    double grab = 0;
};
// Draw an overview for a host-owned time range. Returns true if the visible range changed.
CYUI_API bool timelineNavigator(const char *id, TimelineNavigatorState &, Range domain, Range &visible,
                                ArrayView<TimelineInterval> = {}, ImVec2 size = {},
                                std::optional<double> playhead = {});
// Nearest finite target within tolerance.
CYUI_API std::optional<double> snapTimelineTarget(double time, ArrayView<double> targets, double tolerance);
struct TimelineState {
    Range view{0, 10}, selection{0, 0}; // selection is kept ordered (min <= max)
    double verticalScroll = 0;
    std::vector<ItemId> selected;
    std::vector<TimelineClip> before, preview;
    ItemId active = 0;
    double grab = 0;
    int mode = 0;
    bool focused = false, panning = false;
    std::uint64_t revision = 0;
    double anchor = 0; // where a range-selection drag started
    bool markerMoving = false, laneMoving = false;
    ItemId markerId = 0, laneId = 0, contextId = 0;
    bool contextMarker = false;
    std::vector<TimelineMarker> markerBefore, markerPreview;
    std::string markerLabel, error;
    TimelineNavigatorState navigator;
};
struct TimelineOptions {
    double playhead = 0, snap = 0, minDuration = 1e-6;
    bool readOnly = false;
    std::uint64_t revision = 0;
    std::function<double(double)> snapTime;
    std::function<std::string(double)> formatTime;
    std::function<void(const TimelineClip &, const Rect &)> drawClip;
    std::function<ItemId()> allocateId;
    std::optional<Range> overview;
    ArrayView<TimelineInterval> intervals;
    ArrayView<double> snapTargets;
    float snapDistance = 8; // design pixels
};
// Lane and clip IDs may be any value, 0 included.
struct TimelineResult {
    Edit<std::vector<TimelineClip>> clips;
    bool seek = false, rangeChanged = false;
    double time = 0;
    std::vector<ItemId> remove;
    Edit<std::vector<TimelineMarker>> markers;
    Edit<std::vector<TimelineLane>> lanes;
    std::optional<ItemId> interval;
};
struct TimelineOperation {
    std::vector<TimelineClip> clips;
    std::string error;
    bool changed = false;
};
CYUI_API TimelineOperation splitTimelineClips(ArrayView<TimelineClip>, ArrayView<ItemId>, double time,
                                              const std::function<ItemId()> &, double minDuration = 1e-6);
CYUI_API TimelineOperation duplicateTimelineClips(ArrayView<TimelineClip>, ArrayView<ItemId>, double offset,
                                                  const std::function<ItemId()> &);
CYUI_API std::vector<TimelineLane> reorderTimelineLane(ArrayView<TimelineLane>, ItemId lane,
                                                       std::optional<ItemId> before = std::nullopt);
CYUI_API TimelineResult timeline(const char *id, TimelineState &, const std::vector<TimelineLane> &,
                                 const std::vector<TimelineClip> &, ArrayView<TimelineMarker>, ImVec2 size,
                                 const TimelineOptions & = {});
} // namespace cyui
