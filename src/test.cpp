#include <iostream>
#include <chrono>
#include <random>
#include <vector>

#include "tieredCuckooHash.h"
#include "test.h"

int runTest() {
    // Parameters
    const size_t numElements = 20'000'000; // 10 million
    TieredCuckooHash<int, int> hashmap;

    // Benchmark insertion
    auto startInsert = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < numElements; ++i) {
        hashmap[static_cast<int>(i)] = static_cast<int>(i * 2);
    }
    auto endInsert = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> insertTime = endInsert - startInsert;

    // Memory usage
    size_t originalSize = hashmap.getUncompressedSize();
    size_t compressedSize = hashmap.getCompressedSize();

    // Prepare lookup keys
    std::vector<int> lookupKeys(numElements);
    std::mt19937 rng(42); // fixed seed for reproducibility
    std::uniform_int_distribution<int> dist(0, static_cast<int>(numElements) - 1);
    for (size_t i = 0; i < numElements; ++i) {
        lookupKeys[i] = dist(rng);
    }

    // Benchmark lookup
    auto startLookup = std::chrono::high_resolution_clock::now();
    volatile int sum = 0;
    for (size_t i = 0; i < numElements; ++i) {
        sum += hashmap[lookupKeys[i]];
    }
    auto endLookup = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> lookupTime = endLookup - startLookup;

    // Report results
    std::cout << "Insert throughput: " << static_cast<double>(numElements) / insertTime.count() << " ops/s\n";
    std::cout << "Lookup throughput: " << static_cast<double>(numElements) / lookupTime.count() << " ops/s\n";
    std::cout << "Memory usage before compression: " << static_cast<double>(originalSize) / (1024*1024) << " MB\n";
    std::cout << "Memory usage after compression: " << static_cast<double>(compressedSize) / (1024*1024) << " MB\n";
    std::cout << "Compression ratio: " << static_cast<double>(originalSize) / static_cast<double>(compressedSize) << "\n";
    std::cout << "Memory per element: " << static_cast<double>(originalSize) / numElements << " bytes\n";
    std::cout << "Compressed memory per element: " << static_cast<double>(compressedSize) / numElements << " bytes\n";

    return 0;
}
