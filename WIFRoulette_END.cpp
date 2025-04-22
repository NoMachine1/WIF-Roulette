#include <iostream>
#include <iomanip>
#include <mutex>
#include <thread>
#include <immintrin.h>
#include <cstring>
#include <cstdlib> 
#include <random>  
#include <chrono>  
#include "sha256_avx2.h"
#include <atomic>
#include <cstdint>
#include <array>

using namespace std;

#define ROOT_LENGTH 12

alignas(64) const char BASE58[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

const char WIF_START[] = "KwDiBf89QgGbjEhKnhXJuH7LrciVrZi3qd7sDG4F";

class Timer {

private:
    unsigned long long timer;
    unsigned long long time() {
        return chrono::duration_cast<chrono::milliseconds>(
            chrono::system_clock::now().time_since_epoch()).count();
    }
public:
    Timer() { timer = time(); }
    double stop() {
        unsigned long long newTime = time();
        double returnTime = (double)(newTime - timer) / 1000;
        timer = newTime;
        return returnTime;
    }
};

mutex mutex_;
int threads_number;
atomic<uint64_t> total_WIFs_processed_global(0); 

class Xoshiro256plus {
public:
    Xoshiro256plus(uint64_t seed = 0) {

        state[0] = splitmix64(seed);
        for (int i = 1; i < 4; ++i) {
            state[i] = splitmix64(state[i - 1]);
        }
    }

    inline uint64_t next() __attribute__((hot)) {
        const uint64_t s0 = state[0];
        const uint64_t s1 = state[1];
        const uint64_t s2 = state[2];
        const uint64_t s3 = state[3];

        const uint64_t result = s0 + s3;
        const uint64_t t = s1 << 17;

        state[2] = s2 ^ s0;
        state[3] = s3 ^ s1;
        state[1] = s1 ^ state[2];
        state[0] = s0 ^ state[3];
        state[2] ^= t;

        state[3] = rotl(state[3], 45);

        return result;
    }

private:

    inline uint64_t splitmix64(uint64_t x) __attribute__((always_inline)) {
        uint64_t z = (x += 0x9e3779b97f4a7c15);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
        z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
        return z ^ (z >> 31);
    }

    inline uint64_t rotl(const uint64_t x, int k) __attribute__((always_inline)) {
        return (x << k) | (x >> (64 - k));
    }

    alignas(32) std::array<uint64_t, 4> state;
};

uint64_t get_random_index(Xoshiro256plus& rng) {
    uint64_t max = UINT64_MAX - (UINT64_MAX % 58); 
    while (true) {
        uint64_t rand_val = rng.next();
        if (rand_val < max) {
            return rand_val % 58; 
        }
    }
}



void generate_random_root(char* output, int root_length, Xoshiro256plus& rng) {
    for (int i = 0; i < root_length; ++i) {
        uint64_t rand_index = get_random_index(rng); 
        output[i] = BASE58[rand_index];
    }
    output[root_length] = '\0'; 
}

    alignas(64) const unsigned char BASE58_MAP[256] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
        0x0E, 0x0F, 0x10, 0xFF, 0x11, 0x12, 0x13, 0x14, 0x15, 0xFF,
        0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
        0x20, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x21, 0x22, 0x23,
        0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0xFF, 0x2C,
        0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
        0x37, 0x38, 0x39, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };

typedef __uint128_t uint128_t;

