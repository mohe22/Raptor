#pragma once
#include <array>
#include <cstddef>
#include <cstring>
#include <span>

namespace Raptor::Common {

    /**
     * @brief Fixed-size linear buffer with separate read/write offsets.
     *
     * the read cursor simply advances forward.
     * Call compact() when the buffer is fully drained to reset both cursors
     * to zero without copying anything.
     *
     * Layout:
     *   [consumed | readable | writable]
     *    0        ^          ^         N
     *             rOff_      wOff_
     *
     * @tparam T  Element type (typically uint8_t).
     * @tparam N  Maximum capacity in elements.
     */
    template<typename T, std::size_t N>
    class Buffer {
    public:
        Buffer()  = default;

        Buffer(const Buffer&)            = delete;
        Buffer& operator=(const Buffer&) = delete;
        Buffer(Buffer&&)                 = default;
        Buffer& operator=(Buffer&&)      = default;

        /** @return true if no unread bytes remain. */
        bool isEmpty() const noexcept { return rOff_ == wOff_; }

        /** @return true if no more bytes can be written. */
        bool isFull() const noexcept { return wOff_ == N; }

        /** @return Number of bytes available to read. */
        std::size_t size() const noexcept { return wOff_ - rOff_; }

        /** @return Maximum number of bytes this buffer can hold. */
        std::size_t capacity() const noexcept { return N; }

        /** @return Number of bytes that can still be written. */
        std::size_t available() const noexcept { return N - wOff_; }

        /** @return true if len bytes can be written without truncation. */
        bool canWrite(std::size_t len) const noexcept { return len <= available(); }

        /**
         * @brief Resets both cursors — buffer fully reusable from scratch.
         *
         * Only safe to call when you no longer need any buffered data.
         */
        void clear() noexcept {
            rOff_ = 0;
            wOff_ = 0;
        }

        /**
         * @brief Copies up to `len` bytes into the write region.

         * @return Number of bytes actually written.
         */
        std::size_t write(const T* data, std::size_t len) noexcept {
            const std::size_t toWrite = std::min(len, available());
            if (toWrite == 0) return 0;
            std::memcpy(buff_.data() + wOff_, data, toWrite);
            wOff_ += toWrite;
            return toWrite;
        }

        /**
         * @brief Advances the read cursor by `len` bytes — no copy, no memmove.
         *
         * Call after the parser has finished processing a complete message.
         * Call compact() after consume() if you want to reclaim write space.
         *
         * @return Number of bytes actually consumed.
         */
        std::size_t consume(std::size_t len) noexcept {
            const std::size_t toConsume = std::min(len, size());
            rOff_ += toConsume;
            return toConsume;
        }

        /**
         * @brief Reclaims space by resetting cursors when the buffer is empty.
         *
         * No data is copied. If unread bytes remain, does nothing —
         * call consume() first to drain them.
         *
         * Typical pattern:
         * @code
         *   buf.consume(msg.size());
         *   buf.compact();           // free space for next write if fully drained
         * @endcode
         */
        void compact() noexcept {
            if (isEmpty()) {
                rOff_ = 0;
                wOff_ = 0;
            }
        }

        /** @return Raw pointer to the next readable byte. */
        const T* data() const noexcept { return buff_.data() + rOff_; }
        T* data()       noexcept { return buff_.data() + rOff_; }

        /** @return Span over the readable region only [rOff_, wOff_). */
        std::span<const T> view() const noexcept { return {buff_.data() + rOff_, size()}; }

        /** @return Raw pointer to the next writable position. */
        const T* writePtr() const noexcept { return buff_.data() + wOff_; }
        T* writePtr()       noexcept { return buff_.data() + wOff_; }

        size_t getReadOffset() const noexcept { return rOff_; }
        size_t getWriteOffset() const noexcept { return wOff_; }

    private:
        std::array<T, N> buff_;
        std::size_t      rOff_{0}; // read cursor   next byte to consume
        std::size_t      wOff_{0}; // write cursor next byte to write
    };

} // namespace Raptor::Common
