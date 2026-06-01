#include "base.hpp"
#include "header.hpp"
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
            size_t written = this->buffer.write(data, len);

            while (true) {
                if (this->getState() == State::ReadingHeader) {
                    if (this->buffer.size() < Header::SerializedSize) {
                        return;
                    }
                    header_ = Header::deserialize(
                        this->buffer.data(),
                        Header::SerializedSize
                    );

                    this->buffer.consume(Header::SerializedSize);

                    bodyReceived = 0; // reset body counter

                    header_.print();

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
                    if (bodyReceived >= header_.payloadSize)
                        reset();

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
            this->buffer.clear();

        }
    };
}
