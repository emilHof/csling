//! This crates provides a sequentially locking Ring Buffer. It allows for
//! a fast and non-writer-blocking SPMC-queue, where all consumers read all
//! messages.
//!
//! # Usage
//!
//! There are two ways of consuming from the queue. If threads share a
//! [`SharedReader`] through a shared reference, they will steal
//! queue items from one anothers such that no two threads will read the
//! same message. When a [`SharedReader`] is cloned, the new
//! [`SharedReader`]'s reading progress will no longer affect the other
//! one. If two threads each use a separate [`SharedReader`], they
//! will be able to read the same messages.
//!
//! # Important!
//!
//! It is also important to keep in mind, that slow readers will be overrun by
//! the writer if they do not consume messages quickly enough. This can happen
//! quite frequently if the buffer size is not large enough. It is advisable to
//! test applications on a case-by-case basis and find a buffer size that is
//! optimal to your use-case.

#include <atomic>
#include <cstring>
#include <experimental/optional>

template <class T, int N> class Block;
template <class T, int N> class RingBuffer;
template <class T, int N> class SharedReader;
template <class T, int N> class WriteGuard;

template <class T, int N> class RingBuffer {
    friend class SharedReader<T, N>;
    friend class WriteGuard<T, N>;

  private:
    std::atomic_bool locked;
    std::atomic_int64_t version;
    std::atomic_int64_t index;
    Block<T, N> data[N];

    int64_t start_write(void);

    void end_write(int64_t index);

  public:
    RingBuffer();

    SharedReader<T, N> reader(void);

    std::experimental::optional<WriteGuard<T, N>> try_lock(void);
};

template <class T, int N> RingBuffer<T, N>::RingBuffer() {
    locked.store(false);
    index.store(0);
    version.store(0);
}

template <class T, int N> SharedReader<T, N> RingBuffer<T, N>::reader(void) {
    return SharedReader<T, N>(this);
}

template <class T, int N>
std::experimental::optional<WriteGuard<T, N>> RingBuffer<T, N>::try_lock(void) {
    if (!this->locked.exchange(true, std::memory_order_acquire)) {
        return std::experimental::optional<WriteGuard<T, N>>(
            WriteGuard<T, N>(this));
    } else {
        return std::experimental::nullopt;
    }
}

template <class T, int N> int64_t RingBuffer<T, N>::start_write(void) {
    int64_t index = this->index.load(std::memory_order_relaxed);
    int64_t seq = this->data[index].seq.fetch_add(1, std::memory_order_relaxed);

    int64_t ver = this->version.load(std::memory_order_relaxed);

    this->version.store(std::max(ver, seq + 2), std::memory_order_relaxed);

    return index;
}

template <class T, int N> void RingBuffer<T, N>::end_write(int64_t index) {
    this->index.store((index + 1) % N, std::memory_order_relaxed);

    int64_t seq = this->data[index].seq.fetch_add(1, std::memory_order_release);
}

template <class T, int N> class SharedReader {
  private:
    RingBuffer<T, N> *buffer;
    std::atomic_int64_t index;
    std::atomic_int64_t version;
    bool chech_version(int64_t seq, int64_t ver, int64_t i);

  public:
    SharedReader(RingBuffer<T, N> *buffer);
    SharedReader(SharedReader<T, N> &&);
    std::experimental::optional<T> pop_front();
};

template <class T, int N>
SharedReader<T, N>::SharedReader(RingBuffer<T, N> *buffer) {
    this->buffer = buffer;
    this->version.store(0);
    this->index.store(0);
}

template <class T, int N>
std::experimental::optional<T> SharedReader<T, N>::pop_front() {

    while (true) {
        int64_t i = this->index.load(std::memory_order_acquire);
        int64_t ver = this->version.load(std::memory_order_relaxed);
        int64_t seq1 =
            this->buffer->data[i].seq.load(std::memory_order_acquire);
        if (!this->chech_version(seq1, ver, i)) {
            return std::experimental::nullopt;
        }

        T data;

        memcpy(&data, &this->buffer->data[i].message, sizeof(T));

        int64_t seq2 =
            this->buffer->data[i].seq.load(std::memory_order_acquire);

        if (seq1 != seq2) {
            continue;
        }

        if (!this->version.compare_exchange_strong(ver, seq2,
                                                   std::memory_order_relaxed,
                                                   std::memory_order_relaxed)) {
            return std::experimental::nullopt;
        }

        if (!this->index.compare_exchange_strong(i, (i + 1) % N,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed)) {
            continue;
        }

        return std::experimental::optional<T>(data);
    };
}

template <class T, int N>
bool SharedReader<T, N>::chech_version(int64_t seq, int64_t ver, int64_t i) {
    if (seq % 1 != 0) {
        return false;
    };

    if ((i == 0 && seq == ver) || seq < ver) {
        return false;
    }

    return true;
}

template <class T, int N> class WriteGuard {
  private:
    RingBuffer<T, N> *buffer;

  public:
    WriteGuard(RingBuffer<T, N> *buffer);
    void push_back(T val);
};

template <class T, int N>
WriteGuard<T, N>::WriteGuard(RingBuffer<T, N> *buffer) {
    this->buffer = buffer;
}

template <class T, int N> void WriteGuard<T, N>::push_back(T val) {
    int64_t i = this->buffer->start_write();
    std::memcpy(&this->buffer->data[i].message, &val, sizeof(T));
    this->buffer->end_write(i);
}

template <class T, int N> class Block {
    friend class SharedReader<T, N>;
    friend class RingBuffer<T, N>;
    friend class WriteGuard<T, N>;

  public:
    T message;
    std::atomic<int64_t> seq;
    Block();
};

template <class T, int N> Block<T, N>::Block() { seq.store(0); };
