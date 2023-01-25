#include "../src/lib.cpp"

int main() {
	RingBuffer<int64_t, 8> buffer;
	WriteGuard<int64_t, 8> writer = buffer.try_lock().value();
	writer.write(127);
	SharedReader<int64_t, 8> reader = buffer.reader();
	printf("value: %lu\n", buffer.data[0].message);
	writer.write(30);
	printf("value: %lu\n", buffer.data[0].message);
	printf("value: %lu\n", buffer.data[1].message);
}
