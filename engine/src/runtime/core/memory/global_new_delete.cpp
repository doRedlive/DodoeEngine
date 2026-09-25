// do@Redlive

#include "runtime/core/memory/memory.h"

#include <cstddef>
#include <cstdlib>
#include <malloc.h>
#include <new>

#if DODOE_MEMORY_GLOBAL_NEW_DELETE

void* operator new(std::size_t size) {
    void* p = dodoe::Memory::AllocatePersistent(static_cast<dodoe::Size_t>(size), alignof(std::max_align_t), dodoe::AllocTag::Misc);
    if (!p) {
        throw std::bad_alloc();
    }
    return p;
}

void* operator new[](std::size_t size) {
    void* p = dodoe::Memory::AllocatePersistent(static_cast<dodoe::Size_t>(size), alignof(std::max_align_t), dodoe::AllocTag::Misc);
    if (!p) {
        throw std::bad_alloc();
    }
    return p;
}

void* operator new(std::size_t size, std::align_val_t align) {
    void* p = dodoe::Memory::AllocatePersistent(static_cast<dodoe::Size_t>(size), static_cast<dodoe::Size_t>(align), dodoe::AllocTag::Misc);
    if (!p) {
        throw std::bad_alloc();
    }
    return p;
}

void* operator new[](std::size_t size, std::align_val_t align) {
    void* p = dodoe::Memory::AllocatePersistent(static_cast<dodoe::Size_t>(size), static_cast<dodoe::Size_t>(align), dodoe::AllocTag::Misc);
    if (!p) {
        throw std::bad_alloc();
    }
    return p;
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    return dodoe::Memory::AllocatePersistent(static_cast<dodoe::Size_t>(size), alignof(std::max_align_t), dodoe::AllocTag::Misc);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return dodoe::Memory::AllocatePersistent(static_cast<dodoe::Size_t>(size), alignof(std::max_align_t), dodoe::AllocTag::Misc);
}

void* operator new(std::size_t size, std::align_val_t align, const std::nothrow_t&) noexcept {
    return dodoe::Memory::AllocatePersistent(static_cast<dodoe::Size_t>(size), static_cast<dodoe::Size_t>(align), dodoe::AllocTag::Misc);
}

void* operator new[](std::size_t size, std::align_val_t align, const std::nothrow_t&) noexcept {
    return dodoe::Memory::AllocatePersistent(static_cast<dodoe::Size_t>(size), static_cast<dodoe::Size_t>(align), dodoe::AllocTag::Misc);
}

void operator delete(void* p) noexcept {
    if (!p) return;
    dodoe::Memory::DeallocatePersistent(p, dodoe::Memory::UsableSize(p), dodoe::AllocTag::Misc);
}

void operator delete(void* p, std::size_t size) noexcept {
    if (!p) return;
    dodoe::Memory::DeallocatePersistent(p, static_cast<dodoe::Size_t>(size), dodoe::AllocTag::Misc);
}

void operator delete(void* p, std::align_val_t) noexcept {
    if (!p) return;
    dodoe::Memory::DeallocatePersistent(p, dodoe::Memory::UsableSize(p), dodoe::AllocTag::Misc);
}

void operator delete(void* p, std::size_t size, std::align_val_t) noexcept {
    if (!p) return;
    dodoe::Memory::DeallocatePersistent(p, static_cast<dodoe::Size_t>(size), dodoe::AllocTag::Misc);
}

void operator delete[](void* p) noexcept {
    if (!p) return;
    dodoe::Memory::DeallocatePersistent(p, dodoe::Memory::UsableSize(p), dodoe::AllocTag::Misc);
}

void operator delete[](void* p, std::size_t size) noexcept {
    if (!p) return;
    dodoe::Memory::DeallocatePersistent(p, static_cast<dodoe::Size_t>(size), dodoe::AllocTag::Misc);
}

void operator delete[](void* p, std::align_val_t) noexcept {
    if (!p) return;
    dodoe::Memory::DeallocatePersistent(p, dodoe::Memory::UsableSize(p), dodoe::AllocTag::Misc);
}

void operator delete[](void* p, std::size_t size, std::align_val_t) noexcept {
    if (!p) return;
    dodoe::Memory::DeallocatePersistent(p, static_cast<dodoe::Size_t>(size), dodoe::AllocTag::Misc);
}

void operator delete(void* p, const std::nothrow_t&) noexcept {
    if (!p) return;
    dodoe::Memory::DeallocatePersistent(p, dodoe::Memory::UsableSize(p), dodoe::AllocTag::Misc);
}

void operator delete[](void* p, const std::nothrow_t&) noexcept {
    if (!p) return;
    dodoe::Memory::DeallocatePersistent(p, dodoe::Memory::UsableSize(p), dodoe::AllocTag::Misc);
}

void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept {
    if (!p) return;
    dodoe::Memory::DeallocatePersistent(p, dodoe::Memory::UsableSize(p), dodoe::AllocTag::Misc);
}

void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept {
    if (!p) return;
    dodoe::Memory::DeallocatePersistent(p, dodoe::Memory::UsableSize(p), dodoe::AllocTag::Misc);
}

#else

void* operator new(std::size_t size) {
    void* p = std::malloc(size);
    if (!p) {
        throw std::bad_alloc();
    }
    return p;
}

void* operator new[](std::size_t size) {
    void* p = std::malloc(size);
    if (!p) {
        throw std::bad_alloc();
    }
    return p;
}

void* operator new(std::size_t size, std::align_val_t align) {
    void* p = _aligned_malloc(size, static_cast<std::size_t>(align));
    if (!p) {
        throw std::bad_alloc();
    }
    return p;
}

void* operator new[](std::size_t size, std::align_val_t align) {
    void* p = _aligned_malloc(size, static_cast<std::size_t>(align));
    if (!p) {
        throw std::bad_alloc();
    }
    return p;
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    return std::malloc(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return std::malloc(size);
}

void* operator new(std::size_t size, std::align_val_t align, const std::nothrow_t&) noexcept {
    return _aligned_malloc(size, static_cast<std::size_t>(align));
}

void* operator new[](std::size_t size, std::align_val_t align, const std::nothrow_t&) noexcept {
    return _aligned_malloc(size, static_cast<std::size_t>(align));
}

void operator delete(void* p) noexcept {
    std::free(p);
}

void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}

void operator delete(void* p, std::align_val_t) noexcept {
    _aligned_free(p);
}

void operator delete(void* p, std::size_t, std::align_val_t) noexcept {
    _aligned_free(p);
}

void operator delete[](void* p) noexcept {
    std::free(p);
}

void operator delete[](void* p, std::size_t) noexcept {
    std::free(p);
}

void operator delete[](void* p, std::align_val_t) noexcept {
    _aligned_free(p);
}

void operator delete[](void* p, std::size_t, std::align_val_t) noexcept {
    _aligned_free(p);
}

void operator delete(void* p, const std::nothrow_t&) noexcept {
    std::free(p);
}

void operator delete[](void* p, const std::nothrow_t&) noexcept {
    std::free(p);
}

void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept {
    _aligned_free(p);
}

void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept {
    _aligned_free(p);
}

#endif
