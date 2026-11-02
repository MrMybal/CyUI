#include "cyui/modules/Nodes.h"
#include <unordered_map>
#include <unordered_set>

namespace cyui {
std::vector<NodeMove> arrangeNodes(const std::vector<Node> &nodes, const std::vector<NodeLink> &links,
                                   Point gap) {
    std::unordered_map<ItemId, ItemId> owners;
    std::unordered_map<ItemId, int> indegree, rank, rows;
    std::unordered_map<ItemId, std::vector<ItemId>> next;
    for (const auto &n : nodes) {
        indegree[n.id] = 0;
        for (const auto &p : n.inputs)
            owners[p.id] = n.id;
        for (const auto &p : n.outputs)
            owners[p.id] = n.id;
    }
    for (const auto &l : links)
        if (owners.count(l.from) && owners.count(l.to)) {
            const auto a = owners[l.from], b = owners[l.to];
            if (a != b) {
                next[a].push_back(b);
                ++indegree[b];
            }
        }
    std::vector<ItemId> queue;
    for (const auto &n : nodes)
        if (!indegree[n.id])
            queue.push_back(n.id);
    for (std::size_t i = 0; i < queue.size(); ++i)
        for (auto b : next[queue[i]]) {
            rank[b] = std::max(rank[b], rank[queue[i]] + 1);
            if (--indegree[b] == 0)
                queue.push_back(b);
        }
    std::vector<NodeMove> result;
    for (const auto &n : nodes) {
        const int column = rank[n.id];
        result.push_back({n.id, {column * gap.x, rows[ItemId(column)]++ * gap.y}});
    }
    return result;
}
NodeResult nodeEditor(const char *id, NodeState &s, const std::vector<Node> &nodes,
                      const std::vector<NodeLink> &links, ImVec2 size, const NodeOptions &o) {
    NodeResult result;
    CanvasScope scope(id, s.canvas, size);
    const auto f = scope.frame();
    auto *d = ImGui::GetWindowDrawList();
    auto &io = ImGui::GetIO();
    const float z = float(s.canvas.view.zoom), w = dp(o.width) * z, header = dp(o.header) * z,
                row = dp(o.row) * z;
    struct Card {
        const Node *n;
        Rect rect;
    };
    struct Pin {
        ItemId id, node;
        ImVec2 p;
        bool output;
    };
    std::vector<Card> cards;
    std::vector<Pin> pins;
    auto clear = [&] {
        s.dragging = s.boxing = false;
        s.wire = 0;
        s.preview.clear();
        s.before.clear();
    };
    if ((s.dragging || s.wire) &&
        (s.revision != o.revision || o.readOnly || ImGui::IsKeyPressed(ImGuiKey_Escape))) {
        clear();
        result.moved.phase = EditPhase::cancel;
    }
    for (const auto &n : nodes) {
        Point pos = n.position;
        if (s.dragging)
            for (const auto &p : s.preview)
                if (p.id == n.id)
                    pos = p.position;
        const auto a = s.canvas.view.screen(pos, f.bounds.min);
        const float h =
            header + row * std::max<std::size_t>(1, std::max(n.inputs.size(), n.outputs.size())) + dp(8) * z;
        cards.push_back({&n, {a, {a.x + w, a.y + h}}});
        for (std::size_t i = 0; i < n.inputs.size(); ++i)
            pins.push_back({n.inputs[i].id, n.id, {a.x, a.y + header + row * (float(i) + .5f)}, false});
        for (std::size_t i = 0; i < n.outputs.size(); ++i)
            pins.push_back({n.outputs[i].id, n.id, {a.x + w, a.y + header + row * (float(i) + .5f)}, true});
    }
    auto pinAt = [&](ItemId id) -> const Pin * {
        for (const auto &p : pins)
            if (p.id == id)
                return &p;
        return nullptr;
    };
    auto wire = [&](ImVec2 a, ImVec2 b, ImU32 color) {
        const float bend = std::max(dp(35) * z, std::abs(b.x - a.x) * .5f);
        d->AddBezierCubic(a, {a.x + bend, a.y}, {b.x - bend, b.y}, b, color, dp(2));
    };
    ItemId hitLink = 0;
    float linkDistance = dp(7);
    for (const auto &l : links) {
        const auto *a = pinAt(l.from), *b = pinAt(l.to);
        if (!a || !b)
            continue;
        wire(a->p, b->p, categorical(l.colorIndex));
        const float bend = std::max(dp(35) * z, std::abs(b->p.x - a->p.x) * .5f);
        ImVec2 previous = a->p;
        for (int sample = 1; sample <= 32; ++sample) {
            const float t = sample / 32.f, u = 1 - t;
            const ImVec2 q{u * u * u * a->p.x + 3 * u * u * t * (a->p.x + bend) +
                               3 * u * t * t * (b->p.x - bend) + t * t * t * b->p.x,
                           u * u * u * a->p.y + 3 * u * u * t * a->p.y + 3 * u * t * t * b->p.y +
                               t * t * t * b->p.y};
            const float dx = q.x - previous.x, dy = q.y - previous.y,
                        k = std::clamp(
                            ((io.MousePos.x - previous.x) * dx + (io.MousePos.y - previous.y) * dy) /
                                std::max(1e-6f, dx * dx + dy * dy),
                            0.f, 1.f);
            const float distance =
                std::hypot(io.MousePos.x - previous.x - k * dx, io.MousePos.y - previous.y - k * dy);
            if (distance < linkDistance) {
                linkDistance = distance;
                hitLink = l.id;
            }
            previous = q;
        }
    }
    ItemId hitNode = 0;
    const Pin *hitPin = nullptr;
    for (const auto &c : cards) {
        if (c.rect.contains(io.MousePos))
            hitNode = c.n->id;
        if (c.rect.max.x < f.bounds.min.x || c.rect.min.x > f.bounds.max.x || c.rect.max.y < f.bounds.min.y ||
            c.rect.min.y > f.bounds.max.y)
            continue;
        d->AddRectFilled(c.rect.min, c.rect.max, palette().bg2, dp(4) * z);
        d->AddRectFilled(c.rect.min, {c.rect.max.x, c.rect.min.y + header},
                         mix(palette().bg2, categorical(c.n->colorIndex), .35f), dp(4) * z);
        d->AddRect(c.rect.min, c.rect.max, selected(s.selection, c.n->id) ? palette().accent : palette().line,
                   dp(4) * z, 0, dp(selected(s.selection, c.n->id) ? 2.f : 1.f));
        clippedText(d, {c.rect.min.x + dp(8) * z, c.rect.min.y + dp(5) * z}, w - dp(16) * z, palette().text,
                    c.n->label.c_str());
        for (std::size_t i = 0; i < c.n->inputs.size(); ++i)
            clippedText(d, {c.rect.min.x + dp(10) * z, c.rect.min.y + header + row * float(i)}, w * .44f,
                        palette().textDim, c.n->inputs[i].label.c_str());
        for (std::size_t i = 0; i < c.n->outputs.size(); ++i)
            clippedText(d, {c.rect.min.x + w * .54f, c.rect.min.y + header + row * float(i)}, w * .42f,
                        palette().textDim, c.n->outputs[i].label.c_str());
        if (o.drawBody)
            o.drawBody(*c.n, {{c.rect.min.x + dp(8) * z, c.rect.min.y + header}, c.rect.max});
    }
    for (const auto &p : pins) {
        d->AddCircleFilled(p.p, dp(4) * std::max(.6f, z), palette().info);
        if (std::hypot(p.p.x - io.MousePos.x, p.p.y - io.MousePos.y) < dp(9))
            hitPin = &p;
    }
    if (f.hovered && hitLink && !hitNode && !hitPin && !o.readOnly && ImGui::IsMouseClicked(1))
        result.removeLink = hitLink;
    if (f.hovered && ImGui::IsMouseClicked(0) && !s.canvas.panning) {
        s.focused = true;
        if (hitPin && !o.readOnly) {
            s.wire = hitPin->id;
            s.wireOutput = hitPin->output;
            s.revision = o.revision;
        } else if (hitNode) {
            if (!selected(s.selection, hitNode) || io.KeyCtrl)
                selectItem(s.selection, hitNode, io.KeyCtrl || io.KeyShift);
            if (!o.readOnly && selected(s.selection, hitNode)) {
                s.before.clear();
                for (const auto &n : nodes)
                    if (selected(s.selection, n.id))
                        s.before.push_back({n.id, n.position});
                s.preview = s.before;
                s.grab = f.mouse;
                s.dragging = true;
                s.revision = o.revision;
                result.moved = {EditPhase::begin, s.preview};
            }
        } else {
            if (!io.KeyCtrl && !io.KeyShift)
                s.selection.clear();
            s.boxStart = f.mouse;
            s.boxing = true;
        }
    } else if (ImGui::IsMouseClicked(0) && !f.hovered)
        s.focused = false;
    if (s.dragging) {
        if (ImGui::IsMouseDown(0)) {
            s.preview = s.before;
            for (auto &p : s.preview) {
                p.position.x += f.mouse.x - s.grab.x;
                p.position.y += f.mouse.y - s.grab.y;
                if (!io.KeyAlt) {
                    p.position.x = snapValue(p.position.x, o.snap);
                    p.position.y = snapValue(p.position.y, o.snap);
                }
            }
            if (result.moved.phase != EditPhase::begin && (io.MouseDelta.x || io.MouseDelta.y))
                result.moved = {EditPhase::update, s.preview};
        } else {
            result.moved = {EditPhase::commit, s.preview};
            clear();
        }
    }
    if (s.wire) {
        const auto *p = pinAt(s.wire);
        if (p)
            wire(s.wireOutput ? p->p : io.MousePos, s.wireOutput ? io.MousePos : p->p, palette().accent);
        if (!ImGui::IsMouseDown(0)) {
            if (hitPin && p && hitPin->id != p->id && hitPin->node != p->node &&
                hitPin->output != s.wireOutput) {
                const auto from = s.wireOutput ? s.wire : hitPin->id, to = s.wireOutput ? hitPin->id : s.wire;
                if (!o.canConnect || o.canConnect(from, to)) {
                    result.connectFrom = from;
                    result.connectTo = to;
                }
            }
            s.wire = 0;
        }
    }
    if (s.boxing) {
        const auto a = s.canvas.view.screen(s.boxStart, f.bounds.min);
        Rect box{{std::min(a.x, io.MousePos.x), std::min(a.y, io.MousePos.y)},
                 {std::max(a.x, io.MousePos.x), std::max(a.y, io.MousePos.y)}};
        d->AddRectFilled(box.min, box.max, palette().selection);
        d->AddRect(box.min, box.max, palette().accent, 0, 0, dp(1));
        if (!ImGui::IsMouseDown(0)) {
            for (const auto &c : cards)
                if (box.contains(c.rect.min) && box.contains(c.rect.max) && !selected(s.selection, c.n->id))
                    s.selection.push_back(c.n->id);
            s.boxing = false;
        }
    }
    if (s.focused && f.focused && !io.WantTextInput) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
            s.selection.clear();
            for (const auto &n : nodes)
                s.selection.push_back(n.id);
        }
        if (!o.readOnly && ImGui::IsKeyPressed(ImGuiKey_Delete))
            result.removeNodes = s.selection;
        if (ImGui::IsKeyPressed(ImGuiKey_Home) && !cards.empty()) {
            Point lo = nodes.front().position, hi = lo;
            for (const auto &n : nodes) {
                lo.x = std::min(lo.x, n.position.x);
                lo.y = std::min(lo.y, n.position.y);
                hi.x = std::max(hi.x, n.position.x + dp(o.width));
                hi.y = std::max(hi.y, n.position.y + dp(o.header + o.row * 8));
            }
            s.canvas.view.fit(lo, hi, f.bounds.size(), dp(24));
        }
    }
    return result;
}
} // namespace cyui
