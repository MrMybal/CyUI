#include "cyui/modules/DataView.h"
#include "cyui/Numeric.h"
#include "ModuleInternal.h"
#include <charconv>
#include <iomanip>
#include <locale>
#include <sstream>
#include <type_traits>
namespace cyui {
std::string dataCellText(const DataCell &v) {
    return std::visit(
        [](const auto &n) -> std::string {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, std::string>)
                return n;
            else if constexpr (std::is_same_v<T, bool>)
                return n ? "true" : "false";
            else if constexpr (std::is_same_v<T, std::int64_t>)
                return std::to_string(n);
            else {
                char buffer[64];
                const auto text = std::to_chars(buffer, buffer + sizeof(buffer), n);
                return text.ec == std::errc() ? std::string(buffer, text.ptr) : std::string{};
            }
        },
        v);
}
std::vector<std::size_t> dataViewRows(ArrayView<DataRow> rows, const DataViewState &state) {
    std::vector<std::size_t> result;
    std::vector<std::string> filters;
    for (const auto &f : state.filters)
        filters.push_back(detail::folded(f));
    for (std::size_t i = 0; i < rows.count; ++i) {
        bool match = true;
        for (std::size_t c = 0; c < filters.size(); ++c)
            if (!filters[c].empty() &&
                (c >= rows[i].cells.size() ||
                 !detail::containsFolded(dataCellText(rows[i].cells[c]), filters[c]))) {
                match = false;
                break;
            }
        if (match)
            result.push_back(i);
    }
    if (state.sort.empty())
        return result;
    std::stable_sort(result.begin(), result.end(), [&](auto a, auto b) {
        for (const auto &sort : state.sort) {
            if (sort.column >= rows[a].cells.size() || sort.column >= rows[b].cells.size())
                continue;
            const auto &x = rows[a].cells[sort.column];
            const auto &y = rows[b].cells[sort.column];
            int compare = 0;
            if (x.index() != y.index())
                compare = x.index() < y.index() ? -1 : 1;
            else
                compare = std::visit(
                    [&](const auto &n) {
                        using T = std::decay_t<decltype(n)>;
                        const auto &m = std::get<T>(y);
                        if constexpr (std::is_same_v<T, double>) {
                            if (std::isnan(n) || std::isnan(m))
                                return std::isnan(n) == std::isnan(m) ? 0 : std::isnan(n) ? 1 : -1;
                        }
                        return n < m ? -1 : m < n ? 1 : 0;
                    },
                    x);
            if (compare)
                return sort.descending ? compare > 0 : compare < 0;
        }
        return false;
    });
    return result;
}
namespace {
std::vector<std::size_t> orderedColumns(const DataViewState &state, std::size_t count) {
    std::vector<std::size_t> result;
    if (state.columnOrder.empty())
        for (std::size_t c = 0; c < count; ++c)
            result.push_back(c);
    else
        for (auto c : state.columnOrder)
            if (c < count && std::find(result.begin(), result.end(), c) == result.end())
                result.push_back(c);
    return result;
}
std::pair<std::size_t, std::size_t> columnRange(const std::vector<std::size_t> &order,
                                                const DataViewState &state) {
    auto a = std::size_t(std::find(order.begin(), order.end(), state.anchorColumn) - order.begin());
    auto b = std::size_t(std::find(order.begin(), order.end(), state.activeColumn) - order.begin());
    if (a == order.size() || b == order.size())
        return {order.size(), order.size()};
    return {std::min(a, b), std::max(a, b)};
}
std::pair<std::size_t, std::size_t> range(const std::vector<std::size_t> &order, ArrayView<DataRow> rows,
                                          const DataViewState &s) {
    auto find = [&](ItemId id) {
        auto i = std::find_if(order.begin(), order.end(), [&](auto n) { return rows[n].id == id; });
        return i == order.end() ? order.size() : std::size_t(i - order.begin());
    };
    auto a = find(s.anchor), b = find(s.active);
    return {std::min(a, b), std::max(a, b)};
}
std::vector<std::vector<std::string>> parseTsv(const std::string &text) {
    if (text.size() > 1024 * 1024)
        throw std::invalid_argument("Clipboard exceeds 1 MiB");
    std::vector<std::vector<std::string>> rows(1);
    std::string cell;
    bool quoted = false, closed = false, begin = true;
    for (std::size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (quoted) {
            if (c == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') {
                    cell += '"';
                    ++i;
                } else {
                    quoted = false;
                    closed = true;
                }
            } else
                cell += c;
        } else if (c == '"' && begin) {
            quoted = true;
            begin = false;
        } else if (c == '\t' || c == '\n' || c == '\r') {
            rows.back().push_back(cell);
            cell.clear();
            begin = true;
            closed = false;
            if (c != '\t') {
                if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
                    ++i;
                rows.emplace_back();
            }
        } else {
            if (closed)
                throw std::invalid_argument("Invalid quoted cell");
            cell += c;
            begin = false;
        }
    }
    if (quoted)
        throw std::invalid_argument("Unterminated quoted cell");
    rows.back().push_back(cell);
    if (rows.size() > 1 && rows.back().size() == 1 && rows.back()[0].empty())
        rows.pop_back();
    return rows;
}
DataCell convert(const DataColumn &column, const DataCell &original, const std::string &text) {
    return std::visit(
        [&](const auto &n) -> DataCell {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, std::string>)
                return text;
            else if constexpr (std::is_same_v<T, bool>) {
                if (text == "true" || text == "1")
                    return true;
                if (text == "false" || text == "0")
                    return false;
                throw std::invalid_argument("Expected true or false");
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                std::int64_t value = 0;
                auto r = std::from_chars(text.data(), text.data() + text.size(), value);
                if (r.ec != std::errc() || r.ptr != text.data() + text.size())
                    throw std::invalid_argument("Expected an exact integer");
                if (column.bounded) {
                    if (!std::isfinite(column.minimum) || !std::isfinite(column.maximum) ||
                        std::ceil(column.minimum) > std::floor(column.maximum) ||
                        column.minimum < -9007199254740991. || column.maximum > 9007199254740991.)
                        throw std::invalid_argument("Invalid integer bounds");
                    value = std::clamp(value, static_cast<std::int64_t>(std::ceil(column.minimum)),
                                       static_cast<std::int64_t>(std::floor(column.maximum)));
                }
                return value;
            } else {
                auto e = evaluateNumericExpression(text, n);
                if (!e.valid)
                    throw std::invalid_argument(e.error);
                if (column.bounded) {
                    if (!std::isfinite(column.minimum) || !std::isfinite(column.maximum) ||
                        column.minimum > column.maximum)
                        throw std::invalid_argument("Invalid numeric bounds");
                    e.value = std::clamp(e.value, static_cast<long double>(column.minimum),
                                         static_cast<long double>(column.maximum));
                }
                double value = double(e.value);
                if (!std::isfinite(value))
                    throw std::invalid_argument("Number exceeds double range");
                return value;
            }
        },
        original);
}
} // namespace
std::string copyDataCells(ArrayView<DataRow> rows, const DataViewState &s) {
    const auto order = dataViewRows(rows, s);
    auto [a, b] = range(order, rows, s);
    if (b >= order.size())
        return {};
    std::size_t count = 0;
    for (auto i = a; i <= b; ++i)
        count = std::max(count, rows[order[i]].cells.size());
    const auto columns = orderedColumns(s, count);
    const auto [lo, hi] = columnRange(columns, s);
    if (hi >= columns.size())
        return {};
    std::string out;
    for (auto i = a; i <= b; ++i) {
        if (i > a)
            out += '\n';
        for (auto c = lo; c <= hi; ++c) {
            if (c > lo)
                out += '\t';
            const auto column = columns[c];
            if (column >= rows[order[i]].cells.size())
                continue;
            auto text = dataCellText(rows[order[i]].cells[column]);
            if (text.find_first_of("\t\r\n\"") != std::string::npos) {
                out += '"';
                for (char x : text) {
                    out += x;
                    if (x == '"')
                        out += '"';
                }
                out += '"';
            } else
                out += text;
            if (out.size() > 1024 * 1024)
                throw std::length_error("Clipboard exceeds 1 MiB");
        }
    }
    return out;
}
DataPasteResult pasteDataCells(ArrayView<DataColumn> columns, ArrayView<DataRow> rows, const DataViewState &s,
                               const std::string &text, const DataViewOptions &o) {
    try {
        if (o.readOnly)
            throw std::invalid_argument("Table is read-only");
        auto values = parseTsv(text);
        auto order = dataViewRows(rows, s);
        auto [a, b] = range(order, rows, s);
        (void)b;
        const auto columnOrder = orderedColumns(s, columns.count);
        const auto [first, last] = columnRange(columnOrder, s);
        (void)last;
        if (first >= columnOrder.size())
            throw std::invalid_argument("Paste exceeds editable cells");
        if (a >= order.size() || values.size() > order.size() - a)
            throw std::invalid_argument("Paste exceeds visible rows");
        DataPasteResult result;
        for (std::size_t i = 0; i < values.size(); ++i) {
            const auto &row = rows[order[a + i]];
            if (row.readOnly || values[i].size() > columnOrder.size() - first)
                throw std::invalid_argument("Paste exceeds editable cells");
            for (std::size_t c = 0; c < values[i].size(); ++c) {
                const auto column = columnOrder[first + c];
                if (column >= row.cells.size())
                    throw std::invalid_argument("Paste exceeds editable cells");
                const auto &col = columns[column];
                if (!col.editable)
                    throw std::invalid_argument("Column is read-only");
                auto value = convert(col, row.cells[column], values[i][c]);
                if (o.validate) {
                    auto error = o.validate(row.id, col.id, value);
                    if (!error.empty())
                        throw std::invalid_argument(error);
                }
                result.edits.push_back({row.id, col.id, {EditPhase::commit, value}});
            }
        }
        return result;
    } catch (const std::exception &e) {
        return {{}, e.what()};
    }
}
std::vector<DataCellEdit> dataView(const char *id, DataViewState &s, ArrayView<DataColumn> columns,
                                   ArrayView<DataRow> rows, ImVec2 size, const DataViewOptions &o) {
    ScopedID scope(id);
    std::vector<DataCellEdit> result;
    if (!columns.count)
        return result;
    s.filters.resize(columns.count);
    if (s.revision != o.revision) {
        s.editing = 0;
        s.draft.clear();
        s.error.clear();
        s.revision = o.revision;
    }
    const auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                       ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable |
                       ImGuiTableFlags_SortMulti | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX;
    if (ImGui::BeginTable("##data", int(columns.count), flags, size)) {
        for (std::size_t c = 0; c < columns.count; ++c)
            ImGui::TableSetupColumn(trLabel(columns[c].label.c_str()).c_str(),
                                    ImGuiTableColumnFlags_WidthFixed, dp(columns[c].width), ImGuiID(c));
        ImGui::TableSetupScrollFreeze(0, o.filters ? 2 : 1);
        ImGui::TableHeadersRow();
        s.columnOrder.clear();
        const auto *table = ImGui::GetCurrentTable();
        for (int display = 0; display < int(columns.count); ++display) {
            const auto column = table->DisplayOrderToIndex[display];
            if (table->Columns[column].IsEnabled)
                s.columnOrder.push_back(std::size_t(column));
        }
        const auto [firstColumn, lastColumn] = columnRange(s.columnOrder, s);
        std::vector<std::size_t> columnPositions(columns.count, s.columnOrder.size());
        for (std::size_t display = 0; display < s.columnOrder.size(); ++display)
            columnPositions[s.columnOrder[display]] = display;
        if (auto specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsDirty) {
            s.sort.clear();
            for (int i = 0; i < specs->SpecsCount; ++i)
                s.sort.push_back({std::size_t(specs->Specs[i].ColumnUserID),
                                  specs->Specs[i].SortDirection == ImGuiSortDirection_Descending});
            specs->SpecsDirty = false;
        }
        if (o.filters) {
            ImGui::TableNextRow();
            for (std::size_t c = 0; c < columns.count; ++c) {
                ImGui::TableSetColumnIndex(int(c));
                ScopedID col{int(c)};
                ImGui::SetNextItemWidth(-1);
                detail::inputText("##filter", s.filters[c]);
            }
        }
        const auto order = dataViewRows(rows, s);
        ImGuiListClipper clip;
        // Tables add cell padding to a selectable's height. Let the clipper
        // measure the real table row pitch instead of assuming a widget pitch.
        clip.Begin(int(order.size()));
        auto [a, b] = range(order, rows, s);
        while (clip.Step())
            for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                const auto &row = rows[order[std::size_t(i)]];
                ItemScope key(row.id);
                ImGui::TableNextRow(0, ImGui::GetFrameHeightWithSpacing());
                for (std::size_t c = 0; c < columns.count && c < row.cells.size(); ++c) {
                    if (!ImGui::TableSetColumnIndex(int(c)))
                        continue;
                    ScopedID col{int(c)};
                    const bool picked = std::size_t(i) >= a && std::size_t(i) <= b &&
                                        firstColumn < s.columnOrder.size() &&
                                        columnPositions[c] >= firstColumn && columnPositions[c] <= lastColumn;
                    if (s.editing == row.id && s.editingColumn == c) {
                        if (s.editFocus) {
                            ImGui::SetKeyboardFocusHere();
                            s.editFocus = false;
                        }
                        ImGui::SetNextItemWidth(-1);
                        if (detail::inputText("##edit", s.draft,
                                              ImGuiInputTextFlags_EnterReturnsTrue |
                                                  ImGuiInputTextFlags_AutoSelectAll) &&
                            !o.readOnly && !row.readOnly && columns[c].editable) {
                            try {
                                auto v = convert(columns[c], row.cells[c], s.draft);
                                s.error = o.validate ? o.validate(row.id, columns[c].id, v) : "";
                                if (s.error.empty()) {
                                    result.push_back({row.id, columns[c].id, {EditPhase::commit, v}});
                                    s.editing = 0;
                                }
                            } catch (const std::exception &e) {
                                s.error = e.what();
                            }
                        }
                        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || o.readOnly)
                            s.editing = 0;
                    } else {
                        const auto text = dataCellText(row.cells[c]);
                        if (ImGui::Selectable((text + "###cell").c_str(), picked,
                                              ImGuiSelectableFlags_AllowDoubleClick,
                                              {0, ImGui::GetFrameHeight()})) {
                            if (!ImGui::GetIO().KeyShift) {
                                s.anchor = row.id;
                                s.anchorColumn = c;
                            }
                            s.active = row.id;
                            s.activeColumn = c;
                            s.selection = {row.id};
                            if (ImGui::IsMouseDoubleClicked(0) && columns[c].editable && !o.readOnly &&
                                !row.readOnly) {
                                s.editing = row.id;
                                s.editingColumn = c;
                                s.draft = text;
                                s.editFocus = true;
                            }
                        }
                    }
                }
            }
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && !ImGui::GetIO().WantTextInput &&
            ImGui::GetIO().KeyCtrl) {
            if (ImGui::IsKeyPressed(ImGuiKey_C))
                try {
                    ImGui::SetClipboardText(copyDataCells(rows, s).c_str());
                    s.error.clear();
                } catch (const std::exception &e) {
                    s.error = e.what();
                }
            if (ImGui::IsKeyPressed(ImGuiKey_V)) {
                auto text = ImGui::GetClipboardText();
                auto paste = pasteDataCells(columns, rows, s, text ? text : "", o);
                s.error = paste.error;
                if (paste)
                    result = std::move(paste.edits);
            }
        }
        ImGui::EndTable();
    }
    if (!s.error.empty())
        textColoured(palette().error, "%s", tr(s.error.c_str()).c_str());
    return result;
}
} // namespace cyui
