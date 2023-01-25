This crates provides a sequentially locking Ring Buffer. It allows for
a fast and non-writer-blocking SPMC-queue, where all consumers read all
messages.

## Usage

There are two ways of consuming from the queue. If threads share a
`ReadGuard` through a shared reference, they will steal
queue items from one anothers such that no two threads will read the
same message. When a `ReadGuard` is cloned, the new
`ReadGuard`'s reading progress will no longer affect the other
one. If two threads each use a separate `ReadGuard`, they
will be able to read the same messages.

```cpp
#include "../src/lib.cpp"
#include <stdio.h>
#include <thread>

const int MAX_SPIN = 256;
const int BUF_SIZE = 1024;

void reads(SharedReader<uint8_t, BUF_SIZE> *reader, int t) {
    while (true) {
        while (reader->pop_front()) {
            printf("received by: %d!\n", t);
        }

        int counter = 0;

        while (!reader->pop_front() && counter < MAX_SPIN) {
            counter += 1;
            std::this_thread::yield();
        }

        if (counter < MAX_SPIN) {
            continue;
        }

        break;
    }
}

int main() {
    RingBuffer<uint8_t, BUF_SIZE> buffer;
    WriteGuard<uint8_t, BUF_SIZE> writer = buffer.try_lock().value();
    SharedReader<uint8_t, BUF_SIZE> reader = buffer.reader();

    std::thread threads[8];

    for (int t = 0; t < 8; t++) {
        threads[t] = std::thread(reads, &reader, t);
    };

    for (int i = 0; i < 1000; i++) {
        writer.push_back(0);
    }

    for (int t = 0; t < 8; t++) {
        threads[t].join();
    };
}
```

## Important!

It is also important to keep in mind, that slow readers will be overrun by the writer if they
do not consume messages quickly enough. This can happen quite frequently if the buffer size is
not large enough. It is advisable to test applications on a case-by-case basis and find a
buffer size that is optimal to your use-case.
