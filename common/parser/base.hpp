#pragma once
#include <cstddef>
#include "buffer.hpp"

namespace Raptor::Common::Parsers {

    enum class Status { NeedMore, Complete, Error };
    enum class State  { ReadingHeader, ReadingBody };

    template <typename T, std::size_t N>
    class Base {
    public:
        Base(const Base&)            = delete;
        Base(Base&&)                 = delete;
        Base& operator=(const Base&) = delete;
        Base& operator=(Base&&)      = delete;
        virtual ~Base()              = default;
        Base()                       = default;

        virtual void feed(const T* data, std::size_t len) noexcept = 0;

        // public so HttpParser can call isError() / isComplete() from handler
        bool isComplete() const noexcept { return status_ == Status::Complete; }
        bool isError()    const noexcept { return status_ == Status::Error;    }
        bool needsMore()  const noexcept { return status_ == Status::NeedMore; }

        // reset is virtual so child can reset its own state too
        virtual void reset() noexcept {
            status_ = Status::NeedMore;
            state_  = State::ReadingHeader;
            buffer.clear();
        }

    public:
        protected:
        void   setState (State  s) noexcept { state_  = s; }
        void   setStatus(Status s) noexcept { status_ = s; }
        State  getState ()  const noexcept  { return state_;  }
        Status getStatus()  const noexcept  { return status_; }

        Buffer<T, N> buffer;

    private:
        Status status_{Status::NeedMore};
        State  state_ {State::ReadingHeader};
    };

} // namespace Raptor::Common::Parsers