void decode(const unsigned char* input, unsigned char* output) {
    alignas(64) uint64_t digits[52] = {0};
    int digitslen = 1;
    const int length = strlen((const char*)input);

    if (length > 0) {
        uint64_t current_char = BASE58_MAP[input[0]];
        digits[0] = current_char;
    }

    for (int i = 1; i < length; ++i) {
        uint64_t current_char = BASE58_MAP[input[i]];
        uint128_t carry = current_char;

        int j = 0;
        for (; j + 3 < digitslen; j += 4) {

            uint128_t product0 = ((uint128_t)digits[j] * 58) + carry;
            digits[j] = (uint64_t)(product0 & 0xFFFFFFFFFFFFFFFF);
            carry = product0 >> 64;

            uint128_t product1 = ((uint128_t)digits[j+1] * 58) + carry;
            digits[j+1] = (uint64_t)(product1 & 0xFFFFFFFFFFFFFFFF);
            carry = product1 >> 64;

            uint128_t product2 = ((uint128_t)digits[j+2] * 58) + carry;
            digits[j+2] = (uint64_t)(product2 & 0xFFFFFFFFFFFFFFFF);
            carry = product2 >> 64;

            uint128_t product3 = ((uint128_t)digits[j+3] * 58) + carry;
            digits[j+3] = (uint64_t)(product3 & 0xFFFFFFFFFFFFFFFF);
            carry = product3 >> 64;
        }

        for (; j < digitslen; j++) {
            uint128_t product = ((uint128_t)digits[j] * 58) + carry;
            digits[j] = (uint64_t)(product & 0xFFFFFFFFFFFFFFFF);
            carry = product >> 64;
        }

        while (carry > 0 && digitslen < 52) {
            digits[digitslen++] = (uint64_t)(carry & 0xFFFFFFFFFFFFFFFF);
            carry >>= 64;
        }
    }

    int out_pos = 0;
    bool leading_zero = true;

    for (int i = digitslen - 1; i >= 0; i--) {
        uint64_t digit = digits[i];

        uint64_t swapped_digit = __builtin_bswap64(digit);

        uint8_t* bytes = (uint8_t*)&swapped_digit;

        if (leading_zero) {
            int k = 0;
            while (k < 8 && bytes[k] == 0) k++;
            if (k < 8) {
                leading_zero = false;
                memcpy(output + out_pos, bytes + k, 8 - k);
                out_pos += 8 - k;
            }
        } else {
            memcpy(output + out_pos, bytes, 8);
            out_pos += 8;
        }
    }

    if (out_pos < 38) {
        memset(output + out_pos, 0, 38 - out_pos);
    }
}

struct alignas(64) WIFBatch {
    unsigned char wifs[8][52 + 1];
    unsigned char extended_WIFs[8][64];
};

void init_batch(WIFBatch& batch, int batch_size) {
    thread_local Xoshiro256plus rng([]() {
        static atomic<uint64_t> global_seed(chrono::steady_clock::now().time_since_epoch().count());
        uint64_t seed_value = global_seed.load();
        global_seed.store(seed_value + 1); 
        return Xoshiro256plus(seed_value);
    }());
    char random_root[ROOT_LENGTH + 1]; 
    for (int b = 0; b < batch_size; ++b) {
        generate_random_root(random_root, ROOT_LENGTH, rng);
        memcpy(batch.wifs[b], WIF_START, strlen(WIF_START));
        memcpy(batch.wifs[b] + strlen(WIF_START), random_root, ROOT_LENGTH + 1);
        memset(batch.extended_WIFs[b], 0, 64);
        batch.extended_WIFs[b][0] = 0x80;
        batch.extended_WIFs[b][33] = 0x01;
    }
}

