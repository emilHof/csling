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
