

#pragma once
#include <cstdint>
#include <functional>
#include <print>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include "error.hpp"

namespace Raptor::Core::Db
{
    /**
     * @brief Represents a single SQLite column value.
     *
     * Maps directly to SQLite's four native storage classes:
     *  - int            → SQLITE_INTEGER
     *  - double         → SQLITE_FLOAT
     *  - std::string    → SQLITE_TEXT
     *  - std::monostate → SQLITE_NULL  (no value)
     */
    using ColumnValue = std::variant<int, double, std::string, std::monostate>;
    /**
     * @brief A single result row represented as a map of column name → value.
     *
     * Keys   : column names as returned by sqlite3_column_name().
     * Values : typed as ColumnValue — use std::get_if or std::visit to read
     *
     * @note Iteration order is not guaranteed (unordered_map).
     */
    using RowMap = std::unordered_map<std::string, ColumnValue>;

    /**
        * @brief A lightweight, non-owning view of a single result row returned
        *        by a SQLite query step. Instances are constructed internally and
        *        passed directly to the RowCallback — callers should never store
        *        a Row beyond the lifetime of the callback invocation.
    */
    class Row
    {
        public:
        /**
            * @brief Constructs a Row wrapping the currently-stepped statement.
            * @param s A valid, stepped sqlite3_stmt. Must remain alive for the
            *          duration of this Row's use.
            */
        explicit Row(sqlite3_stmt &s) noexcept : stmt(s) {}

        /**
            * @brief Reads an INTEGER column as a native int.
            * @param col Zero-based column index.
            * @return The integer value stored in that column.
            */
        int getInt(int col) const noexcept
        {
            return sqlite3_column_int(&stmt, col);
        }

        /**
            * @brief Reads a REAL column as a double.
            * @param col Zero-based column index.
            * @return The double value stored in that column.
            */
        double getDouble(int col) const noexcept
        {
            return sqlite3_column_double(&stmt, col);
        }

        /**
            * @brief Checks whether a column holds a SQL NULL.
            * @param col Zero-based column index.
            * @return true if the column is NULL, false otherwise.
            */
        bool isNull(int col) const noexcept
        {
            return sqlite3_column_type(&stmt, col) == SQLITE_NULL;
        }

        /**
            * @brief Reads a TEXT column as a std::string.
            *        Returns an empty string if the column is NULL.
            * @param col Zero-based column index.
            * @return A copy of the UTF-8 text stored in that column.
            */
        std::string getText(int col) const noexcept
        {
            const auto *raw = sqlite3_column_text(&stmt, col);
            return raw ? reinterpret_cast<const char *>(raw) : "";
        }
        /**
         * @brief Reads all columns of the current row into a Row_map.
         *
         * Iterates every column in the stepped statement, detects its SQLite
         * storage class, and stores the value under the column's name:
         *
         *  SQLITE_INTEGER → int
         *  SQLITE_FLOAT   → double
         *  SQLITE_TEXT    → std::string
         *  SQLITE_NULL    → std::monostate
         *
         * @return A Row_map containing one entry per column.
         *
         * @note The Row must not outlive the callback invocation that owns it —
         *       do not store the returned map and access it after the callback returns.
         *
         * Example:
         * @code
         *   db.query("SELECT * FROM users", [](Row row) {
         *       Row_map data = row.getAll();
         *       if (auto* name = std::get_if<std::string>(&data["username"]))
         *           std::cout << *name << "\n";
         *   });
         * @endcode
         */
        RowMap getAll() const noexcept;
        private:
        sqlite3_stmt &stmt; /// Non-owning reference to the active statement.
    };

    /**
        * @brief Callback invoked once per result row during a query.
        *        The Row passed in is valid only for the duration of the call.
        */
    using RowCallback = std::function<void(Row)>;

    /**
        * @brief RAII wrapper around a compiled sqlite3_stmt.
        *
        * Instances are produced by Database::prepare(). The statement is
        * finalized automatically on destruction. Move-only: copying is disabled
        * to prevent double-finalize.
        */
    class Statement {
        public:
        Statement(const Statement &) = delete;
        Statement &operator=(const Statement &) = delete;
        Statement(Statement &&other) noexcept
        : statement(other.statement), index(other.index), db_(other.db_)
        {
            other.statement = nullptr;
        }

