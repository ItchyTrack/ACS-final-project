#ifndef tieredCuckooHash_h
#define tieredCuckooHash_h

#include <iostream>
#include <array>
#include <functional>
#include <utility>
#include <type_traits>
#include <stdexcept>

#include "lzav.h"

const unsigned int compressionBlockSize = 1024;//4096;//

#define doCompresstionDelay true
#define compresstionDelayAmountNS 200
#define compresstionDelayFirstTier 3 // T_4

template <typename T>
int compressed_size_after(const std::vector<T>& input, unsigned int blockSizeBytes) {
	if (input.empty()) return 0;

	unsigned long long compressed_size = 0;

	const int src_size = static_cast<int>(input.size() * sizeof(T));
	for (unsigned int blockIndex = 0; blockIndex < (src_size / blockSizeBytes); blockIndex += 1) {

		const int max_dst_size = lzav_compress_bound_hi(blockSizeBytes);
		std::vector<char> compressed(max_dst_size);

		compressed_size += lzav_compress_hi(
			reinterpret_cast<const char*>(input.data()) + (blockIndex * blockSizeBytes),
			compressed.data(),
			blockSizeBytes,
			max_dst_size
		);
	}
	return compressed_size;

	// // Calculate source size in bytes
	// const int src_size = static_cast<int>(input.size() * sizeof(T));
	// const int max_dst_size = lzav_compress_bound_hi(src_size);

	// std::vector<char> compressed(max_dst_size);

	// const int compressed_size = lzav_compress_hi(
	// 	reinterpret_cast<const char*>(input.data()),
	// 	compressed.data(),
	// 	src_size,
	// 	max_dst_size
	// );

	// return compressed_size;
}

#define kickFirst true
#define maxTableSize 10000000

template <
	class Key,
	class T,
	class Hash = std::hash<Key>,
	class KeyEqual = std::equal_to<Key>,
	class Allocator = std::allocator<std::pair<const Key, T>>, // no used, idk how it works
	std::size_t MaxLoopCountScale = 4096,
	std::size_t BinSize = 4,
	std::size_t InitialTierSize = 1024,
	std::size_t TierGrowthFactor = 5
>
class TieredCuckooHash {
#if doCompresstionDelay
public:
	unsigned long long getTotalCompresstionDelayNS() const { return totalCompresstionDelayNS; }
	void resetCompresstionDelay() { totalCompresstionDelayNS = 0; }
private:
	mutable unsigned long long totalCompresstionDelayNS = 0;
	constexpr void runCompresstionDelay(size_t tier) const {
		if (tier >= compresstionDelayFirstTier) totalCompresstionDelayNS += compresstionDelayAmountNS;
	}
#else
	constexpr void runCompresstionDelay(size_t tier) const {};
#endif
public:
	typedef Key															key_type;
	typedef T															mapped_type;
	typedef std::pair<const Key, T>										value_type;
	typedef std::size_t													size_type;
	typedef std::ptrdiff_t												difference_type;
	typedef Hash														hasher;
	typedef KeyEqual													key_equal;
	typedef Allocator													allocator_type;
	typedef value_type&													reference;
	typedef const value_type&											const_reference;
	typedef value_type*													pointer;
	typedef const value_type*											const_pointer;
	// typedef typename std::allocator_traits<Allocator>::pointer			pointer;
	// typedef typename std::allocator_traits<Allocator>::const_pointer	const_pointer;
private:
	typedef std::pair<const Key, T>										mut_value_type;
public:
	struct Bin {
		template<class... Args>
		void emplace(size_t index, Args&&... args) { ::new(&kvPairs[index]) mut_value_type(std::forward<Args>(args)...); }
		void destroy(size_t index) { std::launder(reinterpret_cast<mut_value_type*>(&kvPairs[index]))->~mut_value_type(); }
		mut_value_type& get(size_t index) { return *std::launder(reinterpret_cast<mut_value_type*>(&kvPairs[index])); }
		const mut_value_type& get(size_t index) const { return *std::launder(reinterpret_cast<const mut_value_type*>(&kvPairs[index])); }
		inline mut_value_type getMove(size_t index) {
			auto* p = std::launder(reinterpret_cast<mut_value_type*>(&kvPairs[index]));
			mut_value_type tmp = std::move(*p);
			destroy(index);
			return tmp;
		}
		std::array<std::aligned_storage_t<sizeof(mut_value_type), alignof(mut_value_type)>, BinSize> kvPairs;
		std::size_t kvCount = 0;
	};
	struct Tier {
		std::vector<Bin> table;
		size_t kvCount = 0;
	};
	class iterator {
		friend TieredCuckooHash;
	public:
		reference operator*() const { return tieredCuckooHash->tiers[tierIndex].table[binIndex].get(kvPairIndex); }
		pointer operator->() const { return std::addressof(operator*()); }
		// pointer operator->() const { return std::pointer_traits<pointer>::pointer_to(operator*()); }

