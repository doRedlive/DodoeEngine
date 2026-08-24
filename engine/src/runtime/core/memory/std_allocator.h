// do@Redlive

#pragma once

#include "runtime/core/base.h"
#include "memory.h"
#include "thread_allocator.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dodoe {

	template <typename T>
	class StdAllocator {
	public:
		using value_type = T;
		using propagate_on_container_move_assignment = std::true_type;
		using is_always_equal = std::true_type;

		StdAllocator() = default;
		template <typename U> StdAllocator(const StdAllocator<U>&) {}

		[[nodiscard]] T* allocate(std::size_t n) {
			return static_cast<T*>(Memory::AllocatePersistent(n * sizeof(T), alignof(T), AllocTag::Misc));
		}

		void deallocate(T* p, std::size_t n) noexcept {
			Memory::DeallocatePersistent(p, n * sizeof(T), AllocTag::Misc);
		}
	};

	template <typename T, typename U>
	bool operator==(const StdAllocator<T>&, const StdAllocator<U>&) { return true; }

	template <typename T, typename U>
	bool operator!=(const StdAllocator<T>&, const StdAllocator<U>&) { return false; }

	template <typename T>
	class FrameAllocator {
	public:
		using value_type = T;
		using propagate_on_container_move_assignment = std::true_type;
		using is_always_equal = std::true_type;

		FrameAllocator() = default;
		template <typename U> FrameAllocator(const FrameAllocator<U>&) {}

		[[nodiscard]] T* allocate(std::size_t n) {
			return static_cast<T*>(Memory::AllocateFrame(n * sizeof(T), alignof(T), AllocTag::Misc));
		}

		void deallocate(T*, std::size_t) noexcept {}
	};

	template <typename T, typename U>
	bool operator==(const FrameAllocator<T>&, const FrameAllocator<U>&) { return true; }

	template <typename T, typename U>
	bool operator!=(const FrameAllocator<T>&, const FrameAllocator<U>&) { return false; }

	template <typename T>
	class ScratchAllocator {
		LinearAllocator* m_alloc;

	public:
		using value_type = T;

		ScratchAllocator() : m_alloc(&threadAllocator().scratch) {}
		template <typename U> explicit ScratchAllocator(const ScratchAllocator<U>& other)
			: m_alloc(other.allocator()) {}

		[[nodiscard]] LinearAllocator* allocator() const { return m_alloc; }

		[[nodiscard]] T* allocate(std::size_t n) {
			return static_cast<T*>(m_alloc->allocate(n * sizeof(T), alignof(T)));
		}

		void deallocate(T*, std::size_t) noexcept {}
	};

	template <typename T, typename U>
	bool operator==(const ScratchAllocator<T>& a, const ScratchAllocator<U>& b) {
		return a.allocator() == b.allocator();
	}

	template <typename T, typename U>
	bool operator!=(const ScratchAllocator<T>& a, const ScratchAllocator<U>& b) {
		return !(a == b);
	}

	template <typename T>
	using PersistentArray = std::vector<T, StdAllocator<T>>;

	template <typename K, typename V, typename H = std::hash<K>, typename E = std::equal_to<K>>
	using PersistentMap = std::unordered_map<K, V, H, E, StdAllocator<std::pair<const K, V>>>;

	template <typename T, typename H = std::hash<T>, typename E = std::equal_to<T>>
	using PersistentSet = std::unordered_set<T, H, E, StdAllocator<T>>;

	using PersistentString = std::basic_string<char, std::char_traits<char>, StdAllocator<char>>;

	template <typename T>
	using FrameArray = std::vector<T, FrameAllocator<T>>;

	using FrameString = std::basic_string<char, std::char_traits<char>, FrameAllocator<char>>;

	template <typename T>
	using ScratchArray = std::vector<T, ScratchAllocator<T>>;

} // namespace dodoe
