#ifndef LOCK_FREE_QUEUE_H
#define LOCK_FREE_QUEUE_H

#include <atomic>
#include <vector>
#include <stdexcept>
#include <unistd.h>

// 无锁队列，用 CAS 替代锁，减少高并发情景下的等待瓶颈
// 用于线程池
template <typename T>
class mpmc_queue {
public:
    explicit mpmc_queue(size_t capacity) 
        : capacity_(capacity), mask_(capacity - 1), buffer_(capacity) {
        if ((capacity & mask_) != 0) {
            throw std::invalid_argument("Capacity must be a power of 2");
        }
        
        for (size_t i = 0; i < capacity_; i++) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }

    }

    bool push(T const& data) {
        Cell* cell;
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        for (;;) {
            cell = &buffer_[pos & mask_];
            size_t seq = cell->sequence.load(std::memory_order_relaxed);
            intptr_t diff = (intptr_t)seq - (intptr_t)pos;

            if (diff == 0) {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            } else if (diff < 0) {
                return false;
            } else {
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }
        cell->data = data;
        cell->sequence.store(pos + 1, std::memory_order_relaxed);
        return true;
    }

    bool pop(T& data) {
        Cell* cell;
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        for (;;) {
            cell = &buffer_[pos & mask_];
            size_t seq = cell->sequence.load(std::memory_order_relaxed);
            intptr_t diff = (intptr_t)seq - (intptr_t)pos;
            if (diff == 0) {
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            } else if (diff < 0) {
                return false;
            } else {
                pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
        }
        data = cell->data;
        cell->sequence.store(pos + mask_ + 1, std::memory_order_relaxed);
        return true;
    }

private:
    struct Cell {
        std::atomic<size_t> sequence;
        T data;
    };

    const size_t capacity_;
    const size_t mask_;
    std::vector<Cell> buffer_;
    alignas(64) std::atomic<size_t> enqueue_pos_;
    alignas(64) std::atomic<size_t> dequeue_pos_;
};

#endif