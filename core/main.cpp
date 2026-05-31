
#include "core/context.hpp"
#include <print>

using namespace Raptor::Core;
using namespace Raptor::Common;


int main() {
    try {

        Context::init(
            "./db",
            "/home/mohe/Documents/C++/C2-Raptor/core/db/schema.sql"
        );


        auto& mgr = Context::get().servers();

        mgr.createServer(Raptor::Common::Types::ServerType::UDP, {
            .ip = "0.0.0.0",
            .port = 9090,
            .instanceName = "metrics"
        });

        mgr.createServer(Raptor::Common::Types::ServerType::TCP, {
            .ip = "0.0.0.0",
            .port = 8080,
            .instanceName = "api"
        });


        Api::run(); // start the API
        mgr.stopAll(); // stop all servers
    }
    catch (const std::exception& e) {
        std::println("[FATAL] Exception: {}", e.what());
        return 1;
    }

    std::println("[INFO] Shutdown complete");
    return 0;
}
