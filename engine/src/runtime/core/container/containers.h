// do@Redlive

#pragma once

#include "runtime/core/memory/std_allocator.h"

#include <spdlog/fmt/bundled/format.h>

namespace dodoe {

	using String = std::basic_string<char, std::char_traits<char>, StdAllocator<char>>;
	using StringView = std::string_view;

	inline std::string string_to_std(const String& s) { return std::string(s.data(), s.size()); }

	template <typename T>
	using Optional = std::optional<T>;

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

template <>
struct fmt::formatter<dodoe::String> {
    constexpr auto parse(fmt::format_parse_context& ctx) {
        auto it = ctx.begin();
        while (it != ctx.end() && *it != '}') ++it;
        return it;
    }
    template <typename FormatContext>
    auto format(const dodoe::String& s, FormatContext& ctx) {
        return std::copy(s.begin(), s.end(), ctx.out());
    }
};
