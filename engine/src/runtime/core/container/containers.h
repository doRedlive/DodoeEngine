// do@Redlive

#pragma once

#include "runtime/core/memory/std_allocator.h"

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <spdlog/fmt/bundled/format.h>

namespace dodoe {

	using StringBase = std::basic_string<char, std::char_traits<char>, StdAllocator<char>>;
	using StringView = std::string_view;

	class String : public std::string {
	public:
		using std::string::operator=;

		String() = default;
		String(const String&) = default;
		String(String&&) noexcept = default;
		~String() = default;
		String& operator=(const String&) = default;
		String& operator=(String&&) noexcept = default;

		String(const char* s) : std::string(s) {}
		String(const char* s, Size_t n) : std::string(s, n) {}
		String(Size_t n, char c) : std::string(n, c) {}
		String(const std::string& s) : std::string(s) {}
		String(std::string&& s) noexcept : std::string(std::move(s)) {}
		String(std::string_view s) : std::string(s) {}

		template <typename InputIt>
		String(InputIt first, InputIt last) : std::string(first, last) {}

		operator std::filesystem::path() const { return std::filesystem::path(this->c_str()); }
	};

	inline std::string string_to_std(const String& s) { return std::string(s.data(), s.size()); }

	template <typename T>
	inline String ToString(const T& value) {
		const std::string text = std::to_string(value);
		return String(text.data(), text.size());
	}

	template <typename T>
	using Optional = std::optional<T>;

	template <typename T>
	using DynamicArray = std::vector<T, StdAllocator<T>>;

	template <typename T, size_t N>
	using StaticArray = std::array<T, N>;

	template <typename TKey, typename TValue, typename THash = std::hash<TKey>, typename TEqual = std::equal_to<TKey>>
	using UnorderedMap = std::unordered_map<TKey, TValue, THash, TEqual, StdAllocator<std::pair<const TKey, TValue>>>;

	template <typename TKey, typename TValue, typename TCompare = std::less<TKey>>
	using OrderedMap = std::map<TKey, TValue, TCompare, StdAllocator<std::pair<const TKey, TValue>>>;

	template <typename T, typename THash = std::hash<T>, typename TEqual = std::equal_to<T>>
	using UnorderedSet = std::unordered_set<T, THash, TEqual, StdAllocator<T>>;

	template <typename TKey, typename TValue>
	using Dictionary = UnorderedMap<TKey, TValue>;

	template <typename T1, typename T2>
	using Pair = std::pair<T1, T2>;

	template <typename T>
	using Deque = std::deque<T>;

} // namespace dodoe

namespace std {
    template <>
    struct hash<dodoe::String> {
        size_t operator()(const dodoe::String& s) const noexcept {
            return hash<dodoe::StringView>{}(dodoe::StringView(s.data(), s.size()));
        }
    };
} // namespace std

template <typename StringType>
struct DodoeStringFormatter {
    constexpr auto parse(fmt::format_parse_context& ctx) {
        auto it = ctx.begin();
        while (it != ctx.end() && *it != '}') ++it;
        return it;
    }
    template <typename FormatContext>
    auto format(const StringType& s, FormatContext& ctx) {
        return std::copy(s.begin(), s.end(), ctx.out());
    }
};

template <>
struct fmt::formatter<dodoe::String> : DodoeStringFormatter<dodoe::String> {};

template <>
struct fmt::formatter<dodoe::StringBase> : DodoeStringFormatter<dodoe::StringBase> {};