void process_batch(WIFBatch& batch, int batch_size) {
    alignas(64) unsigned char payloads[8][34];
    alignas(64) unsigned char stored_checksums[8][4];
    alignas(64) unsigned char avx_inputs[8][64] = {0};
    bool valid_candidate[8] = {false};

    for (int i = 0; i < batch_size; ++i) {
        decode(batch.wifs[i], batch.extended_WIFs[i]);
    }

    for (int i = 0; i < batch_size; ++i) {
        if (batch.extended_WIFs[i][0] == 0x80 && batch.extended_WIFs[i][33] == 0x01) {
            memcpy(payloads[i], batch.extended_WIFs[i], 34);
            memcpy(stored_checksums[i], batch.extended_WIFs[i] + 34, 4);
            valid_candidate[i] = true;

            memcpy(avx_inputs[i], payloads[i], 34);
	    avx_inputs[i][0]   = 0x80;
	    avx_inputs[i][33] = 0x01;
	    avx_inputs[i][34] = 0x80;
	    avx_inputs[i][62] = 0x01;
	    avx_inputs[i][63] = 0x10;
        }
    }

    alignas(64) unsigned char digest1[8][32];
    sha256avx2_8B(
        avx_inputs[0], avx_inputs[1], avx_inputs[2], avx_inputs[3],
        avx_inputs[4], avx_inputs[5], avx_inputs[6], avx_inputs[7],
        digest1[0], digest1[1], digest1[2], digest1[3],
        digest1[4], digest1[5], digest1[6], digest1[7]
    );

    alignas(64) unsigned char second_inputs[8][64] = {0};
    for (int i = 0; i < batch_size; ++i) {
        if (valid_candidate[i]) {
            memcpy(second_inputs[i], digest1[i], 32);
            second_inputs[i][32] = 0x80;
            second_inputs[i][63] = 0x00;
            second_inputs[i][62] = 0x01;
        }
    }

    alignas(64) unsigned char digest2[8][32];
    sha256avx2_8B(
        second_inputs[0], second_inputs[1], second_inputs[2], second_inputs[3],
        second_inputs[4], second_inputs[5], second_inputs[6], second_inputs[7],
        digest2[0], digest2[1], digest2[2], digest2[3],
        digest2[4], digest2[5], digest2[6], digest2[7]
    );

    for (int i = 0; i < batch_size; ++i) {
        if (valid_candidate[i] && memcmp(stored_checksums[i], digest2[i], 4) == 0) {
            lock_guard<mutex> lock(mutex_);
            cout << unitbuf;
            cout << "[W] " << batch.wifs[i] << endl << flush;
        }
    }
}

void thread_function(atomic<uint64_t>& global_counter) {
    WIFBatch batch;
    while (true) {
        init_batch(batch, 8); 
        process_batch(batch, 8); 

        global_counter.fetch_add(8, memory_order_relaxed);
    }
}

int main() {
    cout << "[I] WIF Roulette" << endl;
    cout.precision(2);

    // Get the number of supported threads
    unsigned int threads_number = thread::hardware_concurrency();

    // Fallback if hardware_concurrency() returns 0
    if (threads_number == 0) {
        threads_number = 1;  // Minimum of 1 thread
    }
    // Add a reasonable upper limit to prevent excessive allocation
    else if (threads_number > 128) {  // 128 is a safe upper bound for most systems
        threads_number = 128;
        cerr << "Warning: Limiting threads to 128 (hardware reported " 
             << thread::hardware_concurrency() << " threads)\n";
    }

    // Now safely allocate the thread array
    thread* threads = new thread[threads_number];

    atomic<uint64_t> global_counter(0);

    auto start_time = chrono::high_resolution_clock::now();

    // Changed int t to unsigned int t
    for (unsigned int t = 0; t < threads_number; t++) {
        threads[t] = thread(thread_function, ref(global_counter));
    }

    while (true) {
        this_thread::sleep_for(chrono::seconds(1)); 

        uint64_t current_count = global_counter.load(memory_order_relaxed);

        auto end_time = chrono::high_resolution_clock::now();
        chrono::duration<double> elapsed_seconds = end_time - start_time;

        double mWIFs_per_sec = current_count / (elapsed_seconds.count() * 1e6);

        lock_guard<mutex> lock(mutex_);
        cout << "\r[I] Speed = " << fixed << setprecision(2) << mWIFs_per_sec << " MWIFs/sec" << "\r" << flush;
    }

    // Changed int t to unsigned int t
    for (unsigned int t = 0; t < threads_number; t++) {
        threads[t].join();
    }

    delete[] threads;
    cout << "[I] WIF PREFIX CHECKED" << endl;
    return 0;
}
