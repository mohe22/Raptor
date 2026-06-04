#pragma once
#include <string>
#include <vector>
#include <optional>
#include "db.hpp"

namespace Raptor::Core::Db {
enum class LogLevel : int {
    Debug = 0,
    Info  = 1,
    Warn  = 2,
    Error = 3,
    Fatal = 4
};

enum class LogCategory : int {
    System  = 0,
    Server  = 1,
    Session = 3
};
inline const char* toString(LogCategory c) {
    switch (c) {
        case LogCategory::System:  return "System";
        case LogCategory::Server:  return "Server";
        case LogCategory::Session: return "Session";
        default:                   return "Unknown";
    }
}
inline const char* toString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "Debug";
        case LogLevel::Info:  return "Info";
        case LogLevel::Warn:  return "Warn";
        case LogLevel::Error: return "Error";
        case LogLevel::Fatal: return "Fatal";
        default:             return "unknown";
    }
}

struct LogEntry {
    int64_t     id;
    int64_t     ts;
    LogLevel    level;
    LogCategory category;
    std::string event;
    std::string message;
    std::string meta;

    LogEntry()                           = default;
    LogEntry(LogEntry&&)                 = default;
    LogEntry& operator=(LogEntry&&)      = default;
    LogEntry(const LogEntry&)            = delete;
    LogEntry& operator=(const LogEntry&) = delete;
};

struct LogFilter {
    std::optional<LogLevel>    level;
    std::optional<LogCategory> category;
    std::optional<std::string> event;
    std::optional<int64_t>     from;   // unix ts
    std::optional<int64_t>     to;     // unix ts
    int                        limit  = 100;
    int                        offset = 0;
};

class LogRepository {
public:
    explicit LogRepository(Database& db) noexcept : db_(db) {}


    Result<int> insert(LogLevel    level,
                       LogCategory category,
                       std::string event,
                       std::string message,
                       std::string meta = "") noexcept
    {
        auto stmt = db_.prepare(
            "INSERT INTO logs(levelId, categoryId, event, message, meta)"
            " VALUES(?, ?, ?, ?, ?)");
        if (!stmt) return std::unexpected(stmt.error());

        stmt->bind(static_cast<int>(level))
             .bind(static_cast<int>(category))
             .bind(event)
             .bind(message)
             .bind(meta.empty() ? nullptr : meta.c_str());

        return stmt->run();
    }

    // Convenience wrappers
    void debug(LogCategory cat, std::string event,
               std::string msg, std::string meta = "") noexcept {
        if (auto r = insert(LogLevel::Debug, cat, event, msg, meta); !r)
            std::println("[log error] {}", r.error().message);
    }
    void info(LogCategory cat, std::string event,
              std::string msg, std::string meta = "") noexcept {
        if (auto r = insert(LogLevel::Info, cat, event, msg, meta); !r)
            std::println("[log error] {}", r.error().message);
    }
    void warn(LogCategory cat, std::string event,
              std::string msg, std::string meta = "") noexcept {
        if (auto r = insert(LogLevel::Warn, cat, event, msg, meta); !r)
            std::println("[log error] {}", r.error().message);
    }
    void error(LogCategory cat, std::string event,
               std::string msg, std::string meta = "") noexcept {
        if (auto r = insert(LogLevel::Error, cat, event, msg, meta); !r)
            std::println("[log error] {}", r.error().message);
    }
    void fatal(LogCategory cat, std::string event,
               std::string msg, std::string meta = "") noexcept {
        if (auto r = insert(LogLevel::Fatal, cat, event, msg, meta); !r)
            std::println("[log error] {}", r.error().message);
    }


    Result<std::vector<LogEntry>> get(const LogFilter& f = {}) const noexcept{
        std::string sql =
            "SELECT id, ts, levelId, categoryId, event, message, IFNULL(meta,'') "
            "FROM logs WHERE 1=1";

        if (f.level)    sql += " AND levelId = ?";
        if (f.category) sql += " AND categoryId = ?";
        if (f.event)    sql += " AND event = ?";
        if (f.from)     sql += " AND ts >= ?";
        if (f.to)       sql += " AND ts <= ?";

        sql += " ORDER BY ts DESC";
        sql += " LIMIT " + std::to_string(f.limit);
        sql += " OFFSET " + std::to_string(f.offset);

        auto stmt = db_.prepare(sql);
        if (!stmt) return std::unexpected(stmt.error());

        if (f.level)    stmt->bind(static_cast<int>(*f.level));
        if (f.category) stmt->bind(static_cast<int>(*f.category));
        if (f.event)    stmt->bind(*f.event);
        if (f.from)     stmt->bind(*f.from);
        if (f.to)       stmt->bind(*f.to);

        stmt->bind(f.limit);
        stmt->bind(f.offset);

        std::vector<LogEntry> rows;
        auto result = stmt->query([&](Row row) {
            LogEntry entry;
            entry.id       = row.getInt(0);
            entry.ts       = row.getInt(1);
            entry.level    = static_cast<LogLevel>(row.getInt(2));
            entry.category = static_cast<LogCategory>(row.getInt(3));
            entry.event    = row.getText(4);
            entry.message  = row.getText(5);
            entry.meta     = row.getText(6);
            rows.push_back(std::move(entry));
        });

        if (!result) return std::unexpected(result.error());
        return rows;
    }

    Result<int> count(const LogFilter& f = {}) noexcept
    {
        std::string sql = "SELECT COUNT(*) FROM logs WHERE 1=1";
        if (f.level)    sql += " AND levelId = "    + std::to_string(static_cast<int>(*f.level));
        if (f.category) sql += " AND categoryId = " + std::to_string(static_cast<int>(*f.category));
        if (f.event)    sql += " AND event = '"     + *f.event + "'";
        if (f.from)     sql += " AND ts >= "        + std::to_string(*f.from);
        if (f.to)       sql += " AND ts <= "        + std::to_string(*f.to);

        int total = 0;
        auto result = db_.query(sql, [&](Row row) {
            total = row.getInt(0);
        });

        if (!result) return std::unexpected(result.error());
        return total;
    }

    Result<int> clear() noexcept {
        return db_.exec("DELETE FROM logs");
    }

private:
    Database& db_;
};

} // namespace Raptor::Core::Db
