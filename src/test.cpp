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
				// data[i] = (std::uint8_t)0x10101010;
			}
		}
	}
	std::array<std::uint8_t, Bytes> data;
};

void runTest() {
	const unsigned long long startSize = 120000;
	const unsigned long long incSize = 20000;

	std::mt19937 rng(42);
	for (unsigned long long tableSize = startSize; tableSize <= startSize + incSize*27; tableSize += incSize) {
		float avgInsertThroughput = 0;
		float avgLookupThroughput = 0;
		float avgCompressionRatio = 0;
		float avgMemorySpaceSaving = 0;
		float avgLoadFactor = 0;
		unsigned int count = 0;
		for (unsigned int i = 0; i < 4; i++) {
			TieredCuckooHash<unsigned long long, BytesStruct<56>> hashmap;
			std::vector<int> lookupKeys(tableSize);
			std::uniform_int_distribution<int> dist(0, static_cast<int>(tableSize) - 1);
			for (size_t i = 0; i < tableSize; ++i) {
				lookupKeys[i] = dist(rng);
				hashmap[lookupKeys[i]] = BytesStruct<56>(true);
			}
			std::ranges::shuffle(lookupKeys, rng);

			// get basics
			size_t kvSize = hashmap.getDataSize();
			size_t compressedSize = hashmap.getCompressedSize();
			size_t uncompressedSize = hashmap.getUncompressedSize();
			float loadFactor = hashmap.load_factor();
			size_t teirCount = hashmap.getTierCount();
			// std::cout << compressedSize << "/" << uncompressedSize <<"\n";

			const size_t numElements = 10000;

			// look up
			hashmap.resetCompresstionDelay();
			auto startLookup = std::chrono::high_resolution_clock::now();
			volatile unsigned long long sum = 0;
			for (size_t i = 0; i < numElements; ++i) {
				sum += hashmap[lookupKeys[i]].data[i%56];
			}
			auto endLookup = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> lookupTime = endLookup - startLookup;
			unsigned long long lookupTimeCompresstionDelay = hashmap.getTotalCompresstionDelayNS();

			// insert
			hashmap.resetCompresstionDelay();
			auto startInsert = std::chrono::high_resolution_clock::now();
			for (size_t i = 0; i < numElements; ++i) {
				hashmap[static_cast<int>(i)] = static_cast<int>(i * 2);
			}
			auto endInsert = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> insertTime = endInsert - startInsert;
			unsigned long long insertTimeCompresstionDelay = hashmap.getTotalCompresstionDelayNS();


			if (teirCount == 4) {
				avgInsertThroughput += static_cast<double>(numElements) / (insertTime.count() + (double)insertTimeCompresstionDelay/1000000000.);
				avgLookupThroughput += static_cast<double>(numElements) / (lookupTime.count() + (double)lookupTimeCompresstionDelay/1000000000.);
				avgCompressionRatio += (static_cast<double>(compressedSize) / static_cast<double>(uncompressedSize));
				avgMemorySpaceSaving += 1 - (static_cast<double>(compressedSize) / static_cast<double>(kvSize));
				avgLoadFactor += loadFactor;
				count += 1;
			}
			// std::cout << teirCount << "\n";
		}
		if (count == 0) continue;
		std::cout << "Insert throughput: " << avgInsertThroughput / (float)(count) << " ops/s\n";
		std::cout << "Lookup throughput: " << avgLookupThroughput / (float)(count) << " ops/s\n";
		std::cout << "Compression ratio: " << avgCompressionRatio / (float)(count) << "\n";
		std::cout << "Memory Space Saving: " << avgMemorySpaceSaving / (float)(count) << "\n";
		std::cout << "Load factor: " << avgLoadFactor / (float)(count) << "\n";
	}
}
