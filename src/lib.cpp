#include <atomic>
#include <experimental/optional>


template <typename T>
class Block {
public:
	T message;
	std::atomic<int64_t> version;
};

template <typename T, int N> class RingBuffer;
template <typename T, int N> class SharedReader;
template <typename T, int N> class WriteGuard;

template <typename T, int N>
class RingBuffer {
private:
	std::atomic<bool> locked;
	std::atomic<int64_t> version;
	std::atomic<int64_t> index;
	Block<T> data[N];

	int64_t start_write() {
		int64_t index = this->index.load(std::memory_order::memory_order_relaxed);
		int64_t seq = this->data[index].seq.load(std::memory_order::memory_order_relaxed);

		printf("\nindex: %d, sequence %d\n", index, seq);

		return index;
	}

public:
	explicit RingBuffer();

	SharedReader<T, N> reader() {
		return SharedReader<T, N> { 
			buffer: &this, 
			index: std::atomic<int64_t>(this->index.load(std::memory_order::memory_order_relaxed)),
			version: std::atomic<int64_t>(this->version.load(std::memory_order::memory_order_relaxed))
		};
	}

	std::experimental::optional<WriteGuard<T, N>> try_lock() {
		if (!this->locked.exchange(true, std::memory_order::memory_order_acquire)) {
			return std::experimental::optional<WriteGuard<T, N>>( WriteGuard<T, N> { buffer: &this } );
		} else {
			return std::experimental::nullopt;
		}
	}
};

template <typename T, int N>
class SharedReader {
private:
	RingBuffer<T, N>* buffer;
	std::atomic<int64_t> index;
	std::atomic<int64_t> version;
};

template <typename T, int N>
class WriteGuard {
private:
	RingBuffer<T, N>* buffer;
};
