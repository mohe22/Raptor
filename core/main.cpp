#include "core/context.hpp"
#include <print>
#include <csignal>

using namespace Raptor::Core;
using namespace Raptor::Common;

static void onSignal(int) {
    std::println("[INFO] Shutting down servers...");
    Context::get().servers().stopAll();
    std::println("[INFO] Servers stopped");
    drogon::app().quit();
}

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

        std::signal(SIGINT,  onSignal);
        std::signal(SIGTERM, onSignal);

        Api::run();
    }
    catch (const std::exception& e) {
        std::println("[FATAL] Exception: {}", e.what());
        return 1;
    }

    std::println("[INFO] Shutdown complete");
    return 0;
}