		bool operator==(const iterator& o) const {
			return tieredCuckooHash == o.tieredCuckooHash &&
				tierIndex == o.tierIndex &&
				binIndex == o.binIndex &&
				kvPairIndex == o.kvPairIndex;
		}
		bool operator!=(const iterator& o) const { return !(*this==o); }

		iterator& operator++() {
			if (tierIndex == 0 && binIndex == InitialTierSize) return *this;
			if (kvPairIndex+1 < tieredCuckooHash->tiers[tierIndex].table[binIndex].kvCount) {
				++kvPairIndex;
				return *this;
			}
			kvPairIndex = 0;
			while (++binIndex < tieredCuckooHash->tiers[tierIndex].table.size()) {
				if (tieredCuckooHash->tiers[tierIndex].table[binIndex].kvCount > 0) {
					return *this;
				}
			}
			binIndex = 0;
			do {
				++tierIndex;
				if (tierIndex == tieredCuckooHash->tiers.size()) {
					tierIndex = 0;
					binIndex = InitialTierSize;
					return *this;
				}
			} while (tieredCuckooHash->tiers[tierIndex].kvCount == 0);
			while (tieredCuckooHash->tiers[tierIndex].table[binIndex].kvCount == 0) ++binIndex;
			return *this;
		}
		iterator operator++(int) {
			iterator iter(*this);
			++(*this);
			return iter;
		}
		// iterator& operator--() { // not needed
		// 	if (binIndex == InitialTierSize) {
		// 		tierIndex = tieredCuckooHash->tiers.size()-1;
		// 		binIndex = tieredCuckooHash->tiers[tierIndex].table.size();
		// 	} else if (kvPairIndex > 0 && tieredCuckooHash->tiers[tierIndex].table[binIndex].kvCount > 0) {
		// 		kvPairIndex = std::min(kvPairIndex - 1, tieredCuckooHash->tiers[tierIndex].table[binIndex].kvCount - 1);
		// 		return *this;
		// 	}
		// 	while (binIndex > 0) {
		// 		--binIndex;
		// 		if (tieredCuckooHash->tiers[tierIndex].table[binIndex].kvCount > 0) {
		// 			kvPairIndex = tieredCuckooHash->tiers[tierIndex].table[binIndex].kvCount - 1;
		// 			return *this;
		// 		}
		// 	}
		// 	do {
		// 		if (tierIndex == 0) {
		// 			binIndex = InitialTierSize;
		// 			kvPairIndex = 0;
		// 			return *this;
		// 		}
		// 		--tierIndex;
		// 	} while (tieredCuckooHash->tiers[tierIndex].kvCount == 0);
		// 	while (tieredCuckooHash->tiers[tierIndex].table[binIndex].kvCount == 0) ++binIndex;
		// 	kvPairIndex = tieredCuckooHash->tiers[tierIndex].table[binIndex].kvCount - 1;
		// 	return *this;
		// }
		// iterator operator--(int) {
		// 	iterator iter(*this);
		// 	--(*this);
		// 	return iter;
		// }

	private:
		iterator(
			TieredCuckooHash* tieredCuckooHash, std::size_t tierIndex, std::size_t binIndex, std::size_t kvPairIndex
		) : tieredCuckooHash(tieredCuckooHash), tierIndex(tierIndex), binIndex(binIndex), kvPairIndex(kvPairIndex) {}

