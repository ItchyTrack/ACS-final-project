#ifndef tieredCuckooHash_h
#define tieredCuckooHash_h

#include <iostream>
#include <array>
#include <optional>
#include <limits>
#include <functional>
#include <utility>
#include <type_traits>
#include <stdexcept>

#define kickFirst true
#define maxTableSize 10000000

template <
	class Key,
	class T,
	class Hash = std::hash<Key>,
    class KeyEqual = std::equal_to<Key>,
    class Allocator = std::allocator<std::pair<const Key, T>>, // no used, idk how it works
	std::size_t MaxLoopCountScale = 4096,
	std::size_t BinSize = 16,
	std::size_t InitialTierSize = 1000,
	std::size_t TierGrowthFactor = 4
>
class TieredCuckooHash {
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
	typedef typename std::allocator_traits<Allocator>::pointer			pointer;
	typedef typename std::allocator_traits<Allocator>::const_pointer	const_pointer;
private:
	typedef std::pair<Key, T>										mut_value_type;
public:
	struct Bin {
		std::array<std::optional<mut_value_type>, BinSize> kvPairs;
		std::size_t size = 0;
	};
	struct Tier {
		std::vector<Bin> table;
		size_t kvCount;
	};
	class iterator {
		friend TieredCuckooHash;
	public:
		reference operator*() const { return (reference)tieredCuckooHash->tiers[tierIndex].table[binIndex].kvPairs[kvPairIndex].value(); }
		pointer operator->() const { return std::pointer_traits<pointer>::pointer_to(operator*()); }

		bool operator==(const iterator& o) const {
			return tieredCuckooHash == o.tieredCuckooHash &&
				tierIndex == o.tierIndex &&
				binIndex == o.binIndex &&
				kvPairIndex == o.kvPairIndex;
		}
		bool operator!=(const iterator& o) const { return !(*this==o); }

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
		reference operator*() const { return tieredCuckooHash->tiers[tierIndex][binIndex].kvPairs[kvPairIndex]; }
		pointer operator->() const { return std::pointer_traits<pointer>::pointer_to(operator*()); }

		bool operator==(const iterator& o) const {
			return tieredCuckooHash == o.tieredCuckooHash &&
				tierIndex == o.tierIndex &&
				binIndex == o.binIndex &&
				kvPairIndex == o.kvPairIndex;
		}
		bool operator!=(const iterator& o) const { return !(*this==o); }

	private:
		const_iterator(
			const TieredCuckooHash* tieredCuckooHash, std::size_t tierIndex, std::size_t binIndex, std::size_t kvPairIndex
		) : tieredCuckooHash(tieredCuckooHash), tierIndex(tierIndex), binIndex(binIndex), kvPairIndex(kvPairIndex) {}

		const TieredCuckooHash* tieredCuckooHash;
		std::size_t tierIndex;
		std::size_t binIndex;
		std::size_t kvPairIndex;
	};

	// allocator_type get_allocator() const noexcept { return ;} //

	// Iterators
	iterator begin() { return iterator(this, 0, 0, 0); }
	const_iterator begin() const { return const_iterator(this, 0, 0, 0); }
	const_iterator cbegin() const { return const_iterator(this, 0, 0, 0); }

	iterator end() { return iterator(this, 0, 0, 0); }
	const_pointer end() const { return const_iterator(this, 0, 0, 0); }
	const_pointer cend() const { return const_iterator(this, 0, 0, 0); }

	// Capacity
	bool empty() const { return kvCount == 0; }
	size_type size() const { return kvCount; }
	size_type max_size() const { return std::numeric_limits<difference_type>::max()/2; } // should be fine

