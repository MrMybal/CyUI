#pragma once
#include "cyui/Core.h"
#include "cyui/Interaction.h"
#include <deque>
#include <mutex>
#include <string>

namespace cyui {
enum class LogLevel { debug, info, warning, error };
struct LogEntry {
    ItemId id = 0;
    LogLevel level = LogLevel::info;
    std::string text, source;
};
class CYUI_API LogBuffer {
  public:
    explicit LogBuffer(std::size_t capacity = 10000, std::size_t maxBytes = 8 * 1024 * 1024)
        : capacity_(std::max<std::size_t>(1, capacity)), maxBytes_(std::max<std::size_t>(1, maxBytes)) {}
    void append(LogLevel, const std::string &, const std::string &source = {});
    void clear();
    std::uint64_t snapshot(std::vector<LogEntry> &, std::uint64_t knownRevision) const;

  private:
    mutable std::mutex mutex_;
    std::deque<LogEntry> rows_;
    std::size_t capacity_, maxBytes_, bytes_ = 0;
    ItemId next_ = 1;
    std::uint64_t revision_ = 1;
};
struct LogState {
    std::string query;
    std::vector<LogEntry> rows;
    std::uint64_t revision = 0;
    bool follow = true;
    unsigned levels = 15;
    ItemId selected = 0;
};
CYUI_API ItemId logView(const char *id, LogState &, const LogBuffer &, ImVec2 size = {});
struct Diagnostic {
    ItemId id = 0;
    LogLevel level = LogLevel::error;
    std::string source, message;
    int line = 0, column = 0;
};
CYUI_API ItemId diagnosticList(const char *id, ArrayView<Diagnostic>, ImVec2 size = {});
} // namespace cyui
