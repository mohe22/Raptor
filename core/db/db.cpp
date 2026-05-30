

#include "db.hpp"
#include <sstream>
#include <fstream>
namespace Raptor::Core::Db {
    RowMap Row::getAll() const noexcept {
        std::unordered_map<std::string,ColumnValue> result;
        int columnCount = sqlite3_column_count(&stmt);

        for (int i = 0; i < columnCount; i++) {
            const char* name = sqlite3_column_name(&stmt, i);
            const int  type = sqlite3_column_type(&stmt, i);

            switch (type) {
                case SQLITE_INTEGER:
                result[name] = getInt(i);
                break;
                case SQLITE_FLOAT:
                result[name] = getDouble(i);
                break;
                case SQLITE_TEXT:
                result[name] = getText(i);
                break;
                case SQLITE_NULL:
                result[name] = std::monostate{};
                break;
                default:
                result[name] = std::monostate{};
                break;
            }
        }
        return result;
    }
    Result<int> Statement::run() noexcept {
        if (!statement) {
            return std::unexpected(Error{SQLITE_ERROR, "invalid statement"});
        }

        // Advance the statement machine one step.
        const int res = sqlite3_step(statement);

        // Capture row-change count before finalize invalidates the handle.
        const int changed = sqlite3_changes(db_);

        // Always finalize: frees SQLite resources and resets internal state.
        close();

        if (res != SQLITE_DONE) {
            return std::unexpected(Error{res, sqlite3_errmsg(db_)});
        }
        return changed;
    }


    Result<int> Statement::query(RowCallback cb) noexcept {
        int res{SQLITE_ERROR};

        // sqlite3_step returns SQLITE_ROW while rows remain, SQLITE_DONE when done.
        while ((res = sqlite3_step(statement)) == SQLITE_ROW) {
            // Dereference the pointer so Row holds a reference, not a pointer.
            cb(Row{*statement});
        }

        if (res != SQLITE_DONE) {
            return std::unexpected(Error{res, sqlite3_errmsg(db_)});
        }
        return sqlite3_changes(db_);
    }


    Result<Database>
    Database::create(const std::string& path,
        const std::string& schemaPath) noexcept {
             Database db;

             // 1. Open the schema file that contains the DDL.
             std::ifstream file(schemaPath);
             if (!file.is_open()) {
                 return std::unexpected(
                     Error{SQLITE_ERROR, "failed to open schema file: " + schemaPath});
             }


            // 2. Open (or create) the database file.
            const int rc = sqlite3_open(path.c_str(), &db.db_);
            if (rc != SQLITE_OK) {
                std::string msg = db.getError();
                if (db.db_) {
                    sqlite3_close(db.db_);
                    db.db_ = nullptr;
                }
                return std::unexpected(Error{rc, std::move(msg)});
            }

            // 4. Read the entire schema file into a string.
            std::ostringstream ss;
            ss << file.rdbuf();
            std::string sql = ss.str();

            // 5. Execute all DDL statements in one shot via sqlite3_exec.
            char* errMsg = nullptr;
            const int schemaRc =
                sqlite3_exec(db.db_, sql.c_str(), nullptr, nullptr, &errMsg);
            if (schemaRc != SQLITE_OK) {
                std::string msg = errMsg ? errMsg : "unknown schema error";
                sqlite3_free(errMsg);
                return std::unexpected(
                    Error{schemaRc, "failed to execute schema SQL: " + msg});
            }

            return db;
        }

        Result<int> Database::exec(const std::string& query) noexcept {
            char* errMsg{nullptr};
            const int res = sqlite3_exec(
                db_,
                query.c_str(),
                nullptr,   // no per-row callback needed
                nullptr,   // no user-data pointer
                &errMsg
            );
            if (res != SQLITE_OK) {
                std::string msg = errMsg ? errMsg : "unknown error";
                sqlite3_free(errMsg);
                return std::unexpected(Error{res, std::move(msg)});
            }
            return sqlite3_changes(db_);
        }


        Result<Statement> Database::prepare(const std::string& q) const noexcept {
            Statement stat;

            const int res = sqlite3_prepare_v2(
                db_,              // open database connection
                q.c_str(),        // SQL text to compile
                -1,               // -1 → read until the null terminator
                stat.getRawStmt(), // output: writes the compiled stmt pointer here
                nullptr           // tail pointer not needed for single statements
            );

            if (res != SQLITE_OK) {
                return std::unexpected(Error{res, getError()});
            }

            // Give the statement a non-owning pointer to the db handle so it
            // can report errors from run() / query() after steps fail.
            stat.setDb(db_);
            return stat;
        }



} // namespace C2::Server::DB
