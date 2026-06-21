#include "base.hpp"
#include "common/header.hpp"
#include <cstddef>
#include <cstdio>



namespace Raptor::Common::Parsers{
    template <typename T, std::size_t N>
    class TcpParser : public ParserBase<T, N>{
        public:
        TcpParser() = default;
        ~TcpParser() = default;

        void feed(const T* data, std::size_t len) noexcept override {
            if (!this->buffer.canWrite(len)) {
                this->buffer.compact();
            }
            std::size_t written = this->buffer.write(data, len);

            while (true) {
                if (this->getState() == State::ReadingHeader) {
                    if (this->buffer.size() < Header::SIZE)
                        return;

                    header_ = Header::deserialize(this->buffer.data(), Header::SIZE);
                    this->buffer.consume(Header::SIZE);
                    bodyReceived = 0;
                    this->setState(State::ReadingBody);
                    continue;
                }

                if (this->getState() == State::ReadingBody) {
                    size_t available = this->buffer.size();
                    if (available == 0) return;

                    size_t remaining = header_.payloadSize - bodyReceived;
                    size_t toConsume = std::min(available, remaining);
                    const char* ptr = reinterpret_cast<const char*>(this->buffer.data());

                    switch (header_.type) {
                        case PacketType::Command: {
                            this->onCommand(header_, {ptr, toConsume});
                            size_t consumed = this->buffer.consume(toConsume);
                            bodyReceived += consumed;
                            break;
                        }
                        case PacketType::FileUpload: {
                            this->onUpload(header_, {ptr, toConsume});
                            size_t consumed = this->buffer.consume(toConsume);
                            bodyReceived += consumed;
                            break;
                        }
                        case PacketType::FileDownload: {
                            this->onDownload(header_, {ptr, toConsume});
                            size_t consumed = this->buffer.consume(toConsume);
                            bodyReceived += consumed;
                            break;
                        }
                        case PacketType::Register: {
                            // Only call when complete, but accumulate without consuming until then
                            if (bodyReceived + toConsume >= header_.payloadSize) {
                                this->onRegister(header_, {ptr, header_.payloadSize});
                                //  NOW consume the accumulated Register data
                                size_t consumed = this->buffer.consume(toConsume);
                                bodyReceived += consumed;
                            } else {
                                // Incomplete accumulate without consuming
                                bodyReceived += toConsume;
                                return;
                            }
                            break;
                        }
                    }

                    if (bodyReceived >= header_.payloadSize) {
                        reset();
                        continue;  // next packet
                    }
                    return;
                }
            }
        }
        private:
        Header header_;
        size_t bodyReceived{0};
        void reset() noexcept override {
            bodyReceived = 0;
            this->setState(State::ReadingHeader);
        }
    };
}
