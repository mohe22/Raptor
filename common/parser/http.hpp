#include "base.hpp"
#include <cstddef>
#include <print>
#include <string_view>

namespace Raptor::Common::Parsers {
    template <typename T, std::size_t N>
    class HttpParser : public Base<T, N> {
    public:
        HttpParser()  = default;
        ~HttpParser() = default;

        void feed(const T* data, std::size_t len) noexcept override {
            this->buffer.write(data, len);
            parse();
        }

    private:
        std::string_view requestLine_;
        std::size_t      bodyLen_{0};
        std::size_t      bodyReceived_{0};

        void reset() noexcept override {
            Base<T,N>::reset();
            bodyLen_      = 0;
            bodyReceived_ = 0;
            requestLine_  = {};
        }

        std::size_t findCRLF(std::string_view view, std::size_t from = 0) const noexcept {
            auto pos = view.find("\r\n", from);
            return pos == std::string_view::npos ? std::string_view::npos : pos;
        }

        void parse() noexcept {
            while (true) {
                if (this->getState() == State::ReadingHeader) {
                    if (!parseHeaders()) return;
                }
                if (this->getState() == State::ReadingBody) {
                    if (!streamBody()) return;
                }
            }
        }

        bool parseHeaders() noexcept {
            std::string_view view{
                reinterpret_cast<const char*>(this->buffer.data()),
                this->buffer.size()
            };

            auto headerEnd = view.find("\r\n\r\n");
            if (headerEnd == std::string_view::npos)
                return false;

            std::string_view headerBlock = view.substr(0, headerEnd);

            auto lineEnd = findCRLF(headerBlock);
            if (lineEnd == std::string_view::npos) return false;

            requestLine_ = headerBlock.substr(0, lineEnd);
            std::println("── Request Line ──────────────────");
            std::println("  {}", requestLine_);
            std::println("── Headers ───────────────────────");

            std::size_t pos = lineEnd + 2;
            while (pos < headerBlock.size()) {
                auto end = findCRLF(headerBlock, pos);
                if (end == std::string_view::npos) break;

                std::string_view line = headerBlock.substr(pos, end - pos);
                if (line.empty()) break;

                auto colon = line.find(':');
                if (colon != std::string_view::npos) {
                    auto key = line.substr(0, colon);
                    auto val = line.substr(colon + 2);
                    std::println("  {}: {}", key, val);

                    if (key == "Content-Length")
                        bodyLen_ = std::stoul(std::string(val));
                }
                pos = end + 2;
            }

            this->buffer.consume(headerEnd + 4);
            this->setState(State::ReadingBody);
            return true;
        }

        bool streamBody() noexcept {
            if (bodyLen_ == 0) {
                std::println("── Body (empty) ──────────────────");
                this->setStatus(Status::Complete);
                this->reset();
                return true;
            }

            const std::size_t inBuffer = this->buffer.size();
            if (inBuffer == 0) return false; // nothing to process yet

            const std::size_t remaining = bodyLen_ - bodyReceived_;
            const std::size_t toProcess = std::min(remaining, inBuffer);

            bodyReceived_ += toProcess;
            this->buffer.consume(toProcess);
            this->buffer.compact();

            const double pct = (static_cast<double>(bodyReceived_) / bodyLen_) * 100.0;
            std::print("\r── Body [{:>12} / {:>12} bytes] {:>6.2f}%",
                bodyReceived_, bodyLen_, pct);
            std::fflush(stdout);

            if (bodyReceived_ >= bodyLen_) {
                std::println("\n── Body complete ({} bytes) ──────", bodyLen_);
                this->setStatus(Status::Complete);
                this->reset();
                return true;
            }

            return false; // need more data
        }

    };
} // namespace Raptor::Common::Parsers
