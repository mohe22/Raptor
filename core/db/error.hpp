

#pragma once
#include <expected>
#include <string>
#include <sqlite3.h>

namespace Raptor::Core::Db {

    struct Error {
        int         code;
        std::string message;

        Error(int rc, std::string msg )
            : code(rc), message(std::move(msg)) {}

        bool is(int sqlite_code) const noexcept { return code == sqlite_code; }
    };

    template<typename T>
    using Result = std::expected<T, Error>;
}
