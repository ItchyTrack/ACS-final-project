#include <unordered_map>
#include <iostream>
#include <cassert>
#include <random>
#include <string>

#include "tieredCuckooHash.h"

int test() {
    using Key = int;
    using Value = std::string;

    TieredCuckooHash<Key, Value> cuckooMap;
    std::unordered_map<Key, Value> stdMap;

	std::size_t keyDistMax = 1000000;
    std::mt19937 rng(42); // fixed seed for reproducibility
    std::uniform_int_distribution<int> keyDist(0, keyDistMax);
    std::uniform_int_distribution<int> valDist(0, 100000);

    auto randomString = [&](int len = 8) {
        std::string s;
        for (int i = 0; i < len; ++i) {
            char c = 'a' + (valDist(rng) % 26);
            s.push_back(c);
        }
        return s;
    };

    const int iterations = 1000000;
    for (int i = 0; i < iterations; ++i) {
        int key = keyDist(rng);
        std::string val = randomString();

        if (i % 2 == 0) {
            cuckooMap.insert_or_assign(key, val);
            stdMap.insert_or_assign(key, val);
        } else {
            cuckooMap.try_emplace(key, val);
            stdMap.try_emplace(key, val);
        }
    }

    // Validate sizes
    assert(cuckooMap.size() == stdMap.size());

    // Validate contents
	size_t c1 = 0;
    for (const auto& kv : stdMap) {
        assert(cuckooMap.find(kv.first) != cuckooMap.end());
        assert(cuckooMap.at(kv.first) == kv.second);
		c1++;
    }
	size_t c2 = 0;
    for (const auto& kv : cuckooMap) {
        assert(stdMap.find(kv.first) != stdMap.end());
        assert(stdMap.at(kv.first) == kv.second);
		assert(cuckooMap.find(kv.first) != cuckooMap.end());
        assert(cuckooMap.at(kv.first) == kv.second);
		c2++;
    }
	assert(c1 == c2);
	size_t c3 = 0;
    for (auto iter = cuckooMap.begin(); iter != cuckooMap.end(); ++iter) {
		const auto& kv = *iter;
        assert(stdMap.find(kv.first) != stdMap.end());
        assert(stdMap.at(kv.first) == kv.second);
		assert(cuckooMap.find(kv.first) != cuckooMap.end());
        assert(cuckooMap.at(kv.first) == kv.second);
		c3++;
    }
	assert(c1 == c3);
	size_t c4 = 0;
    for (auto iter = cuckooMap.cbegin(); iter != cuckooMap.cend(); ++iter) {
		const auto& kv = *iter;
        assert(stdMap.find(kv.first) != stdMap.end());
        assert(stdMap.at(kv.first) == kv.second);
		assert(cuckooMap.find(kv.first) != cuckooMap.end());
        assert(cuckooMap.at(kv.first) == kv.second);
		c4++;
    }
	assert(c1 == c4);

    // Validate operator[]
    for (const auto& kv : stdMap) {
        cuckooMap[kv.first] = kv.second;
        assert(cuckooMap.at(kv.first) == kv.second);
    }

    // Validate emplace
    cuckooMap.emplace(keyDistMax + 1, "test_val");
    assert(cuckooMap.at(keyDistMax + 1) == "test_val");

    std::cout << "All tests passed successfully!\n";
    return 0;
}

void speedTest() {
	std::cout << "write\n";
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
			stdHashMap.reserve(1024);
			for (unsigned int i = 0; i < 1000000; i++) {
				stdHashMap.insert({rand(), rand()});
			}
		}
		auto duration = std::chrono::high_resolution_clock::now() - start;
		std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1e9 / 20<< "\n";
	}

	std::cout << "read\n";
	{
		TieredCuckooHash<unsigned int, unsigned int> tieredCuckooHash;
		std::unordered_map<unsigned int, unsigned int> stdHashMap;
		for (unsigned int j = 0; j < 2000000; j++) {
			auto pair = std::make_pair(rand(), rand());
			tieredCuckooHash.insert(pair);
			stdHashMap.insert(pair);
		}
		{
			auto start = std::chrono::high_resolution_clock::now();
			for (unsigned int j = 0; j < 1000000; j++) {
				tieredCuckooHash.find(rand());
			}
			auto duration = std::chrono::high_resolution_clock::now() - start;
			std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1e9 << "\n";
		}
		{
			auto start = std::chrono::high_resolution_clock::now();
			for (unsigned int i = 0; i < 1000000; i++) {
				stdHashMap.find(rand());
			}
			auto duration = std::chrono::high_resolution_clock::now() - start;
			std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1e9 << "\n";
		}
	}

	std::cout << "write/read\n";
	{
		auto start = std::chrono::high_resolution_clock::now();
		for (unsigned int i = 0; i < 20; i++) {
			TieredCuckooHash<unsigned int, unsigned int> tieredCuckooHash;
			for (unsigned int j = 0; j < 1000000; j++) {
				tieredCuckooHash.insert({rand(), rand()});
				tieredCuckooHash.find(rand());
			}
		}
		auto duration = std::chrono::high_resolution_clock::now() - start;
		std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1e9 / 20 << "\n";
	}
	{
		auto start = std::chrono::high_resolution_clock::now();
		for (unsigned int i = 0; i < 20; i++) {
			std::unordered_map<unsigned int, unsigned int> stdHashMap;
			stdHashMap.reserve(1024);
			for (unsigned int i = 0; i < 1000000; i++) {
				stdHashMap.insert({rand(), rand()});
				stdHashMap.find(rand());
			}
		}
		auto duration = std::chrono::high_resolution_clock::now() - start;
		std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1e9 / 20<< "\n";
	}

	std::cout << "write/read local\n";
	{
		auto start = std::chrono::high_resolution_clock::now();
		for (unsigned int i = 0; i < 10; i++) {
			TieredCuckooHash<unsigned int, unsigned int> tieredCuckooHash;
			for (unsigned int j = 0; j < 1000; j++) {
				unsigned int zone = rand();
				for (unsigned int j = 0; j < 10000; j++) {
					tieredCuckooHash.insert({zone + (rand() % 2500), rand()});
					tieredCuckooHash.find(zone + (rand() % 2500));
				}
			}
		}
		auto duration = std::chrono::high_resolution_clock::now() - start;
		std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1e9 / 10 << "\n";
	}
	{
		auto start = std::chrono::high_resolution_clock::now();
		for (unsigned int i = 0; i < 10; i++) {
			std::unordered_map<unsigned int, unsigned int> stdHashMap;
			stdHashMap.reserve(1024);
			for (unsigned int i = 0; i < 1000; i++) {
				unsigned int zone = rand();
				for (unsigned int i = 0; i < 10000; i++) {
					stdHashMap.insert({zone + (rand() % 2500), rand()});
					stdHashMap.find(zone + (rand() % 2500));
				}
			}
		}
		auto duration = std::chrono::high_resolution_clock::now() - start;
		std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1e9 / 10<< "\n";
	}
}


int main() {
	test();
	// speedTest();
	return 0;
}
