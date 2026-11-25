#include "cyui/modules/Timeline.h"
#include "ModuleInternal.h"
#include <cstdio>

namespace cyui {
std::optional<double> snapTimelineTarget(double time, ArrayView<double> targets, double tolerance) {
    if (!std::isfinite(time) || !std::isfinite(tolerance) || tolerance < 0) return {};
    std::optional<double> nearest;
    for (std::size_t index = 0; index < targets.count; ++index) {
        const double distance = std::abs(targets[index] - time);
        if (distance < tolerance || (!nearest && distance == tolerance)) {
            nearest = targets[index];
            tolerance = distance;
        }
    }
    return nearest;
}

bool timelineNavigator(const char *id, TimelineNavigatorState &state, Range domain, Range &visible,
                       ArrayView<TimelineInterval> intervals, ImVec2 size, std::optional<double> playhead) {
    if (!domain.valid() || !std::isfinite(domain.span())) { state.dragging = false; return false; }
    const auto before = visible;
    if (size.y <= 0) size.y = dp(32);
    CanvasState canvas;
    CanvasOptions options;
    options.grid = options.navigation = false;
    options.claimWheel = true;
    CanvasScope surface(id, canvas, size, options);
    const auto &frame = surface.frame();
    const auto bounds = frame.bounds;
    const float width = bounds.size().x;
    const auto &mouse = ImGui::GetIO().MousePos;
    const auto x = [&](double time) {
        return bounds.min.x + width * float(std::clamp((time - domain.min) / domain.span(), 0., 1.));
    };
    const auto time = domain.min + std::clamp(double((mouse.x - bounds.min.x) / width), 0., 1.) * domain.span();
    const auto place = [&](double start, double span) {
        span = std::min(domain.span(), span);
        const auto lo = std::clamp(start, domain.min, std::max(domain.min, domain.max - span));
        const Range next{lo, std::min(domain.max, lo + span)};
        if (next.valid()) visible = next;
    };
    if (!visible.valid() || !std::isfinite(visible.span()) || visible.span() >= domain.span()) visible = domain;
    else place(visible.min, visible.span());
    if (frame.hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        visible = domain;
        state.dragging = false;
    } else if (frame.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        state.grab = visible.contains(time) ? time - visible.min : visible.span() * .5;
        state.dragging = true;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::GetIO().AppFocusLost) state.dragging = false;
    if (state.dragging) place(time - state.grab, visible.span());
    if (frame.wheel) {
        const auto zoomed = detail::zoomView(visible, time, std::exp(-frame.wheel * .15), domain.span());
        place(zoomed.min, zoomed.span());
    }
    auto *draw = ImGui::GetWindowDrawList();
    const auto &p = palette();
    for (std::size_t n = 0; n < intervals.count; ++n) {
        const auto &interval = intervals[n];
        if (!interval.time.valid() || interval.time.max <= domain.min || interval.time.min >= domain.max) continue;
        draw->AddRectFilled({x(interval.time.min), bounds.min.y + dp(6)},
                            {std::max(x(interval.time.min) + dp(1), x(interval.time.max)), bounds.max.y - dp(6)},
                            alpha(categorical(interval.colorIndex), .65f), dp(2));
    }
    const ImVec2 start{x(visible.min), bounds.min.y + dp(2)}, end{x(visible.max), bounds.max.y - dp(2)};
    draw->AddRectFilled(start, end, p.selection, dp(3));
    draw->AddRect(start, end, p.accent, dp(3), 0, dp(1));
    if (playhead && std::isfinite(*playhead) && domain.contains(*playhead))
        draw->AddLine({x(*playhead), bounds.min.y}, {x(*playhead), bounds.max.y}, p.warning, dp(1));
    if (frame.hovered || state.dragging) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    return before.min != visible.min || before.max != visible.max;
}

TimelineResult timeline(const char *id, TimelineState &s, const std::vector<TimelineLane> &lanes,
                        const std::vector<TimelineClip> &clips, ArrayView<TimelineMarker> markers,
                        ImVec2 size, const TimelineOptions &o) {
    TimelineResult result;
    ScopedID viewScope(id);
    if (o.overview && o.overview->valid() && std::isfinite(o.overview->span())) {
        timelineNavigator("##overview", s.navigator, *o.overview, s.view, o.intervals, {size.x, dp(32)}, o.playhead);
        if (size.y > 0) size.y = std::max(dp(30), size.y - dp(32) - ImGui::GetStyle().ItemSpacing.y);
    } else s.navigator.dragging = false;
    CanvasState canvas;
    CanvasOptions options;
    options.grid = options.navigation = false;
    options.claimWheel = true;
    CanvasScope scope(id, canvas, size, options);
    const auto f = scope.frame();
    const auto r = f.bounds;
    auto *d = ImGui::GetWindowDrawList();
    auto &io = ImGui::GetIO();
    if ((s.markerMoving || s.laneMoving) &&
        (s.revision != o.revision || o.readOnly || ImGui::IsKeyPressed(ImGuiKey_Escape))) {
        if (s.markerMoving)
            result.markers.phase = EditPhase::cancel;
        s.markerMoving = s.laneMoving = false;
    }
    if (s.markerMoving)
        markers = s.markerPreview;
    if (!s.view.valid())
        s.view = {0, 10};
    const float head = std::min(dp(150), r.size().x * .3f), ticksHeight = ImGui::GetFrameHeight();
    const float ruler = ticksHeight + (o.intervals.count ? dp(15) : 0);
    const float left = r.min.x + head, width = std::max(dp(1), r.max.x - left);
    auto x = [&](double t) { return left + float((t - s.view.min) / s.view.span()) * width; };
    const auto timeAtMouse = [&] { return s.view.min + (io.MousePos.x - left) / width * s.view.span(); };
    auto snap = [&](double t) {
        if (io.KeyAlt) return t;
        if (const auto target = snapTimelineTarget(t, o.snapTargets, s.view.span() * dp(o.snapDistance) / width))
            return *target;
        return o.snapTime ? o.snapTime(t) : snapValue(t, o.snap);
    };
    auto clear = [&] {
        s.mode = 0;
        s.active = 0;
        s.before.clear();
        s.preview.clear();
    };
    if (s.mode > 0 && (s.revision != o.revision || o.readOnly || ImGui::IsKeyPressed(ImGuiKey_Escape))) {
        clear();
        result.clips.phase = EditPhase::cancel;
    }
    if (f.wheel != 0) {
        if (io.KeyCtrl)
            s.verticalScroll = std::max(0., s.verticalScroll - f.wheel * dp(40));
        else if (io.KeyShift)
            s.view = detail::panView(s.view, -f.wheel * s.view.span() * .1);
        else
            s.view = detail::zoomView(s.view, timeAtMouse(), std::exp(-f.wheel * .15));
    }
    if (f.hovered && ImGui::IsMouseClicked(2))
        s.panning = true;
    if (!ImGui::IsMouseDown(2))
        s.panning = false;
    if (s.panning)
        s.view = detail::panView(s.view, -io.MouseDelta.x / width * s.view.span());
    const double mouse = timeAtMouse();
    struct Row {
        ItemId id;
        float top, bottom;
    };
    std::vector<Row> rows;
    float y = r.min.y + ruler - float(s.verticalScroll);
    for (const auto &lane : lanes) {
        const float height = dp(std::max(24.f, lane.height));
        rows.push_back({lane.id, y, y + height});
        y += height;
    }
    const double full = y - r.min.y - ruler + s.verticalScroll;
    s.verticalScroll = std::clamp(s.verticalScroll, 0., std::max(0., full - (r.size().y - ruler)));
    for (const auto &row : rows)
        if (row.bottom > r.min.y + ruler && row.top < r.max.y) {
            d->AddRectFilled({r.min.x, std::max(row.top, r.min.y + ruler)}, {left, row.bottom},
                             palette().bg2);
            d->AddLine({r.min.x, row.bottom}, {r.max.x, row.bottom}, palette().line, dp(1));
        }
    for (std::size_t i = 0; i < lanes.size(); ++i)
        if (rows[i].bottom > r.min.y + ruler && rows[i].top < r.max.y)
            clippedText(d, {r.min.x + dp(8), std::max(rows[i].top, r.min.y + ruler) + dp(5)}, head - dp(16),
                        palette().textDim, lanes[i].label.c_str());
    const double desired = s.view.span() * dp(90) / width,
                 power = std::pow(10., std::floor(std::log10(std::max(1e-12, desired))));
    const double step = (desired / power < 2 ? 1 : desired / power < 5 ? 2 : 5) * power;
    detail::forEachTick(s.view.min, s.view.max, step, [&](double t) {
        const float px = x(t);
        d->AddLine({px, r.min.y}, {px, r.max.y}, palette().grid, dp(1));
        char text[48];
        std::snprintf(text, sizeof(text), "%.5g", t);
        const auto label = o.formatTime ? o.formatTime(t) : std::string(text);
        d->AddText({px + dp(3), r.min.y + dp(3)}, palette().textDim, label.c_str());
    });
    const auto &shown = s.mode > 0 ? s.preview : clips;
    const TimelineClip *hit = nullptr;
    const TimelineMarker *hitMarker = nullptr;
    const TimelineInterval *hitInterval = nullptr;
    for (std::size_t n = 0; n < o.intervals.count; ++n) {
        const auto &item = o.intervals[n];
        if (!item.time.valid() || item.time.max <= s.view.min || item.time.min >= s.view.max) continue;
        const float a = std::max(left, x(item.time.min));
        const Rect band{{a, r.min.y + ticksHeight + dp(2)},
                        {std::min(r.max.x, std::max(a + dp(2), x(item.time.max))), r.min.y + ruler - dp(2)}};
        const bool hover = f.hovered && band.contains(io.MousePos);
        d->AddRectFilled(band.min, band.max, alpha(categorical(item.colorIndex), hover ? .85f : .55f), dp(2));
        if (hover && (!hitInterval || item.time.span() < hitInterval->time.span())) hitInterval = &item;
    }
    if (hitInterval && !hitInterval->label.empty()) ImGui::SetTooltip("%s", hitInterval->label.c_str());
    int edge = 1;
    const Row *hoveredLane = nullptr;
    for (const auto &row : rows)
        if (io.MousePos.y >= row.top && io.MousePos.y < row.bottom)
            hoveredLane = &row;
    d->PushClipRect({left, r.min.y + ruler}, r.max, true);
    for (const auto &c : shown) {
        const auto it = std::find_if(rows.begin(), rows.end(), [&](const Row &a) { return a.id == c.lane; });
        if (it == rows.end() || c.start + c.duration < s.view.min || c.start > s.view.max ||
            it->bottom < r.min.y + ruler || it->top > r.max.y)
            continue;
        Rect rect{{x(c.start), it->top + dp(3)},
                  {std::max(x(c.start) + dp(2), x(c.start + c.duration)), it->bottom - dp(3)}};
        d->AddRectFilled(rect.min, rect.max, mix(palette().bg2, categorical(c.colorIndex), .45f), dp(3));
        d->AddRect(rect.min, rect.max, selected(s.selected, c.id) ? palette().accent : palette().line, dp(3),
                   0, dp(selected(s.selected, c.id) ? 2.f : 1.f));
        clippedText(d, {std::max(left, rect.min.x) + dp(5), rect.min.y + dp(3)},
                    std::min(r.max.x, rect.max.x) - std::max(left, rect.min.x) - dp(10), palette().text,
                    c.label.c_str());
        if (o.drawClip)
            o.drawClip(c, rect);
        if (rect.contains(io.MousePos)) {
            hit = &c;
            edge = std::abs(io.MousePos.x - rect.min.x) < dp(7)   ? 2
                   : std::abs(io.MousePos.x - rect.max.x) < dp(7) ? 3
                                                                  : 1;
        }
    }
    if (s.selection.max != s.selection.min) {
        const float a = x(std::min(s.selection.min, s.selection.max)),
                    b = x(std::max(s.selection.min, s.selection.max));
        d->AddRectFilled({a, r.min.y + ruler}, {b, r.max.y}, palette().selection);
    }
    d->PopClipRect();
    for (std::size_t i = 0; i < markers.count; ++i)
        if (s.view.contains(markers[i].time)) {
            const float px = x(markers[i].time);
            d->AddLine({px, r.min.y}, {px, r.max.y}, palette().warning, dp(1));
            d->AddText({px + dp(3), r.min.y + ruler}, palette().warning, markers[i].label.c_str());
            if (f.hovered && io.MousePos.y < r.min.y + ruler && std::abs(io.MousePos.x - px) < dp(7))
                hitMarker = &markers[i];
        }
    d->AddLine({x(o.playhead), r.min.y}, {x(o.playhead), r.max.y}, palette().accent, dp(2));
    if (f.hovered && ImGui::IsMouseClicked(0)) {
        s.focused = true;
        if (hitInterval) result.interval = hitInterval->id;
        else if (!o.readOnly && hitMarker) {
            s.markerMoving = true;
            s.markerId = hitMarker->id;
            s.grab = mouse;
            s.revision = o.revision;
            s.markerBefore.assign(markers.data, markers.data + markers.count);
            s.markerPreview = s.markerBefore;
            result.markers = {EditPhase::begin, s.markerPreview};
        } else if (!o.readOnly && io.MousePos.x < left && hoveredLane) {
            s.laneMoving = true;
            s.laneId = hoveredLane->id;
            s.revision = o.revision;
        } else if (io.MousePos.x >= left && io.MousePos.y < r.min.y + ruler) {
            result.seek = true;
            result.time = snap(mouse);
        } else if (hit) {
            const auto key = hit->id;
            if (!selected(s.selected, key) || io.KeyCtrl)
                selectItem(s.selected, key, io.KeyCtrl || io.KeyShift);
            const auto it =
                std::find_if(clips.begin(), clips.end(), [&](const TimelineClip &c) { return c.id == key; });
            if (!o.readOnly && it != clips.end() && !it->locked) {
                s.active = key;
                s.mode = edge;
                s.grab = mouse;
                s.before = s.preview = clips;
                s.revision = o.revision;
                result.clips = {EditPhase::begin, s.preview};
            }
        } else if (io.MousePos.x >= left) {
            s.selection = {mouse, mouse};
            s.anchor = mouse;
            s.mode = -1;
            result.seek = true;
            result.time = mouse;
            if (!io.KeyCtrl)
                s.selected.clear();
        }
    } else if (ImGui::IsMouseClicked(0) && !f.hovered)
        s.focused = false;
    if (s.markerMoving) {
        s.markerPreview = s.markerBefore;
        for (auto &m : s.markerPreview)
            if (m.id == s.markerId)
                m.time = snap(m.time + mouse - s.grab);
        if (result.markers.phase != EditPhase::begin)
            result.markers = {ImGui::IsMouseDown(0) ? EditPhase::update : EditPhase::commit, s.markerPreview};
        if (!ImGui::IsMouseDown(0))
            s.markerMoving = false;
    }
    if (s.laneMoving && !ImGui::IsMouseDown(0)) {
        if (hoveredLane && hoveredLane->id != s.laneId)
            result.lanes = {EditPhase::commit, reorderTimelineLane(lanes, s.laneId, hoveredLane->id)};
        s.laneMoving = false;
    }
    auto applyOperation = [&](TimelineOperation operation) {
        s.error = operation.error;
        if (operation.changed)
            result.clips = {EditPhase::commit, std::move(operation.clips)};
    };
    auto duplicate = [&]() {
        double first = HUGE_VAL, last = -HUGE_VAL;
        for (const auto &c : clips)
            if (selected(s.selected, c.id) && !c.locked) {
                first = std::min(first, c.start);
                last = std::max(last, c.start + c.duration);
            }
        if (first <= last)
            applyOperation(duplicateTimelineClips(clips, s.selected, std::max(o.minDuration, last - first),
                                                  o.allocateId));
    };
    if (f.hovered && ImGui::IsMouseClicked(1)) {
        s.contextMarker = hitMarker != nullptr;
        s.contextId = hitMarker ? hitMarker->id : hit ? hit->id : 0;
        if (hitMarker)
            s.markerLabel = hitMarker->label;
        else if (hit && !selected(s.selected, hit->id))
            s.selected = {hit->id};
        s.anchor = snap(mouse);
        ImGui::OpenPopup("##timelineMenu");
    }
    if (ImGui::BeginPopup("##timelineMenu")) {
        if (s.contextMarker) {
            if (detail::inputText("##markerLabel", s.markerLabel, ImGuiInputTextFlags_EnterReturnsTrue) &&
                !o.readOnly) {
                std::vector<TimelineMarker> values(markers.data, markers.data + markers.count);
                for (auto &m : values)
                    if (m.id == s.contextId)
                        m.label = s.markerLabel;
                result.markers = {EditPhase::commit, std::move(values)};
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem(trLabel("Remove marker").c_str(), nullptr, false, !o.readOnly)) {
                std::vector<TimelineMarker> values;
                for (std::size_t i = 0; i < markers.count; ++i)
                    if (markers[i].id != s.contextId)
                        values.push_back(markers[i]);
                result.markers = {EditPhase::commit, std::move(values)};
            }
        } else {
            if (ImGui::MenuItem(trLabel("Split at playhead").c_str(), "S", false,
                                !o.readOnly && bool(o.allocateId)))
                applyOperation(
                    splitTimelineClips(clips, s.selected, o.playhead, o.allocateId, o.minDuration));
            if (ImGui::MenuItem(trLabel("Duplicate").c_str(), "Ctrl+D", false,
                                !o.readOnly && bool(o.allocateId)))
                duplicate();
            if (ImGui::MenuItem(trLabel("Add marker").c_str(), nullptr, false,
                                !o.readOnly && bool(o.allocateId))) {
                auto fresh = o.allocateId();
                bool used = !fresh;
                for (std::size_t i = 0; i < markers.count; ++i)
                    used |= markers[i].id == fresh;
                if (!used) {
                    std::vector<TimelineMarker> values;
                    for (std::size_t i = 0; i < markers.count; ++i)
                        values.push_back(markers[i]);
                    values.push_back({fresh, s.anchor, tr("Marker")});
                    result.markers = {EditPhase::commit, std::move(values)};
                } else
                    s.error = "Allocator returned duplicate or zero ID";
            }
        }
        ImGui::EndPopup();
    }
    if (s.mode > 0) {
        if (ImGui::IsMouseDown(0)) {
            s.preview = s.before;
            // The grabbed clip snaps; the others follow by the same delta and keep their spacing.
            double delta = mouse - s.grab;
            for (const auto &c : s.before)
                if (c.id == s.active)
                    delta = s.mode == 3 ? snap(c.start + c.duration + delta) - (c.start + c.duration)
                                        : snap(c.start + delta) - c.start;
            for (auto &c : s.preview)
                if (!c.locked && ((s.mode == 1 && selected(s.selected, c.id)) || c.id == s.active)) {
                    if (s.mode == 1) {
                        c.start += delta;
                        if (c.id == s.active && hoveredLane)
                            c.lane = hoveredLane->id;
                    } else if (s.mode == 2) {
                        const double end = c.start + c.duration;
                        c.start = std::min(end - o.minDuration, c.start + delta);
                        c.duration = end - c.start;
                    } else
                        c.duration = std::max(o.minDuration, c.duration + delta);
                }
            if (result.clips.phase != EditPhase::begin && (io.MouseDelta.x || io.MouseDelta.y))
                result.clips = {EditPhase::update, s.preview};
        } else {
            result.clips = {EditPhase::commit, s.preview};
            clear();
        }
    }
    if (s.mode == -1) {
        s.selection = {std::min(s.anchor, mouse), std::max(s.anchor, mouse)};
        result.rangeChanged = true;
        if (!ImGui::IsMouseDown(0))
            s.mode = 0;
    }
    if (s.focused && f.focused && !io.WantTextInput && !o.readOnly)
        ImGui::SetNextFrameWantCaptureKeyboard(true); // Hosts (Unreal) route Delete here.
    if (s.focused && f.focused && !io.WantTextInput && !o.readOnly && ImGui::IsKeyPressed(ImGuiKey_Delete))
        for (const auto &c : clips)
            if (!c.locked && selected(s.selected, c.id))
                result.remove.push_back(c.id);
    if (s.focused && f.focused && !io.WantTextInput && !o.readOnly && o.allocateId) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D))
            duplicate();
        if (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
            applyOperation(splitTimelineClips(clips, s.selected, o.playhead, o.allocateId, o.minDuration));
    }
    return result;
}
} // namespace cyui
