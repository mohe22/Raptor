#include "request.hpp"
#include "core/config.hpp"
#include <cstdint>
#include <print>

namespace Raptor::Core::Servers::Tcp::Handlers {

    Net::Result<std::size_t> handleRequest(Core::Session::TcpSession* session) {
        std::array<uint8_t,Config::RECV_BUFFER_SIZE> buf;
        std::size_t totalRead = 0;

        while (true) {
            auto r = session->read(buf.data(), buf.size());

            if (!r)  return std::unexpected{r.error()};
            if (r.value() == 0) break;

            session->parser.feed(buf.data(), r.value());

            if (session->parser.isError()) {
                std::println("[PARSER] error");
                return 0;
            }

            totalRead += r.value();
        }

        if (session->parser.isComplete())
            std::println("\n[PARSER] complete");

        return totalRead;
    }

} // namespace Raptor::Core::Servers::Tcp::Handlers