		TieredCuckooHash* tieredCuckooHash;
		std::size_t tierIndex;
		std::size_t binIndex;
		std::size_t kvPairIndex;
	};
	class const_iterator {
		friend TieredCuckooHash;
	public:
		const_reference operator*() const { return tieredCuckooHash->tiers[tierIndex].table[binIndex].get(kvPairIndex); }
		pointer operator->() const { return std::addressof(operator*()); }
		// const_pointer operator->() const { return std::pointer_traits<pointer>::pointer_to(operator*()); }

		bool operator==(const const_iterator& o) const {
			return tieredCuckooHash == o.tieredCuckooHash &&
				tierIndex == o.tierIndex &&
				binIndex == o.binIndex &&
				kvPairIndex == o.kvPairIndex;
		}
		bool operator!=(const const_iterator& o) const { return !(*this==o); }

		const_iterator& operator++() {
			if (tierIndex == 0 && binIndex == InitialTierSize) return *this;
			if (kvPairIndex+1 < tieredCuckooHash->tiers[tierIndex].table[binIndex].kvCount) {
				++kvPairIndex;
				return *this;
			}
			kvPairIndex = 0;
			while (++binIndex < tieredCuckooHash->tiers[tierIndex].table.size()) {
				if (tieredCuckooHash->tiers[tierIndex].table[binIndex].kvCount > 0) {
					return *this;
				}
			}
			binIndex = 0;
			do {
				++tierIndex;
				if (tierIndex == tieredCuckooHash->tiers.size()) {
					tierIndex = 0;
					binIndex = InitialTierSize;
					return *this;
				}
			} while (tieredCuckooHash->tiers[tierIndex].kvCount == 0);
			while (tieredCuckooHash->tiers[tierIndex].table[binIndex].kvCount == 0) ++binIndex;
			return *this;
		}
		const_iterator operator++(int) {
			const_iterator iter(*this);
			++(*this);
			return iter;
		}
		// const_iterator& operator--() { // not needed
		// 	if (binIndex == InitialTierSize) {
		// 		tierIndex = tieredCuckooHash->tiers.size()-1;
		// 		binIndex = tieredCuckooHash->tiers[tierIndex].table.size();
		// 	} else if (kvPairIndex > 0 && tieredCuckooHash->tiers[tierIndex].table[binIndex].kvCount > 0) {
		// 		kvPairIndex = std::min(kvPairIndex - 1, tieredCuckooHash->tiers[tierIndex].table[binIndex].kvCount - 1);
		// 		return *this;
		// 	}
		// 	while (binIndex > 0) {
		// 		--binIndex;
		// 		if (tieredCuckooHash->tiers[tierIndex].table[binIndex].kvCount > 0) {
		// 			kvPairIndex = tieredCuckooHash->tiers[tierIndex].table[binIndex].kvCount - 1;
		// 			return *this;
		// 		}
		// 	}
		// 	do {
		// 		if (tierIndex == 0) {
		// 			binIndex = InitialTierSize;
		// 			kvPairIndex = 0;
		// 			return *this;
		// 		}
		// 		--tierIndex;
		// 	} while (tieredCuckooHash->tiers[tierIndex].kvCount == 0);
		// 	while (tieredCuckooHash->tiers[tierIndex].table[binIndex].kvCount == 0) ++binIndex;
		// 	kvPairIndex = tieredCuckooHash->tiers[tierIndex].table[binIndex].kvCount - 1;
		// 	return *this;
		// }
		// const_iterator operator--(int) {
		// 	const_iterator iter(*this);
		// 	--(*this);
		// 	return iter;
		// }

	private:
		const_iterator(
			const TieredCuckooHash* tieredCuckooHash, std::size_t tierIndex, std::size_t binIndex, std::size_t kvPairIndex
		) : tieredCuckooHash(tieredCuckooHash), tierIndex(tierIndex), binIndex(binIndex), kvPairIndex(kvPairIndex) {}

		const TieredCuckooHash* tieredCuckooHash;
		std::size_t tierIndex;
		std::size_t binIndex;
		std::size_t kvPairIndex;
	};
	typedef iterator local_iterator;
	typedef const_iterator const_local_iterator;

	// allocator_type get_allocator() const noexcept { return ;} //

