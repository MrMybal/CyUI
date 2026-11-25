#include "cyui/modules/Images.h"
#include "cyui/CoreWidgets.h"
#include <limits>

namespace cyui {
namespace {
bool valid(const ImageData &image) { return image.width > 0 && image.height > 0; }

void applyView(ImageState &state, Point extent, ImVec2 top, ImVec2 room, bool fit) {
    if (state.fitPending || fit) {
        state.canvas.view.fit({}, extent, room, dp(12));
        state.fitPending = false;
    }
    if (state.zoomRequest) {
        const double zoom = *state.zoomRequest;
        if (std::isfinite(zoom) && zoom > 0)
            state.canvas.view.zoomAt({top.x + room.x * .5f, top.y + room.y * .5f},
                                     std::clamp(zoom, .05, 64.), top);
        state.zoomRequest.reset();
    }
}

void drawImage(ImDrawList *draw, const ImageData &image, const CanvasView &view, ImVec2 origin, const Rect &clip) {
    if (!valid(image)) return;
    const auto a = view.screen({}, origin), b = view.screen({double(image.width), double(image.height)}, origin);
    const float tile = dp(16);
    const float left = std::max(a.x, clip.min.x), top = std::max(a.y, clip.min.y);
    const float right = std::min(b.x, clip.max.x), bottom = std::min(b.y, clip.max.y);
    if (right <= left || bottom <= top) return;
    const float startX = left - std::fmod(left - a.x, tile), startY = top - std::fmod(top - a.y, tile);
    int row = std::fmod((top - a.y) / tile, 2.f) >= 1.f;
    const int firstColumn = std::fmod((left - a.x) / tile, 2.f) >= 1.f;
    for (float y = startY; y < bottom; y += tile, ++row) {
        int column = firstColumn;
        for (float x = startX; x < right; x += tile, ++column) {
            draw->AddRectFilled({std::max(x, left), std::max(y, top)},
                                {std::min(x + tile, right), std::min(y + tile, bottom)},
                                (row + column) % 2 ? palette().bg2 : palette().bg3);
        }
    }
    if (image.texture) draw->AddImage(image.texture, a, b, image.uvMin, image.uvMax);
}
} // namespace

ImageResult imageViewport(const char *id, ImageState &s, const ImageData &image, ImVec2 size,
                          ImageTool tool) {
    CanvasOptions options;
    options.grid = false;
    options.leftPan = tool == ImageTool::inspect && valid(image);
    CanvasScope scope(id, s.canvas, size, options);
    const auto f = scope.frame();
    auto *d = ImGui::GetWindowDrawList();
    auto &io = ImGui::GetIO();
    ImageResult result;
    if (s.cropping && (tool != ImageTool::crop || s.canvas.panning || !valid(image) || io.AppFocusLost)) {
        s.cropping = false;
        s.crop = s.before;
        result.crop = {EditPhase::cancel, s.before};
    }
    if (!valid(image)) return result;
    applyView(s, {double(image.width), double(image.height)}, f.bounds.min, f.bounds.size(),
              f.hovered && !s.canvas.panning && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left));
    drawImage(d, image, s.canvas.view, f.bounds.min, f.bounds);
    const auto mouse = s.canvas.view.world(io.MousePos, f.bounds.min);
    if (f.hovered && mouse.x >= 0 && mouse.y >= 0 && mouse.x < image.width && mouse.y < image.height) {
        result.pixelX = int(mouse.x);
        result.pixelY = int(mouse.y);
        result.clicked = options.leftPan ? f.leftClicked : !s.canvas.panning && ImGui::IsMouseClicked(0);
    }
    if (tool == ImageTool::crop && !s.canvas.panning) {
        if (result.clicked) {
            s.start = mouse;
            s.before = s.crop;
            s.cropping = true;
            result.crop.phase = EditPhase::begin;
        }
        if (s.cropping) {
            const ImVec2 low{float(std::clamp(std::min(s.start.x, mouse.x), 0., double(image.width))),
                             float(std::clamp(std::min(s.start.y, mouse.y), 0., double(image.height)))};
            const ImVec2 high{float(std::clamp(std::max(s.start.x, mouse.x), 0., double(image.width))),
                              float(std::clamp(std::max(s.start.y, mouse.y), 0., double(image.height)))};
            s.crop = {low, high};
            result.crop.value = s.crop;
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                s.cropping = false;
                s.crop = s.before;
                result.crop.value = s.before;
                result.crop.phase = EditPhase::cancel;
            } else if (!ImGui::IsMouseDown(0)) {
                s.cropping = false;
                result.crop.phase = EditPhase::commit;
            } else if (result.crop.phase != EditPhase::begin && (io.MouseDelta.x || io.MouseDelta.y))
                result.crop.phase = EditPhase::update;
        }
    }
    if (s.cropping || s.crop.size().x > 0) {
        const auto low = s.canvas.view.screen({s.crop.min.x, s.crop.min.y}, f.bounds.min),
                   high = s.canvas.view.screen({s.crop.max.x, s.crop.max.y}, f.bounds.min);
        d->AddRectFilled(low, high, palette().selection);
        d->AddRect(low, high, palette().accent, 0, 0, dp(2));
    }
    return result;
}

