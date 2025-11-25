#include <iostream>
#include <unordered_map>

#include "tieredCuckooHash.h"

int main() {
	{
		auto start = std::chrono::high_resolution_clock::now();
		for (unsigned int i = 0; i < 20; i++) {
			TieredCuckooHash<unsigned int, unsigned int> tieredCuckooHash;
			for (unsigned int j = 0; j < 1000000; j++) {
				tieredCuckooHash.insert({rand(), rand()});
			}
		}
		auto duration = std::chrono::high_resolution_clock::now() - start;
		std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1e9 / 20 << "\n";
	}
	{
		auto start = std::chrono::high_resolution_clock::now();
		for (unsigned int i = 0; i < 20; i++) {
			std::unordered_map<unsigned int, unsigned int> stdHashMap;
			for (unsigned int i = 0; i < 1000000; i++) {
				stdHashMap.insert({rand(), rand()});
			}
		}
		auto duration = std::chrono::high_resolution_clock::now() - start;
		std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1e9 / 20<< "\n";
	}
}
