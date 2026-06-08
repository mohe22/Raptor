#pragma once
#include <cstddef>
#include "common/buffer.hpp"
#include "common/header.hpp"

namespace Raptor::Common::Parsers {

    enum class State  { ReadingHeader, ReadingBody };

    template <typename T, std::size_t N>
    class ParserBase {
    public:
        ParserBase(const ParserBase&)            = delete;
        ParserBase(ParserBase&&)                 = delete;
        ParserBase& operator=(const ParserBase&) = delete;
        ParserBase& operator=(ParserBase&&)      = delete;
        virtual ~ParserBase()              = default;
        ParserBase()                       = default;

        virtual void feed(const T* data, std::size_t len) noexcept = 0;
        // reset is virtual so child can reset its own state too
        virtual void reset() noexcept {
            state_  = State::ReadingHeader;
            buffer.clear();
        }


        protected:
        void   setState (State  s) noexcept { state_  = s; }
        State  getState ()  const noexcept  { return state_;  }

        Buffer<T, N> buffer;

    private:
        State  state_ {State::ReadingHeader};
        protected:

        virtual void onRegister(const Common::Header& , std::string_view ) noexcept {};
        virtual void onCommand(const Common::Header& , std::string_view) noexcept = 0;
        virtual void onUpload(const Common::Header& , std::string_view ) noexcept = 0;
        virtual void onDownload(const Common::Header& , std::string_view ) noexcept = 0;

    };

} // namespace Raptor::Common::Parsers
