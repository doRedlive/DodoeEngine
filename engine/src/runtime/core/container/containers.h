// do@Redlive

#pragma once

#include "runtime/core/memory/std_allocator.h"

namespace dodoe {

	using String = std::basic_string<char, std::char_traits<char>, StdAllocator<char>>;
	using StringView = std::string_view;

	template <typename T>
	using DynamicArray = std::vector<T, StdAllocator<T>>;

	template <typename T, size_t N>
	using StaticArray = std::array<T, N>;

	template <typename TKey, typename TValue, typename THash = std::hash<TKey>, typename TEqual = std::equal_to<TKey>>
	using UnorderedMap = std::unordered_map<TKey, TValue, THash, TEqual, StdAllocator<std::pair<const TKey, TValue>>>;

	template <typename T, typename THash = std::hash<T>, typename TEqual = std::equal_to<T>>
	using UnorderedSet = std::unordered_set<T, THash, TEqual, StdAllocator<T>>;

	template <typename TKey, typename TValue>
	using Dictionary = UnorderedMap<TKey, TValue>;

	template <typename T1, typename T2>
	using Pair = std::pair<T1, T2>;

} // namespace dodoe