	// Iterators
	iterator begin() {
		if (empty()) return end();
		std::size_t tierIndex = 0;
		while (tiers[tierIndex].kvCount == 0) { ++tierIndex; }
		runCompresstionDelay(tierIndex);
		std::size_t binIndex = 0;
		while (tiers[tierIndex].table[binIndex].kvCount == 0) { ++binIndex; }
		return iterator(this, tierIndex, binIndex, 0);
	}
	const_iterator begin() const {
		return cbegin();
	}
	const_iterator cbegin() const {
		if (empty()) return cend();
		std::size_t tierIndex = 0;
		while (tiers[tierIndex].kvCount == 0) { ++tierIndex; }
		runCompresstionDelay(tierIndex);
		std::size_t binIndex = 0;
		while (tiers[tierIndex].table[binIndex].kvCount == 0) { ++binIndex; }
		return const_iterator(this, tierIndex, binIndex, 0);
	}

	iterator end() { return iterator(this, 0, InitialTierSize, 0); }
	const_iterator end() const { return cend(); }
	const_iterator cend() const { return const_iterator(this, 0, InitialTierSize, 0); }

	// Capacity
	bool empty() const { return kvCount == 0; }
	size_type size() const { return kvCount; }
	size_type max_size() const { return std::numeric_limits<difference_type>::max()/2; } // should be fine

