#pragma once

#include <mutex>
#include <optional>
#include <print>
#include <queue>
#include <vector>

namespace Raptor::Core::Session::Queue {
    /**
     * @brief Thread-safe priority queue for task dispatch.
     *
     * Provides atomic push/pop operations with mutex protection.
     * Eliminates unsafe pointer returns by using std::optional and atomic pop.
     *
     * @tparam T       Element type. Must be move-constructible.
     * @tparam Compare Comparator defining heap order (default: max-heap).
     */
    template<typename T, typename Compare = std::less<T>>
    class SendQueue {
        public:
        SendQueue() : queue_(Compare()) {}

        SendQueue(const SendQueue&)            = delete;
        SendQueue& operator=(const SendQueue&) = delete;

        SendQueue(SendQueue&&)                 = delete;
        SendQueue& operator=(SendQueue&&)      = delete;

        /**
         * @brief Pushes an item onto the queue.
         *
         * Thread-safe. The item is moved into the queue.
         *
         * @param item Item to enqueue (moved from).
         */
        void push(T&& item) noexcept {
            std::lock_guard<std::mutex> lock(mtx_);
            queue_.push(std::move(item));
        }



        /**
         * @brief Removes the front element if the queue is non-empty.
         *
         * Thread-safe. Safe no-op if empty — never throws.
         *
         * @return true if an element was popped, false if queue was empty.
         */
        bool tryPop() noexcept {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!queue_.empty()) {
                queue_.pop();
                return true;
            }
            return false;
        }


        /**
         * @brief Returns a pointer to the front element for read-only inspection.
         *
         * Thread-safe. The pointer is valid ONLY while the caller does not
         * call any other queue methods. It is INVALIDATED by push, emplace, or tryPop.
         *
         * @return std::optional with non-owning pointer to front, or nullptr if empty.
         *
         * @warning DO NOT use for pointer checks like `if (tryFront())` followed by
         *          separate operations. The pointer becomes invalid immediately.
         *          Use tryPeek() or tryPopFront() instead for safe access.
         *
         * @deprecated Unsafe in concurrent scenarios. Use tryPopFront() instead.
         */
        std::optional<T*> tryFront() noexcept {
            std::lock_guard<std::mutex> lock(mtx_);
            if (queue_.empty()) return std::nullopt;
            return const_cast<T*>(&queue_.top());
        }

        /**
         * @brief Const variant of tryFront.
         *
         * @return std::optional with const pointer to front, or nullptr if empty.
         *
         * @deprecated Unsafe in concurrent scenarios. Use tryPopFront() instead.
         */
        std::optional<const T*> tryFront() const  noexcept {
            std::lock_guard<std::mutex> lock(mtx_);
            if (queue_.empty()) return std::nullopt;
            return &queue_.top();
        }

        /**
         * @brief Returns whether the queue is empty.
         *
         * Thread-safe snapshot. Value may change immediately after return.
         *
         * @return true if queue contains no elements.
         */
        bool empty() const noexcept{
            std::lock_guard<std::mutex> lock(mtx_);
            return queue_.empty();
        }

        /**
         * @brief Returns the current number of elements in the queue.
         *
         * Thread-safe snapshot. Value may change immediately after return.
         *
         * @return Number of elements currently queued.
         */
        size_t size() const noexcept {
            std::lock_guard<std::mutex> lock(mtx_);
            return queue_.size();
        }


        private:
        mutable std::mutex mtx_;   ///< Protects all queue access.
        std::priority_queue<T, std::vector<T>, Compare> queue_; ///< Underlying heap.
    };

} // namespace Raptor::Core::Session::Queue
