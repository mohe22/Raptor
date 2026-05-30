

#pragma once
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <vector>

namespace Raptor::Core::Session::Queue {
    /**
        * @brief A thread-safe priority queue for outbound task dispatch.
        *
        * Wraps `std::priority_queue` with a mutex so that multiple threads can
        * push and pop concurrently without external synchronisation.
        *
        * The ordering is controlled by the `Compare` policy (default: `std::less<T>`,
        * which yields a max-heap — the largest item is served first). Supply a custom
        * comparator to invert the order or rank by a specific field:
        *
        * @code
        * // Min-heap — lowest priority value served first
        * SendQueue<Task, std::greater<Task>> q;
        * @endcode
        *
        * @tparam T       Type of items stored. Must be movable.
        * @tparam Compare Binary comparator that defines heap order.
        *                 Defaults to `std::less<T>` (max-heap).
        */
    template<typename T, typename Compare = std::less<T>>
    class SendQueue {
        public:
        /**
            * @brief Constructs an empty queue with a default-constructed comparator.
            */
        SendQueue() : queue_(Compare()) {}

        /// Non-copyable — ownership is exclusive.
        SendQueue(const SendQueue&)            = delete;
        SendQueue& operator=(const SendQueue&) = delete;

        /// Movable — transfers internal state and mutex ownership.
        SendQueue(SendQueue&&)                 = default;
        SendQueue& operator=(SendQueue&&)      = default;

        /**
            * @brief Moves @p item into the queue and re-heapifies.
            *
            * @param item  Item to enqueue. Moved from — do not use after this call.
            * @threadsafe   Acquires the internal mutex for the duration of the push.
            */
        void push(T&& item) {
            std::lock_guard<std::mutex> lock(mtx_);
            queue_.push(std::move(item));
        }

        /**
            * @brief Constructs an item in-place at its correct heap position.
            *
            * Identical to `push` for move-only types; may save a move for types
            * with non-trivial construction.
            *
            * @param item  Item to emplace. Moved from — do not use after this call.
            * @threadsafe  Acquires the internal mutex for the duration of the emplace.
            */
        void emplace(T&& item) {
            std::lock_guard<std::mutex> lock(mtx_);
            queue_.emplace(std::move(item));
        }

        /**
            * @brief Removes the highest-priority item if the queue is non-empty.
            *
            * Silent no-op if the queue is empty — never throws.
            *
            * @threadsafe  Acquires the internal mutex.
            */
        void tryPop() {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!queue_.empty()) queue_.pop();
        }

        /**
            * @brief Returns a pointer to the highest-priority item, or nullptr.
            *
            * The pointer is valid only while the caller holds no other lock and does
            * not call any mutating method. It is invalidated by the next `push`,
            * `emplace`, or `tryPop`.
            *
            * @return  Non-owning pointer to the top item, or `nullptr` if empty.
            * @threadsafe  Acquires the internal mutex.
            */
        T* tryFront() {
            std::lock_guard<std::mutex> lock(mtx_);
            if (queue_.empty()) return nullptr;
            return const_cast<T*>(&queue_.top());
        }
        template<typename Fn>
        void forEach(Fn&& fn) {
            std::lock_guard<std::mutex> lock(mtx_);

            // priority_queue stores data in a std::vector internally
            // reinterpret_cast reaches in and grabs it directly
            auto& container = reinterpret_cast<std::vector<T>&>(queue_);

            // now we can loop normally
            for (auto& item : container) fn(item);
        }
        /**
            * @brief Const overload of `tryFront`.
            *
            * @return  `const` non-owning pointer to the top item, or `nullptr`.
            * @threadsafe  Acquires the internal mutex.
            */
        const T* tryFront() const {
            std::lock_guard<std::mutex> lock(mtx_);
            if (queue_.empty()) return nullptr;
            return &queue_.top();
        }

        /**
            * @brief Returns `true` if the queue contains no items.
            * @threadsafe  Acquires the internal mutex.
            */
        bool empty() const {
            std::lock_guard<std::mutex> lock(mtx_);
            return queue_.empty();
        }

        /**
            * @brief Returns the number of items currently in the queue.
            * @threadsafe  Acquires the internal mutex.
            */
        size_t size() const {
            std::lock_guard<std::mutex> lock(mtx_);
            return queue_.size();
        }

        private:
        mutable std::mutex mtx_;   ///< Guards all queue access.
        std::priority_queue<T, std::vector<T>, Compare> queue_; ///< Underlying heap storage.
    };


    template<typename T>
    class ReceiveQueue {
        public:
        ReceiveQueue()                              = default;
        ReceiveQueue(const ReceiveQueue&)            = delete;
        ReceiveQueue& operator=(const ReceiveQueue&) = delete;
        ReceiveQueue(ReceiveQueue&&)                 = default;
        ReceiveQueue& operator=(ReceiveQueue&&)      = default;

        void push(T&& item) {
            {
                std::lock_guard<std::mutex> lock(mtx_);
                queue_.push(std::move(item));
            }
            cv_.notify_one(); // will trigger the wait_for
        }

        // i will block unitil i get something
        std::optional<T> wait(std::chrono::milliseconds timeout = std::chrono::seconds(30)) {
            std::unique_lock<std::mutex> lock(mtx_);
            if (!cv_.wait_for(lock, timeout, [this]{ return !queue_.empty(); })) {
                return std::nullopt; //  return empty optional on timeout
            }
            T item = std::move(queue_.front());
            queue_.pop();
            return item;
        }

        // non blocking  returns immediately
        std::optional<T> tryPop() {
            std::lock_guard<std::mutex> lock(mtx_);
            if (queue_.empty()) return std::nullopt;
            T item = std::move(queue_.front());
            queue_.pop();
            return item;
        }

        bool empty() const {
            std::lock_guard<std::mutex> lock(mtx_);
            return queue_.empty();
        }

        size_t size() const {
            std::lock_guard<std::mutex> lock(mtx_);
            return queue_.size();
        }

        private:
        mutable std::mutex      mtx_;
        std::condition_variable cv_;
        std::queue<T>           queue_;
    };

} // namespace C2::Tasks
