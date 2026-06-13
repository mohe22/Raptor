#include "base.hpp"
#include "common/header.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>



namespace Raptor::Common::Parsers{
    template <typename T, std::size_t N>
    class TcpParser : public ParserBase<T, N>{
        public:
        TcpParser() = default;
        ~TcpParser() = default;

        void feed(const T* data, std::size_t len) noexcept override {

            // check if we have enough space in the buffer to write the data
            if (!this->buffer.canWrite(len)) {
                 this->buffer.compact();
             };
            this->buffer.write(data, len);

            while (true) {
                if (this->getState() == State::ReadingHeader) {
                    if (this->buffer.size() < Header::SIZE) {
                        return;
                    }
                    header_ = Header::deserialize(
                        this->buffer.data(),
                        Header::SIZE
                    );
                    this->buffer.consume(Header::SIZE);


                    // header_.print();

                    bodyReceived = 0; // reset body counter


                    this->setState(State::ReadingBody);
                }
                if (this->getState() == State::ReadingBody) {
                    size_t available = this->buffer.size();
                    size_t remaining = header_.payloadSize - bodyReceived;

                    if (available == 0)
                        return; // need more

                    size_t toConsume = std::min(available, remaining);


                    uint8_t* ptr= this->buffer.data();

                    switch (header_.type) {
                        case PacketType::Command: {
                            this->onCommand(header_, std::string_view(reinterpret_cast<char*>(ptr), toConsume));
                            break;
                        }
                        case PacketType::FileUpload:{
                            this->onUpload(header_, std::string_view(reinterpret_cast<char*>(ptr), toConsume));
                            break;
                        }
                        case PacketType::FileDownload: {
                            this->onDownload(header_, std::string_view(reinterpret_cast<char*>(ptr), toConsume));
                            break;
                        }
                        case PacketType::Register: {
                            if (bodyReceived + toConsume < header_.payloadSize) break;
                            this->onRegister(header_, std::string_view(reinterpret_cast<char*>(ptr), header_.payloadSize));
                            break;
                        }
                    }


                    size_t consumed = this->buffer.consume(toConsume);
                    bodyReceived += consumed;
                    if (bodyReceived >= header_.payloadSize) {
                        reset();
                        continue;
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
