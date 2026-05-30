#pragma once
#include <stdexcept>
#include <string>
#include "core/db/error.hpp"
#include "core/db/logRepository.hpp"
#include "core/servers/manager.hpp"
#include "db/db.hpp"
#include "core/api/init.hpp"

namespace Raptor::Core {

    class Context {
    public:
        // No copy, no move
        Context(const Context&)            = delete;
        Context& operator=(const Context&) = delete;
        Context(Context&&)                 = delete;
        Context& operator=(Context&&)      = delete;

        static void init(const std::string& dbPath,
                         const std::string& schemaPath) {
            auto& ctx = instance();
            if (ctx.initialized) {
                throw std::runtime_error("Context already initialized");
            }

            Db::Result<Db::Database> result = Db::Database::create(dbPath, schemaPath);
            if (result.has_value()) {
                ctx.db_ = std::move(result.value());
                ctx.initialized = true;
            } else {
                throw std::runtime_error(result.error().message);
            }
             Api::init();
        }

        static Context& get() {
            auto& ctx = instance();
            if (!ctx.initialized) {
                throw std::runtime_error("Context not initialized — call Context::init() first");
            }
            return ctx;
        }

        Db::Database& db() noexcept { return db_; }
        Db::LogRepository& logs() noexcept { return logs_; }
        Servers::Manager& servers() noexcept { return servers_; }

        const Db::Database& db() const noexcept { return db_; }
        const Db::LogRepository& logs() const noexcept { return logs_; }
        const Servers::Manager& servers() const noexcept { return servers_; }
    private:
        Context()  = default;
        ~Context() = default;

        static Context& instance() {
            static Context ctx;
            return ctx;
        }

        Db::Database db_;
        bool initialized = false;
        Db::LogRepository logs_{ db_ };
        Servers::Manager servers_;
    };

} // namespace Raptor::Core