	// Modifiers
	void clear() { tiers.clear(); kvCount = 0; }
	// ------------------------------ insert(const value_type& value) ------------------------------
	std::pair<iterator, bool> insert(const value_type& value) {
		std::size_t hash = hasher{}(value.first);
		if (tiers.size() == 0) {
			addTier();
		} else { // if there were no tiers it cant be in one
			// check if the key is in the map
			for (std::size_t tierIndex = 0; tierIndex < tiers.size(); ++tierIndex) {
				runCompresstionDelay(tierIndex);
				std::size_t binIndex = hash % tiers[tierIndex].table.size();
				const Bin& bin = tiers[tierIndex].table[binIndex];
				for (std::size_t kvPairIndex = 0; kvPairIndex < bin.kvCount; ++kvPairIndex) {
					if (key_equal{}(value.first, bin.get(kvPairIndex).first)) {
						return {iterator(this, tierIndex, binIndex, kvPairIndex), false};
					}
				}
			}
		}
		// add it otherwise
		++kvCount;
		Tier& tier = tiers[0];
		std::size_t binIndex = hash % tier.table.size();
		Bin& bin = tier.table[binIndex];
		if (bin.kvCount < bin.kvPairs.size()) {
			bin.emplace(bin.kvCount, value);
			++tier.kvCount;
			return {iterator(this, 0, binIndex, bin.kvCount++), true};
		}
#if (kickFirst)
		std::size_t kickIndex = 0;
#else
		std::size_t kickIndex = hash % bin.kvCount;
#endif
		mut_value_type otherValue = bin.getMove(kickIndex);
		bin.emplace(kickIndex, value);
		insertNonExistingKey(std::move(otherValue), 1);
		return {iterator(this, 0, binIndex, kickIndex), true};
	}
	// ------------------------------ insert(value_type&& value) ------------------------------
	std::pair<iterator, bool> insert(value_type&& value) {
		std::size_t hash = hasher{}(value.first);
		if (tiers.size() == 0) {
			addTier();
		} else { // if there were no tiers it cant be in one
			// check if the key is in the map
			for (std::size_t tierIndex = 0; tierIndex < tiers.size(); ++tierIndex) {
				runCompresstionDelay(tierIndex);
				std::size_t binIndex = hash % tiers[tierIndex].table.size();
				const Bin& bin = tiers[tierIndex].table[binIndex];
				for (std::size_t kvPairIndex = 0; kvPairIndex < bin.kvCount; ++kvPairIndex) {
					if (key_equal{}(value.first, bin.get(kvPairIndex).first)) {
						return {iterator(this, tierIndex, binIndex, kvPairIndex), false};
					}
				}
			}
		}
		// add it otherwise
		++kvCount;
		Tier& tier = tiers[0];
		std::size_t binIndex = hash % tier.table.size();
		Bin& bin = tier.table[binIndex];
		if (bin.kvCount < bin.kvPairs.size()) {
			bin.emplace(bin.kvCount, value);
			++tier.kvCount;
			return {iterator(this, 0, binIndex, bin.kvCount++), true};
		}
#if (kickFirst)
		std::size_t kickIndex = 0;
#else
		std::size_t kickIndex = hash % bin.kvCount;
#endif
		mut_value_type otherValue = bin.getMove(kickIndex);
		bin.emplace(kickIndex, std::move(value));
		insertNonExistingKey(std::move(otherValue), 1);
		return {iterator(this, 0, binIndex, kickIndex), true};
	}
	// ------------------------------ insert_or_assign(const Key& k, M&& obj) ------------------------------
	template <class M>
	std::pair<iterator, bool> insert_or_assign(const Key& key, M&& obj) {
		std::pair<iterator, bool> pair = emplace_key_and_args(key, std::forward<M>(obj));
		if (!pair.second) {
			pair.first->second =  std::forward<M>(obj);
		}
		return pair;
	}
	// ------------------------------ insert_or_assign(Key&& k, M&& obj) ------------------------------
	template <class M>
	std::pair<iterator, bool> insert_or_assign(Key&& key, M&& obj) {
		std::pair<iterator, bool> pair = emplace_key_and_args(std::move(key), std::forward<M>(obj));
		if (!pair.second) {
			pair.first->second = std::forward<M>(obj);
		}
		return pair;
	}
	// ------------------------------ insert_or_assign(const_iterator, const Key& k, M&& obj) ------------------------------
	template <class M>
	iterator insert_or_assign(const_iterator, const Key& key, M&& obj) {
		return insert_or_assign(key, std::forward<M>(obj)).first;
	}
	// ------------------------------ insert_or_assign(const_iterator, Key&& k, M&& obj) ------------------------------
	template <class M>
	iterator insert_or_assign(const_iterator, Key&& key, M&& obj) {
		return insert_or_assign(std::move(key), std::forward<M>(obj)).first;
	}
	// ------------------------------ emplace(Args&&... args) ------------------------------
	template<class... Args>
	std::pair<iterator, bool> emplace(Args&&... args) {
		return emplace_key_in_args(std::forward<Args>(args)...);
	}
	// ------------------------------ emplace_hint(const_iterator, Args&&... args) ------------------------------
	template<class... Args>
	iterator emplace_hint(const_iterator, Args&&... args) {
		return emplace(std::forward<Args>(args)...).first;
	}
	// ------------------------------ try_emplace(const Key& k, Args&&... args) ------------------------------
	template<class... Args>
	std::pair<iterator, bool> try_emplace(const Key& key, Args&&... args) {
		return emplace_key_and_args(key, std::forward<Args>(args)...);
	}
	// ------------------------------ try_emplace(const_iterator, const Key& k, Args&&... args) ------------------------------
	template<class... Args>
	iterator try_emplace(const_iterator, const Key& key, Args&&... args) {
		return try_emplace(key, std::forward<Args>(args)...).first;
	}
	// ------------------------------ erase(iterator pos) ------------------------------
	iterator erase(iterator pos) {
		Bin& bin = tiers[pos.tierIndex].table[pos.binIndex];
		--kvCount;
		--tiers[pos.tierIndex].kvCount;
		if (bin.kvCount-1 == pos.kvPairIndex) {
			bin.destroy(pos.kvPairIndex);
			--bin.kvCount;
			return ++pos;
		} else {
			bin.emplace(pos.kvPairIndex, bin.getMove(bin.kvCount - 1));
			--bin.kvCount;
			return pos;
		}
	}
	// ------------------------------ erase(const_iterator pos) ------------------------------
	iterator erase(const_iterator pos) {
		Bin& bin = tiers[pos.tierIndex].table[pos.binIndex];
		--kvCount;
		--tiers[pos.tierIndex].kvCount;
		if (bin.kvCount-1 == pos.kvPairIndex) {
			bin.destroy(pos.kvPairIndex);
			--bin.kvCount;
			return ++pos;
		} else {
			bin.emplace(pos.kvPairIndex, bin.getMove(bin.kvCount - 1));
			--bin.kvCount;
			return pos;
		}
	}
	// ------------------------------ erase(const_iterator first, const_iterator last) ------------------------------
	iterator erase(const_iterator first, const_iterator last) {
		for (; first != last; first = erase(first)) {}
		return iterator(last.tieredCuckooHash, last.tierIndex, last.binIndex, last.kvPairIndex);
	}
	// ------------------------------ erase(const Key& key) ------------------------------
	size_type erase(const Key& key) {
		iterator iter = find(key);
		if (iter == end())
			return 0;
		erase(iter);
		return 1;
	}
	// Lookup
	// ------------------------------ at(const Key& key) ------------------------------
	T& at(const Key& key) {
		iterator iter = find(key);
		if (iter == end())
			throw std::out_of_range("TieredCuckooHash::at: key not found");
		return iter->second;
	}
	// ------------------------------ at(const Key& key) const ------------------------------
	const T& at(const Key& key) const {
		const_iterator iter = find(key);
		if (iter == end())
			throw std::out_of_range("TieredCuckooHash::at: key not found");
		return iter->second;
	}
	// ------------------------------ operator[](const Key& key) ------------------------------
	T& operator[](const Key& key) {
		return emplace(key, mapped_type()).first->second;
	}
	// ------------------------------ operator[](Key&& key) ------------------------------
	T& operator[](Key&& key) {
		return emplace(std::move(key), mapped_type()).first->second;
	}
	// ------------------------------ count(const Key& key) ------------------------------
	size_type count(const Key& key) const {
		return static_cast<size_type>(find(key) != end());
	}
	// ------------------------------ find(const Key& key) ------------------------------
	iterator find(const Key& key) {
		std::size_t hash = hasher{}(key);
		for (std::size_t tierIndex = 0; tierIndex < tiers.size(); ++tierIndex) {
			runCompresstionDelay(tierIndex);
			std::size_t binIndex = hash % tiers[tierIndex].table.size();
			const Bin& bin = tiers[tierIndex].table[binIndex];
			for (std::size_t kvPairIndex = 0; kvPairIndex < bin.kvCount; ++kvPairIndex) {
				if (key_equal{}(key, bin.get(kvPairIndex).first)) {
					return iterator(this, tierIndex, binIndex, kvPairIndex);
				}
			}
		}
		return end();
	}
	// ------------------------------ find(const Key& key) const ------------------------------
	const_iterator find(const Key& key) const {
		std::size_t hash = hasher{}(key);
		for (std::size_t tierIndex = 0; tierIndex < tiers.size(); ++tierIndex) {
			runCompresstionDelay(tierIndex);
			std::size_t binIndex = hash % tiers[tierIndex].table.size();
			const Bin& bin = tiers[tierIndex].table[binIndex];
			for (std::size_t kvPairIndex = 0; kvPairIndex < bin.kvCount; ++kvPairIndex) {
				if (key_equal{}(key, bin.get(kvPairIndex).first)) {
					return const_iterator(this, tierIndex, binIndex, kvPairIndex);
				}
			}
		}
		return cend();
	}
	// ------------------------------ contains(const Key& key) const ------------------------------
	bool contains( const Key& key ) const {
		return find(key) != end();
	}
	// ------------------------------ equal_range(const Key& key) ------------------------------
	std::pair<iterator, iterator> equal_range(const Key& key) {
		iterator iter = find(key);
		iterator endIter = iter;
		if (iter != end())
			++endIter;
		return std::pair<iterator, iterator>(iter, endIter);
	}
	// ------------------------------ equal_range(const Key& key) const ------------------------------
	std::pair<const_iterator, const_iterator> equal_range(const Key& key) const {
		const_iterator iter = find(key);
		const_iterator endIter = iter;
		if (iter != end())
			++endIter;
		return std::pair<const_iterator, const_iterator>(iter, endIter);
	}
	// Bucket interface
	// // ------------------------------ begin(size_type) ------------------------------
	// local_iterator begin(size_type n) {
	// 	size_type binCount = tiers[0];
	// 	size_type tierIndex = 0;
	// 	while (binCount <= n) {
	// 		binCount += tiers[++tierIndex].table.size();
	// 		if (tierIndex >= tiers.size()) return end();
	// 	}
	// 	return local_iterator(this, tierIndex, n - (binCount - tiers[++tierIndex].table.size()), 0);
	// }
	// // ------------------------------ begin(size_type) const ------------------------------
	// const_local_iterator begin(size_type n) const {
	// 	return cbegin(n);
	// }
	// // ------------------------------ cbegin(size_type) const ------------------------------
	// const_local_iterator cbegin(size_type n) const {
	// 	size_type binCount = tiers[0];
	// 	size_type tierIndex = 0;
	// 	while (binCount <= n) {
	// 		binCount += tiers[++tierIndex].table.size();
	// 		if (tierIndex >= tiers.size()) return end();
	// 	}
	// 	return const_local_iterator(this, tierIndex, n - (binCount - tiers[++tierIndex].table.size()), 0);
	// }
	// // ------------------------------ end(size_type) ------------------------------
	// local_iterator end(size_type n) {
	// 	size_type binCount = tiers[0];
	// 	size_type tierIndex = 0;
	// 	while (binCount <= n) {
	// 		binCount += tiers[++tierIndex].table.size();
	// 		if (tierIndex >= tiers.size()) return end();
	// 	}
	// 	return local_iterator(this, tierIndex, n - (binCount - tiers[++tierIndex].table.size()), 0);
	// }
	// // ------------------------------ end(size_type) const ------------------------------
	// const_local_iterator end(size_type n) const {
	// 	return cend(n);
	// }
	// // ------------------------------ cend(size_type) const ------------------------------
	// const_local_iterator cend(size_type n) const {
	// 	size_type binCount = tiers[0];
	// 	size_type tierIndex = 0;
	// 	while (binCount <= n) {
	// 		binCount += tiers[++tierIndex].table.size();
	// 		if (tierIndex >= tiers.size()) return end();
	// 	}
	// 	return const_local_iterator(this, tierIndex, n - (binCount - tiers[++tierIndex].table.size()), 0);
	// }
	// ------------------------------ bucket_count() const ------------------------------
	size_type bucket_count() const {
		size_type binCount = 0;
		for (const Tier& tier : tiers) {
			binCount += tier.table.size();
		}
		return binCount;
	}
	// Hash policy
	// ------------------------------ load_factor() const ------------------------------
	float load_factor() const {
		return (float)(size() * sizeof(value_type)) / (float)(bucket_count() * sizeof(Bin));
	}
	// ------------------------------ max_load_factor() const ------------------------------
	float max_load_factor() const {
		return BinSize;
	}
	// ------------------------------ max_load_factor() const ------------------------------
	void max_load_factor(float ml) {} // does not doing anything
	// ------------------------------ rehash(size_type count) ------------------------------
	void rehash(size_type count) {} // does not need to do anything
	// ------------------------------ reserve(size_type count) ------------------------------
	void reserve(size_type count) {
		rehash(std::ceil(count / max_load_factor()));
	}

