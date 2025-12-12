#include <iostream>
#include <chrono>
#include <random>
#include <vector>

#include "tieredCuckooHash.h"
#include "test.h"

template <unsigned int Bytes>
struct BytesStruct {
	BytesStruct(bool doRandom = false) {
		if (doRandom) {
			for (unsigned int i = 0; i < Bytes; i++){
				data[i] = (std::uint8_t)rand();
			}
		}
	}
	std::array<std::uint8_t, Bytes> data;
};

void runTest() {
	const unsigned long long startSize = 300000;
	const unsigned long long incSize = 100000;



	std::mt19937 rng(42);
	for (unsigned long long tableSize = startSize; tableSize <= startSize + incSize*25; tableSize += incSize) {
		float avgInsertThroughput = 0;
		float avgLookupThroughput = 0;
		float avgCompressionRatio = 0;
		float avgLoadFactor = 0;
		unsigned int count = 0;
		for (unsigned int i = 0; i < 6; i++) {
			TieredCuckooHash<unsigned long long, BytesStruct<56>> hashmap;
			std::vector<int> lookupKeys(tableSize);
			std::uniform_int_distribution<int> dist(0, static_cast<int>(tableSize) - 1);
			for (size_t i = 0; i < tableSize; ++i) {
				lookupKeys[i] = dist(rng);
				hashmap[lookupKeys[i]] = BytesStruct<56>(true);
			}
			std::ranges::shuffle(lookupKeys, rng);

			// get basics
			size_t originalSize = hashmap.getUncompressedSize();
			size_t compressedSize = hashmap.getCompressedSize();
			float loadFactor = hashmap.load_factor();
			size_t teirCount = hashmap.getTierCount();

			const size_t numElements = 10000;

			// look up
			auto startLookup = std::chrono::high_resolution_clock::now();
			volatile unsigned long long sum = 0;
			for (size_t i = 0; i < numElements; ++i) {
				sum += hashmap[lookupKeys[i]].data[i%56];
			}
			auto endLookup = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> lookupTime = endLookup - startLookup;

			// insert
			auto startInsert = std::chrono::high_resolution_clock::now();
			for (size_t i = 0; i < numElements; ++i) {
				hashmap[static_cast<int>(i)] = static_cast<int>(i * 2);
			}
			auto endInsert = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> insertTime = endInsert - startInsert;

			if (teirCount == 4) {
				avgInsertThroughput += static_cast<double>(numElements) / insertTime.count();
				avgLookupThroughput += static_cast<double>(numElements) / lookupTime.count();
				avgCompressionRatio += static_cast<double>(compressedSize) / static_cast<double>(originalSize);
				avgLoadFactor += loadFactor;
				count += 1;
			}
		}
		if (count == 0) continue;
		std::cout << "Insert throughput: " << avgInsertThroughput / (float)(count) << " ops/s\n";
		std::cout << "Lookup throughput: " << avgLookupThroughput / (float)(count) << " ops/s\n";
		std::cout << "Compression ratio: " << avgCompressionRatio / (float)(count) << "\n";
		std::cout << "Load factor: " << avgLoadFactor / (float)(count) << "\n";
	}
}