	// Modifiers
	void clear() { tiers.clear(); }
	// ------------------------------ insert(const value_type& value) ------------------------------
	std::pair<iterator, bool> insert(const value_type& value) {
		std::size_t hash = hasher{}(value.first);
		if (tiers.size() == 0) {
			addTier();
		} else { // if there were no tiers it cant be in one
			// check if the key is in the map
			for (std::size_t tierIndex = 0; tierIndex < tiers.size(); ++tierIndex) {
				std::size_t binIndex = hash % tiers[tierIndex].table.size();
				const Bin& bin = tiers[tierIndex].table[binIndex];
				for (std::size_t kvPairIndex = 0; kvPairIndex < bin.size; ++kvPairIndex) {
					if (key_equal{}(value.first, bin.kvPairs[kvPairIndex]->first)) {
						return {iterator(this, tierIndex, binIndex, kvPairIndex), false};
					}
				}
			}
		}
		// add it otherwise
		++kvCount;
		Tier& tier = tiers[0];
		std::size_t binIndex = hash % tier.table.size();
		Bin& bin = tier[binIndex];
		if (bin.size < bin.kvPairs.size()) {
			bin.kvPairs[bin.size] = value;
			++tier.kvCount;
			return {iterator(this, 0, binIndex, bin.size++), true};
		}
#if (kickFirst)
		std::size_t kickIndex = 0;
#else
		std::size_t kickIndex = hash % bin.size;
#endif
		mut_value_type otherValue = std::move(bin.kvPairs[kickIndex].value());
		bin.kvPairs[kickIndex] = value;
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
				std::size_t binIndex = hash % tiers[tierIndex].table.size();
				const Bin& bin = tiers[tierIndex].table[binIndex];
				for (std::size_t kvPairIndex = 0; kvPairIndex < bin.size; ++kvPairIndex) {
					if (key_equal{}(value.first, bin.kvPairs[kvPairIndex]->first)) {
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
		if (bin.size < bin.kvPairs.size()) {
			bin.kvPairs[bin.size] = value;
			++tier.kvCount;
			return {iterator(this, 0, binIndex, bin.size++), true};
		}
#if (kickFirst)
		std::size_t kickIndex = 0;
#else
		std::size_t kickIndex = hash % bin.size;
#endif
		mut_value_type otherValue = std::move(bin.kvPairs[kickIndex].value());
		bin.kvPairs[kickIndex] = std::move(value);
		insertNonExistingKey(std::move(otherValue), 1);
		return {iterator(this, 0, binIndex, kickIndex), true};
	}
	// ------------------------------ insert_or_assign(const Key& k, M&& obj) ------------------------------
	template <class M>
	std::pair<iterator, bool> insert_or_assign(const Key& key, M&& obj) {
		std::pair<iterator, bool> pair = emplace_key_and_args(key, std::forward<M>(obj));
		if (!pair.second) {
			pair.first.second =  std::forward<M>(obj);
		}
		return pair;
	}
	// ------------------------------ insert_or_assign(Key&& k, M&& obj) ------------------------------
	template <class M>
	std::pair<iterator, bool> insert_or_assign(Key&& key, M&& obj) {
		std::pair<iterator, bool> pair = emplace_key_and_args(std::move(key), std::forward<M>(obj));
		if (!pair.second) {
			pair.first.second =  std::forward<M>(obj);
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
		return emplace_key_in_args(std::forward(args)...);
	}
	// ------------------------------ emplace_hint(const_iterator, Args&&... args) ------------------------------
	template<class... Args>
	iterator emplace_hint(const_iterator, Args&&... args) {
		return emplace(std::forward(args)...).first;
	}
	// ------------------------------ try_emplace(const Key& k, Args&&... args) ------------------------------
	template<class... Args>
	std::pair<iterator, bool> try_emplace(const Key& key, Args&&... args) {
		return emplace_key_and_args(key, std::forward(args)...);
	}
	// ------------------------------ try_emplace(const_iterator, const Key& k, Args&&... args) ------------------------------
	template<class... Args>
	iterator try_emplace(const_iterator, const Key& key, Args&&... args) {
		return try_emplace(key, std::forward(args)...).first;
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
	// ------------------------------ operator[]( const Key& key ) ------------------------------
	T& operator[]( const Key& key ) {
		return insert({key, allocator_type{}()}).first.second;
	}
	// ------------------------------ operator[]( Key&& key ) ------------------------------
	T& operator[]( Key&& key ) {
		return insert({key, allocator_type{}()}).first.second;
	}
	// ------------------------------ find(const Key& key) ------------------------------
	iterator find(const Key& key) {
		std::size_t hash = hasher{}(key);
		for (std::size_t tierIndex = 0; tierIndex < tiers.size(); ++tierIndex) {
			std::size_t binIndex = hash % tiers[tierIndex].table.size();
			const Bin& bin = tiers[tierIndex].table[binIndex];
			for (std::size_t kvPairIndex = 0; kvPairIndex < bin.size; ++kvPairIndex) {
				if (key_equal{}(key, bin.kvPairs[kvPairIndex]->first)) {
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
			std::size_t binIndex = hash % tiers[tierIndex].table.size();
			const Bin& bin = tiers[tierIndex].table[binIndex];
			for (std::size_t kvPairIndex = 0; kvPairIndex < bin.size; ++kvPairIndex) {
				if (key_equal{}(key, bin.kvPairs[kvPairIndex]->first)) {
					return const_iterator(this, tierIndex, binIndex, kvPairIndex);
				}
			}
		}
		return cend();
	}

	void visualize() const {
		for (const Tier& tier : tiers) {
			for (const Bin& bin : tier.table) {
				std::cout << "[";
				bool first = true;
				for (std::size_t i = 0; i < bin.size; ++i) {
					if (!first) std::cout << ",";
					const std::optional<mut_value_type>& value = bin.kvPairs[i];
					std::cout << value.value().first << ":" << value.value().second;
					first = false;
				}
				std::cout << "]";
			}
			std::cout << "\n";
		}
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
				std::size_t binIndex = hash % tiers[tierIndex].table.size();
				const Bin& bin = tiers[tierIndex].table[binIndex];
				for (std::size_t kvPairIndex = 0; kvPairIndex < bin.size; ++kvPairIndex) {
					if (key_equal{}(key, bin.kvPairs[kvPairIndex]->first)) {
						return {iterator(this, tierIndex, binIndex, kvPairIndex), false};
					}
				}
			}
		}
		// add it otherwise
		++kvCount;
		Tier& tier = tiers[0];
		std::size_t binIndex = hash % tier.table.size();
		Bin& bin = tier[binIndex];
		if (bin.size < bin.kvPairs.size()) {
			bin.kvPairs[bin.size].emplace(key, std::forward(args)...);
			++tier.kvCount;
			return {iterator(this, 0, binIndex, bin.size++), true};
		}
#if (kickFirst)
		std::size_t kickIndex = 0;
#else
		std::size_t kickIndex = hash % bin.size;
#endif
		mut_value_type otherValue = std::move(bin.kvPairs[kickIndex].value());
		bin.kvPairs[kickIndex].reset();
		bin.kvPairs[kickIndex].emplace(key, std::forward(args)...);
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
			std::size_t hash = hasher{}(value.first);
			std::size_t binIndex = hash % tier.table.size();
			Bin& bin = tier.table[binIndex];
			bin.kvPairs[0] = std::move(value);
			return iterator(this, tiers.size()-1, binIndex, bin.size++);
		}
		Tier& tier = tiers[tierIndex];
		std::size_t hash = hasher{}(value.first);
		std::size_t binIndex = hash % tier.table.size();
		Bin& bin = tier.table[binIndex];
		if (bin.size < bin.kvPairs.size()) {
			bin.kvPairs[bin.size] = std::move(value);
			return iterator(this, tierIndex, binIndex, bin.size++);
		}
#if (kickFirst)
		std::size_t kickIndex = 0;
#else
		std::size_t kickIndex = hash % bin.size;
#endif
		mut_value_type otherValue = std::move(bin.kvPairs[kickIndex].value());
		bin.kvPairs[kickIndex] = std::move(value);
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