	void visualize() const {
		for (const Tier& tier : tiers) {
			for (const Bin& bin : tier.table) {
				std::cout << "[";
				bool first = true;
				for (std::size_t i = 0; i < bin.kvCount; ++i) {
					if (!first) std::cout << ",";
					const mut_value_type& value = bin.get(i);
					std::cout << value.first << ":" << value.second;
					first = false;
				}
				std::cout << "]";
			}
			std::cout << "\n";
		}
	}

	size_t getUncompressedSize() const {
		size_t size = 0;
		for (const Tier& tier : tiers) {
			size += tier.table.size() * sizeof(Bin);
		}
		return size;
	}
	size_t getDataSize() const {
		return size() * sizeof(value_type);
	}
	size_t getCompressedSize() const {
		size_t size = 0;
		size_t index = 0;
		for (const Tier& tier : tiers) {
			if (index < compresstionDelayFirstTier) {
				size += tier.table.size() * sizeof(Bin);
			} else {
				size += compressed_size_after(tier.table, compressionBlockSize);
			}
			index++;
		}
		return size;
	}

	size_t getTierCount() const {
		return tiers.size();
	}

private:
	// might contain key
	template<class A0, class... Args>
	std::pair<iterator, bool> emplace_key_in_args(A0&& a0, Args&&... args) {
		return emplace_key_and_args(static_cast<const Key&>(a0), std::forward<Args>(args)...);
	}
	template<class... Args>
	std::pair<iterator, bool> emplace_key_and_args(const Key& key, Args&&... args) {
		std::size_t hash = hasher{}(key);
		if (tiers.size() == 0) {
			addTier();
		} else { // if there were no tiers it cant be in one
			// check if the key is in the map
			for (std::size_t tierIndex = 0; tierIndex < tiers.size(); ++tierIndex) {
				runCompresstionDelay(tierIndex);
				std::size_t binIndex = hash % tiers[tierIndex].table.size();
				const Bin& bin = tiers[tierIndex].table[binIndex];
				for (std::size_t kvPairIndex = 0; kvPairIndex < bin.kvCount; ++kvPairIndex) {
					if (key_equal{}(key, bin.get(kvPairIndex).first)) {
						return {iterator(this, tierIndex, binIndex, kvPairIndex), false};
					}
				}
			}
		}
		// add it otherwise
		++kvCount;
		Tier& tier = tiers[0];
		std::size_t binIndex = hash % tier.table.size();
		Bin& bin = tier.table[binIndex];
		if (bin.kvCount < bin.kvPairs.size()) {
			bin.emplace(bin.kvCount, key, std::forward<Args>(args)...);
			++tier.kvCount;
			return {iterator(this, 0, binIndex, bin.kvCount++), true};
		}
#if (kickFirst)
		std::size_t kickIndex = 0;
#else
		std::size_t kickIndex = hash % bin.kvCount;
#endif
		mut_value_type otherValue = bin.getMove(kickIndex);
		bin.emplace(kickIndex, key, std::forward<Args>(args)...);
		insertNonExistingKey(std::move(otherValue), 1);
		return {iterator(this, 0, binIndex, kickIndex), true};
	}
	// does not contain key
	iterator insertNonExistingKey(mut_value_type&& value, std::size_t tierIndex, std::size_t loopCount = 0) {
		if (loopCount > MaxLoopCountScale * tiers.size()) {
			addTier();
		}
		if (tiers.size() <= tierIndex) {
			addTier();
			Tier& tier = tiers[tiers.size()-1];
			++tier.kvCount;
			std::size_t hash = hasher{}(value.first);
			std::size_t binIndex = hash % tier.table.size();
			Bin& bin = tier.table[binIndex];
			bin.emplace(0, std::move(value));
			return iterator(this, tiers.size()-1, binIndex, bin.kvCount++);
		}
		runCompresstionDelay(tierIndex);
		Tier& tier = tiers[tierIndex];
		std::size_t hash = hasher{}(value.first);
		std::size_t binIndex = hash % tier.table.size();
		Bin& bin = tier.table[binIndex];
		if (bin.kvCount < bin.kvPairs.size()) {
			bin.emplace(bin.kvCount, std::move(value));
			++tier.kvCount;
			return iterator(this, tierIndex, binIndex, bin.kvCount++);
		}
#if (kickFirst)
		std::size_t kickIndex = 0;
#else
		std::size_t kickIndex = hash % bin.kvCount;
#endif
		mut_value_type otherValue = bin.getMove(kickIndex);
		bin.emplace(kickIndex, std::move(value));
		insertNonExistingKey(std::move(otherValue), (tierIndex + 1) % tiers.size(), loopCount + 1);
		return iterator(this, tierIndex, binIndex, kickIndex);
	}
	void addTier() {
		// pow
		size_t result = 1;
		size_t exp = tiers.size();
		size_t base = TierGrowthFactor;
		while (true) {
			if (exp & 1) result *= base;
			if (result >= maxTableSize) {
				std::cout << "Warning: Max array size of " << maxTableSize << " hit\n";
				result = maxTableSize;
				break;
			}
			exp >>= 1;
			if (!exp) break;
			base *= base;
		}
		// add tier
		tiers.push_back(Tier{std::vector<Bin>(InitialTierSize*result), 0});
	}

	size_type kvCount = 0;
	std::vector<Tier> tiers;
};

#endif /* tieredCuckooHash_h */