        Statement &operator=(Statement &&other) noexcept
        {
            if (this != &other)
            {
                close(); // finalize any handle we already own
                statement = other.statement;
                index = other.index;
                db_ = other.db_;
                other.statement = nullptr;
            }
            return *this;
        }

        /** @brief Constructs an empty (uncompiled) statement. */
        Statement() noexcept = default;

        /**
            * @brief Finalizes the underlying sqlite3_stmt if it is still open.
            * @return Always returns true (error-free finalization is guaranteed
            *         by the SQLite API when the statement is valid).
        */
        bool close() noexcept
        {
            if (statement)
            {
                sqlite3_finalize(statement);
                statement = nullptr;
                return true;
            }
            return true;
        }

        /** @brief Finalizes the statement on destruction. */
        ~Statement() noexcept { close(); }

        /**
            * @brief Returns a pointer to the internal sqlite3_stmt* so that
            *        sqlite3_prepare_v2 (which writes through a sqlite3_stmt**)
            *        can initialize it.
            * @return Pointer to the internal statement handle storage.
            */
        sqlite3_stmt **getRawStmt() noexcept { return &statement; }

        /**
            * @brief Binds the next positional parameter (?) to @p val.
            *
            * Parameters are bound in left-to-right order; each call advances
            * the internal index by one. Supported types:
            *  - float / double        → REAL
            *  - sqlite3_int64 / long long → INTEGER (64-bit)
            *  - any other integral    → INTEGER (32-bit)
            *  - std::nullptr_t        → NULL
            *
            * @tparam T  The C++ type of the value to bind.
            * @param  val The value to bind.
            * @return A reference to *this, enabling method chaining.
            */
        template <typename T>
        Statement &bind(T val) noexcept
        {
            if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>)
            {
                sqlite3_bind_double(statement, index++, static_cast<double>(val));
            }
            else if constexpr (std::is_same_v<T, sqlite3_int64> || std::is_same_v<T, long long>)
            {
                sqlite3_bind_int64(statement, index++, val);
            }
            else if constexpr (std::is_same_v<T, uint8_t>)
            {
                sqlite3_bind_int(statement, index++, static_cast<uint8_t>(val));
            }
            else if constexpr (std::is_same_v<T, uint16_t>)
            {
                sqlite3_bind_int(statement, index++, static_cast<uint16_t>(val));
            }
            else if constexpr (std::is_same_v<T, uint32_t>)
            {
                sqlite3_bind_int(statement, index++, static_cast<uint32_t>(val));
            }
            else if constexpr (std::is_integral_v<T>)
            {
                sqlite3_bind_int(statement, index++, static_cast<int>(val));
            }
            else if constexpr (std::is_same_v<T, std::nullptr_t>)
            {
                sqlite3_bind_null(statement, index++);
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                sqlite3_bind_text(statement, index++, val.c_str(), -1, SQLITE_TRANSIENT);
            }
            else if constexpr (std::is_same_v<T, const char *> || std::is_same_v<T, char*>)
            {
                sqlite3_bind_text(statement, index++, val, -1, SQLITE_TRANSIENT);
            }
            else if constexpr (std::is_same_v<T, std::string_view>)
            {
                sqlite3_bind_text(statement, index++, val.data(), val.size(), SQLITE_TRANSIENT);
            }
            else
            {
                static_assert(sizeof(T) == 0, "Unsupported type for bind()");
            }
            return *this;
        }

        /**
            * @brief Executes a non-SELECT (INSERT / UPDATE / DELETE) statement.
            *
            * Steps the compiled statement exactly once, then finalizes it.
            * @return The number of rows affected on success, or an Error.
            */
        Result<int> run() noexcept;

        /**
            * @brief Executes a SELECT statement and iterates over every result row.
            *
            * Calls @p cb once per row with a Row view of the current step.
            * The statement is NOT finalized after this call so the caller may
            * reset and re-execute if needed.
            *
            * @param cb  Callback invoked for each row; must not throw.
            * @return The number of rows that were visited, or an Error.
            */
        Result<int> query(RowCallback cb) noexcept;