void imageViewToolbar(const char *id, ImageState &state) {
    ScopedID scope(id);
    if (button(trLabel("Fit").c_str())) {
        state.fitPending = true;
        state.zoomRequest.reset();
    }
    ImGui::SameLine();
    if (button("100%##image-actual")) state.zoomRequest = 1.;
    ImGui::SameLine();
    if (iconButton(icon::zoomOut, tr("Zoom out").c_str())) state.zoomRequest = state.canvas.view.zoom / 1.25;
    ImGui::SameLine();
    if (iconButton(icon::zoomIn, tr("Zoom in").c_str())) state.zoomRequest = state.canvas.view.zoom * 1.25;
    ImGui::SameLine();
    textDim("%.0f%%", state.canvas.view.zoom * 100.);
}

ImageCompareResult imageCompare(const char *id, ImageCompareState &state, const ImageData &first,
                                const ImageData &second, ImVec2 size, const ImageCompareOptions &options) {
    ScopedID scope(id);
    const auto side = tr("Side by side"), wipe = tr("Wipe"), blink = tr("Blink");
    const char *modes[]{side.c_str(), wipe.c_str(), blink.c_str()};
    int mode = std::clamp(int(state.mode), 0, 2);
    if (combo(trLabel("Comparison##image-mode").c_str(), &mode, modes, 3, dp(144)))
        state.mode = ImageCompareMode(mode);
    imageViewToolbar("##view", state.view);

    const auto top = ImGui::GetCursorScreenPos();
    const auto available = ImGui::GetContentRegionAvail();
    const float width = std::max(dp(1), size.x > 0 ? size.x : available.x + size.x);
    const float height = std::max(dp(1), size.y > 0 ? size.y : available.y + size.y);
    const bool paired = state.mode == ImageCompareMode::sideBySide;
    const bool wiping = state.mode == ImageCompareMode::wipe;
    const ImVec2 room{paired ? width * .5f : width, height};
    auto &io = ImGui::GetIO();
    if (!std::isfinite(state.divider)) state.divider = .5f;
    state.divider = std::clamp(state.divider, 0.f, 1.f);
    const bool handle = wiping && std::abs(io.MousePos.x - (top.x + width * state.divider)) <= dp(9);
    const bool rightPane = paired && io.MousePos.x >= top.x + room.x;
    if (state.previousPaneSize.x > 0 && !state.view.fitPending && state.view.canvas.view.zoom > 0) {
        state.view.canvas.view.origin.x += (state.previousPaneSize.x - room.x) * .5 / state.view.canvas.view.zoom;
        state.view.canvas.view.origin.y += (state.previousPaneSize.y - room.y) * .5 / state.view.canvas.view.zoom;
    }
    state.previousPaneSize = room;
    CanvasOptions canvasOptions;
    canvasOptions.grid = false;
    canvasOptions.leftPan = !handle && !state.draggingDivider;
    canvasOptions.zoomOriginOffset.x = rightPane ? room.x : 0;
    CanvasScope canvas("##comparison", state.view.canvas, {width, height}, canvasOptions);
    const auto &frame = canvas.frame();
    ImageCompareResult result;
    if (frame.hovered && handle && !state.view.canvas.panning && !ImGui::IsKeyDown(ImGuiKey_Space) &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) state.draggingDivider = true;
    if (!wiping || !ImGui::IsMouseDown(ImGuiMouseButton_Left) || io.AppFocusLost) state.draggingDivider = false;
    if (state.draggingDivider) {
        const auto split = std::clamp((io.MousePos.x - top.x) / width, 0.f, 1.f);
        result.dividerChanged = split != state.divider;
        state.divider = split;
    }
    if ((handle && frame.hovered) || state.draggingDivider) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (!valid(first) && !valid(second)) return result;
    const Point extent{double(std::max(first.width, second.width)), double(std::max(first.height, second.height))};
    applyView(state.view, extent, top, room,
              frame.hovered && !handle && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left));
    auto *draw = ImGui::GetWindowDrawList();
    const auto &view = state.view.canvas.view;
    const float split = top.x + width * (paired ? .5f : state.divider);
    const bool click = frame.leftClicked && !state.draggingDivider;
    float smallest = std::numeric_limits<float>::infinity();
    const auto pane = [&](const ImageData &image, ImVec2 origin, Rect clip, const char *label, bool labelOnRight) {
        if (clip.size().x <= 0) return;
        draw->PushClipRect(clip.min, clip.max, true);
        drawImage(draw, image, view, origin, clip);
        const auto screen = [&](ImVec2 p) { return view.screen({p.x, p.y}, origin); };
        for (std::size_t n = 0; n < options.annotations.count; ++n) {
            const auto &annotation = options.annotations[n];
            if (annotation.region.size().x <= 0 || annotation.region.size().y <= 0) continue;
            const Rect box{screen(annotation.region.min), screen(annotation.region.max)};
            const bool selected = options.selected == annotation.id;
            const auto color = selected ? palette().accent : categorical(annotation.colorIndex);
            if (selected) draw->AddRectFilled(box.min, box.max, alpha(color, .18f));
            draw->AddRect(box.min, box.max, color, 0, 0, dp(selected ? 2.f : 1.f));
            const float area = box.size().x * box.size().y;
            if (frame.hovered && clip.contains(io.MousePos) && box.contains(io.MousePos) && area < smallest) {
                smallest = area;
                if (click) result.selected = annotation.id;
                if (!annotation.label.empty() && !state.draggingDivider) ImGui::SetTooltip("%s", annotation.label.c_str());
            }
        }
        if (label && *label) {
            const auto title = tr(label);
            const auto textSize = ImGui::CalcTextSize(title.c_str());
            const float x = labelOnRight ? clip.max.x - textSize.x - dp(12) : clip.min.x + dp(12);
            draw->AddRectFilled({x - dp(5), top.y + dp(5)}, {x + textSize.x + dp(5), top.y + textSize.y + dp(11)}, palette().bg2, dp(3));
            draw->AddText({x, top.y + dp(8)}, palette().text, title.c_str());
        }
        draw->PopClipRect();
    };
    if (wiping || paired) {
        pane(first, top, {top, {split, top.y + height}}, options.firstLabel, false);
        pane(second, {paired ? split : top.x, top.y}, {{split, top.y}, {top.x + width, top.y + height}}, options.secondLabel, true);
        draw->AddLine({split, top.y}, {split, top.y + height}, wiping ? palette().accent : palette().lineStrong, dp(2));
        if (wiping) draw->AddCircleFilled({split, top.y + height * .5f}, dp(5), palette().accent);
    } else {
        const double seconds = std::isfinite(options.blinkSeconds) ? std::clamp(options.blinkSeconds, .1, 10.) : .6;
        const bool alternate = std::fmod(ImGui::GetTime(), seconds * 2) >= seconds;
        pane(alternate ? second : first, top, frame.bounds, alternate ? options.secondLabel : options.firstLabel, false);
    }
    return result;
}
ImVec4 sampleGradient(ArrayView<GradientStop> stops, double t, bool linear) {
    if (!stops.count)
        return {};
    const GradientStop *left = nullptr, *right = nullptr;
    for (std::size_t i = 0; i < stops.count; ++i) {
        const auto &s = stops[i];
        if (s.position <= t && (!left || s.position >= left->position))
            left = &s;
        if (s.position > t && (!right || s.position < right->position))
            right = &s;
    }
    if (!left) {
        left = &stops[0];
        for (std::size_t i = 1; i < stops.count; ++i)
            if (stops[i].position < left->position)
                left = &stops[i];
        return left->color;
    }
    if (!right)
        return left->color;
    const float k = float((t - left->position) / (right->position - left->position));
    auto blend = [&](float a, float b) {
        if (!linear)
            return a + (b - a) * k;
        auto decode = [](float x) {
            return x <= .04045f ? x / 12.92f : std::pow((x + .055f) / 1.055f, 2.4f);
        };
        const float v = decode(a) + (decode(b) - decode(a)) * k;
        return v <= .0031308f ? v * 12.92f : 1.055f * std::pow(v, 1 / 2.4f) - .055f;
    };
    return {blend(left->color.x, right->color.x), blend(left->color.y, right->color.y),
            blend(left->color.z, right->color.z), left->color.w + (right->color.w - left->color.w) * k};
}
Edit<std::vector<GradientStop>> gradientEditor(const char *id, GradientState &s,
                                               const std::vector<GradientStop> &stops, ImVec2 size,
                                               bool linear, std::uint64_t revision) {
    if (size.y <= 0)
        size.y = dp(48);
    CanvasState canvas;
    CanvasOptions options;
    options.grid = options.navigation = false;
    CanvasScope scope(id, canvas, size, options);
    const auto f = scope.frame();
    const auto room = f.bounds.size();
    auto *d = ImGui::GetWindowDrawList();
    auto &io = ImGui::GetIO();
    Edit<std::vector<GradientStop>> result;
    if (s.active && (s.revision != revision || ImGui::IsKeyPressed(ImGuiKey_Escape))) {
        s.active = false;
        s.dragging = 0;
        s.preview.clear();
        result.phase = EditPhase::cancel;
    }
    const auto &shown = s.active ? s.preview : stops;
    const float barBottom = f.bounds.max.y - dp(14);
    const int samples = std::clamp(int(room.x / dp(2)), 1, 4096);
    for (int i = 0; i < samples; ++i) {
        const auto color = sampleGradient(shown, (i + .5) / samples, linear);
        d->AddRectFilled({f.bounds.min.x + room.x * i / samples, f.bounds.min.y},
                         {f.bounds.min.x + room.x * (i + 1) / samples, barBottom},
                         ImGui::ColorConvertFloat4ToU32(color));
    }
    const GradientStop *hit = nullptr;
    for (const auto &stop : shown) {
        const float x = f.bounds.min.x + room.x * float(stop.position);
        const ImVec2 a{x, barBottom}, b{x - dp(6), barBottom + dp(10)}, c{x + dp(6), barBottom + dp(10)};
        d->AddTriangleFilled(a, b, c, ImGui::ColorConvertFloat4ToU32(stop.color));
        d->AddTriangle(a, b, c, s.hasSelection && s.selected == stop.id ? palette().accent : palette().text,
                       dp(1.5f));
        if (std::abs(io.MousePos.x - x) < dp(8))
            hit = &stop;
    }
    const double mouse = std::clamp(double((io.MousePos.x - f.bounds.min.x) / room.x), 0., 1.);
    if (f.hovered && ImGui::IsMouseDoubleClicked(0) && !hit) {
        result.value = stops;
        ItemId next = 1;
        for (const auto &v : stops)
            next = std::max(next, v.id + 1);
        result.value.push_back({next, mouse, sampleGradient(stops, mouse, linear)});
        s.selected = next;
        s.hasSelection = true;
        result.phase = EditPhase::commit;
    } else if (f.hovered && ImGui::IsMouseClicked(0) && hit) {
        s.selected = s.dragging = hit->id;
        s.hasSelection = s.active = true;
        s.before = s.preview = stops;
        s.revision = revision;
        result = {EditPhase::begin, s.preview};
    }
    if (s.active) {
        if (ImGui::IsMouseDown(0)) {
            for (auto &stop : s.preview)
                if (stop.id == s.dragging)
                    stop.position = mouse;
            if (result.phase != EditPhase::begin && io.MouseDelta.x)
                result = {EditPhase::update, s.preview};
        } else {
            result = {EditPhase::commit, s.preview};
            s.active = false;
            s.dragging = 0;
        }
    }
    if (f.hovered && hit && ImGui::IsMouseClicked(1) && stops.size() > 2) {
        const auto key = hit->id;
        result.value = stops;
        result.value.erase(std::remove_if(result.value.begin(), result.value.end(),
                                          [&](const GradientStop &p) { return p.id == key; }),
                           result.value.end());
        result.phase = EditPhase::commit;
    }
    return result;
}
} // namespace cyui
