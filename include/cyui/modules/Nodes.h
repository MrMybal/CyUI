#pragma once
#include "cyui/modules/Canvas.h"
#include <functional>
#include <optional>
#include <string>

namespace cyui {
struct NodePin {
    ItemId id = 0;
    std::string label;
    int colorIndex = 0;
};
struct Node {
    ItemId id = 0;
    Point position;
    std::string label;
    std::vector<NodePin> inputs, outputs;
    int colorIndex = 0;
};
struct NodeLink {
    ItemId id = 0, from = 0, to = 0;
    int colorIndex = 0;
};
struct NodeMove {
    ItemId id = 0;
    Point position;
};
struct NodeGroup {
    ItemId id = 0;
    Point position, size{400, 240};
    std::string label;
    int colorIndex = 0;
};
struct NodeReroute {
    ItemId id = 0, link = 0;
    Point position;
};
struct NodeFragment {
    std::vector<Node> nodes;
    std::vector<NodeLink> links;
    std::vector<NodeGroup> groups;
    std::vector<NodeReroute> reroutes;
};
// Source ID namespaces may overlap; keep the selected kinds separate.
struct NodeSelection {
    std::vector<ItemId> nodes, groups, links, reroutes;
};
enum class GraphChangeKind { added, modified, removed };
struct GraphChange {
    ItemId id = 0;
    GraphChangeKind kind = GraphChangeKind::modified;
    bool link = false;
    std::string title, detail;
};
struct NodeNavigationItem {
    ItemId id = 0;
    std::string title, detail;
};
struct NodeNavigationState {
    TextFilter filter;
    std::optional<ItemId> selected;
};
struct NodeResult {
    Edit<std::vector<NodeMove>> moved;
    ItemId connectFrom = 0, connectTo = 0; // valid when connect
    std::vector<ItemId> removeNodes;
    ItemId removeLink = 0; // valid when linkRemoved
    bool connect = false, linkRemoved = false;
    NodeFragment created;
    Edit<std::vector<NodeGroup>> groups;
    Edit<std::vector<NodeReroute>> reroutes;
    std::vector<ItemId> removeGroups, removeReroutes;
    std::vector<ItemId> removeLinks;
    std::optional<ItemId> revealed;
};
struct NodeState {
    CanvasState canvas;
    std::vector<ItemId> selection; // node IDs; groups/links/reroutes have separate selections below
    std::vector<NodeMove> before, preview;
    Point grab, boxStart;
    ItemId wire = 0; // the pin a new link starts from, while wiring
    bool wireOutput = false, dragging = false, boxing = false, focused = false;
    std::uint64_t revision = 0;
    bool wiring = false;
    std::vector<NodeGroup> groupBefore, groupPreview;
    std::vector<NodeReroute> routeBefore, routePreview;
    std::vector<NodeMove> groupNodesBefore, groupNodesPreview;
    std::vector<NodeReroute> groupRoutesBefore, groupRoutesPreview;
    ItemId overlay = 0;
    int overlayMode = 0; // 1 group move, 2 group resize, 3 reroute move
    std::string clipboardError;
    ItemId contextId = 0;
    int contextKind = 0;
    Point contextPosition;
    std::string groupLabel;
    std::vector<ItemId> selectedGroups, selectedLinks, selectedReroutes;
    std::optional<ItemId> reveal; // center and select this node during the next draw
};
struct NodeOptions {
    bool readOnly = false;
    double snap = 0;
    std::uint64_t revision = 0;
    float width = 190, header = 28, row = 24;
    std::function<bool(ItemId, ItemId)> canConnect;
    // Extra drawing inside each card, in screen coordinates (world-to-screen via state.canvas.view).
    std::function<void(const Node &, const Rect &)> drawBody;
    ArrayView<NodeGroup> groups;
    ArrayView<NodeReroute> reroutes;
    std::function<ItemId()> allocateId; // fresh, nonzero IDs across nodes, pins, links and overlays
    bool moveGroupContents = true;
    ArrayView<GraphChange> changes; // host supplies before/after differences, including any visible ghosts
};
CYUI_API NodeFragment copyNodeSelection(ArrayView<ItemId>, ArrayView<Node>, ArrayView<NodeLink>,
                                        ArrayView<NodeGroup> = {}, ArrayView<NodeReroute> = {});
// Groups include contained groups/nodes. Explicit links/reroutes include their endpoint nodes,
// making the fragment self-contained and safe to paste into a different graph.
CYUI_API NodeFragment copyGraphSelection(const NodeSelection &, ArrayView<Node>, ArrayView<NodeLink>,
                                         ArrayView<NodeGroup> = {}, ArrayView<NodeReroute> = {});
CYUI_API std::string nodesToJson(const NodeFragment &);
CYUI_API bool nodesFromJson(const std::string &, NodeFragment &, std::string &error);
CYUI_API bool cloneNodes(const NodeFragment &, const std::function<ItemId()> &allocateId, Point offset,
                         NodeFragment &, std::string &error);
CYUI_API NodeResult nodeEditor(const char *id, NodeState &, const std::vector<Node> &,
                               const std::vector<NodeLink> &, ImVec2 size, const NodeOptions & = {});
// Layout produces positions; the host decides whether and how to commit them.
CYUI_API std::vector<NodeMove> arrangeNodes(const std::vector<Node> &, const std::vector<NodeLink> &,
                                            Point spacing = {240, 130});
CYUI_API std::optional<ItemId> nodeNavigator(const char *id, NodeNavigationState &, NodeState &,
                                             ArrayView<NodeNavigationItem>, ImVec2 size = {});
// Returns the selected change index. Selecting an existing node requests a reveal.
CYUI_API std::optional<std::size_t> graphChanges(const char *id, NodeState &, ArrayView<GraphChange>,
                                                ImVec2 size = {});
} // namespace cyui