        /**
            * @brief Stores the database handle needed for error reporting.
            *        Called by Database::prepare() immediately after construction.
            * @param db The owning database connection. Must outlive this Statement.
            */
        void setDb(sqlite3 *db) noexcept { db_ = db; }

        int change() noexcept
        {
            return sqlite3_changes(db_);
        }

        private:
        sqlite3_stmt *statement{nullptr}; ///< Compiled statement handle; null until prepared.
        int index{1};                     ///< 1-based bind index (SQLite convention).
        sqlite3 *db_{nullptr};            ///< Non-owning pointer used only for error messages.
    };

    /**
        * @brief Singleton RAII wrapper around a SQLite database connection.
        *
        * Obtain the single instance via Database::create(). The connection is
        * closed automatically on program exit. Copying and moving are both
        * disabled to enforce the singleton contract.
        */
    class Database {
        public:
        Database(const Database &) = delete;
        Database &operator=(const Database &) = delete;
        Database(Database&& other) noexcept
            : db_(std::exchange(other.db_, nullptr))
        {}
        Database& operator=(Database&& other) noexcept
        {
            if (this != &other)
            {
                close();
                db_ = std::exchange(other.db_, nullptr);
            }
            return *this;
        }
        ~Database() noexcept{
            if (db_) {
                close();
                std::println("[debug] ~database was called");
            }
        }
        Database() noexcept = default;

        /**
            * @brief Opens (or creates) the SQLite database at @p path and
            *        executes the DDL contained in @p schemaPath.
            *
            * The first call initializes the static instance; subsequent calls
            * re-open the same object (useful for reconnecting after close).
            *
            * @param dbPath        File-system path to the .sqlite / .db file.
            * @param schemaPath  Path to a plain-text SQL file with CREATE TABLE
            *                    statements (and similar DDL). Defaults to
            *                    "./schema.sql".
            * @return A reference_wrapper to the singleton on success, or an Error.
            */
        [[nodiscard]] static Result<Database>
        create(const std::string &dbPath,
            const std::string &schemaPath = "./schema.sql") noexcept;

        /**
            * @brief Closes the underlying SQLite connection and nulls the handle.
            * @return Always true; a missing handle is treated as already-closed.
            */
        bool close() noexcept
        {
            if (db_)
            {
                sqlite3_close(db_);
                db_ = nullptr;
                return true;
            }
            return true;
        }

        /**
            * @brief Executes a plain SQL string with no ? placeholders.
            *
            * Internally calls sqlite3_exec, which prepares, steps, and finalizes
            * for you. Suitable for one-shot statements that define or modify schema
            * (e.g. CREATE, DROP) or manipulate data (e.g. INSERT, UPDATE, DELETE),
            * as long as they have no bind parameters and return no rows.
            *
            * @param query  A complete SQL statement (or semicolon-separated batch).
            * @return The number of rows changed on success, or an Error.
            */
        Result<int> exec(const std::string &query) noexcept;

        /**
            * @brief Compiles a SQL string into a reusable prepared Statement.
            *
            * The returned Statement is not yet executed; call bind() as needed,
            * then run() (for DML) or query() (for SELECT).
            *
            * @param query  A single SQL statement, optionally containing ?
            *               placeholders for bind parameters.
            * @return A ready-to-use Statement on success, or an Error.
            */
        Result<Statement> prepare(const std::string &query) const  noexcept;

        Result<int> query(const std::string& query, RowCallback cb) const noexcept {
            auto stmt = prepare(query);
            if (!stmt)
                return std::unexpected(stmt.error());
            return stmt.value().query(cb);
        }

        private:
        /**
            * @brief Returns the last SQLite error message for this connection,
            *        or a generic fallback if the handle is null.
            */
        std::string getError() const
        {
            return db_ ? sqlite3_errmsg(db_) : "failed to allocate database handle";
        }



        sqlite3 *db_{nullptr}; ///< The live database connection handle.
    };

} // namespace C2::Server::DB
