#include <atomic>
#include <experimental/optional>
#include <cstring>

template <class T> class Block;
template <class T, int N> class RingBuffer;
template <class T, int N> class SharedReader;
template <class T, int N> class WriteGuard;

template <class T, int N> class RingBuffer {
public:
	std::atomic_bool locked;
	std::atomic_int64_t version;
	std::atomic_int64_t index;
	Block<T> data[N];

	RingBuffer();

	SharedReader<T, N> reader(void);

	std::experimental::optional<WriteGuard<T, N>> try_lock(void);

	int64_t start_write(void); 

	void end_write(int64_t index);
};

template<class T, int N> RingBuffer<T, N>::RingBuffer() {
	locked.store(false);
	index.store(0);
	version.store(0);
}

template<class T, int N> SharedReader<T, N> RingBuffer<T, N>::reader(void) {
	return SharedReader<T, N>(this);
}

template<class T, int N> std::experimental::optional<WriteGuard<T, N>> RingBuffer<T, N>::try_lock(void) {
	if (!this->locked.exchange(true, std::memory_order::memory_order_acquire)) {
		return std::experimental::optional<WriteGuard<T, N>>(WriteGuard<T, N>(this));
	} else {
		return std::experimental::nullopt;
	}
}

template<class T, int N> int64_t RingBuffer<T, N>::start_write(void) {
	int64_t index = this->index.load(std::memory_order::memory_order_relaxed);
	int64_t seq = this->data[index].seq.load(std::memory_order::memory_order_relaxed);

	int64_t ver = this->version.load(std::memory_order::memory_order_relaxed);

	this->version.store(ver + 1, std::memory_order::memory_order_relaxed);

	return index;
}

template<class T, int N> void RingBuffer<T, N>::end_write(int64_t index) {
	this->index.store((index + 1) % N, std::memory_order::memory_order_relaxed);

	int64_t seq = this->data[index].seq.fetch_add(1, std::memory_order::memory_order_release);
}

template <class T, int N> class SharedReader {
private:
	RingBuffer<T, N>* buffer;
	std::atomic_int64_t index;
	std::atomic_int64_t version;
public:
	SharedReader(RingBuffer<T, N>* buffer);
};

template <class T, int N> SharedReader<T, N>::SharedReader(RingBuffer<T, N>* buffer) {
	this->buffer = buffer;
	this->version.store(buffer->version.load(std::memory_order::memory_order_relaxed));
	this->index.store(0);
}

template <class T, int N> class WriteGuard {
private:
	RingBuffer<T, N>* buffer;
public:
	WriteGuard(RingBuffer<T, N>* buffer);
	void write(T val);
};

template <class T, int N> WriteGuard<T, N>::WriteGuard(RingBuffer<T, N>* buffer) {
	this->buffer = buffer;
}

template <class T, int N> void WriteGuard<T, N>::write(T val) {
	int64_t i = this->buffer->start_write();
	std::memcpy(&this->buffer->data[i].message, &val, sizeof(T));
	this->buffer->end_write(i);
}

template <class T> class Block {
public:
	T message;
	std::atomic<int64_t> seq;
	Block();
};

template <class T> Block<T>::Block() {
	seq.store(0);
};
