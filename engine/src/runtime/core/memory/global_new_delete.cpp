// do@Redlive

#include "runtime/core/memory/memory.h"

#include <cstddef>
#include <cstdlib>
#include <new>

namespace {

bool IsEngineHeapPointer(const void* p) noexcept {
    return dodoe::Memory::IsPersistentHeap(p);
}

std::size_t EngineHeapSize(const void* p) noexcept {
    return dodoe::Memory::UsableSize(p);
}

} // namespace

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
    if (!IsEngineHeapPointer(p)) {
        std::free(p);
        return;
    }
    dodoe::Memory::DeallocatePersistent(p, EngineHeapSize(p), dodoe::AllocTag::Misc);
}

void operator delete(void* p, std::size_t size) noexcept {
    if (!p) return;
    if (!IsEngineHeapPointer(p)) {
        std::free(p);
        return;
    }
    dodoe::Memory::DeallocatePersistent(p, static_cast<dodoe::Size_t>(size), dodoe::AllocTag::Misc);
}

void operator delete(void* p, std::align_val_t) noexcept {
    if (!p) return;
    if (!IsEngineHeapPointer(p)) {
        std::free(p);
        return;
    }
    dodoe::Memory::DeallocatePersistent(p, EngineHeapSize(p), dodoe::AllocTag::Misc);
}

void operator delete(void* p, std::size_t size, std::align_val_t) noexcept {
    if (!p) return;
    if (!IsEngineHeapPointer(p)) {
        std::free(p);
        return;
    }
    dodoe::Memory::DeallocatePersistent(p, static_cast<dodoe::Size_t>(size), dodoe::AllocTag::Misc);
}

void operator delete[](void* p) noexcept {
    if (!p) return;
    if (!IsEngineHeapPointer(p)) {
        std::free(p);
        return;
    }
    dodoe::Memory::DeallocatePersistent(p, EngineHeapSize(p), dodoe::AllocTag::Misc);
}

void operator delete[](void* p, std::size_t size) noexcept {
    if (!p) return;
    if (!IsEngineHeapPointer(p)) {
        std::free(p);
        return;
    }
    dodoe::Memory::DeallocatePersistent(p, static_cast<dodoe::Size_t>(size), dodoe::AllocTag::Misc);
}

void operator delete[](void* p, std::align_val_t) noexcept {
    if (!p) return;
    if (!IsEngineHeapPointer(p)) {
        std::free(p);
        return;
    }
    dodoe::Memory::DeallocatePersistent(p, EngineHeapSize(p), dodoe::AllocTag::Misc);
}

void operator delete[](void* p, std::size_t size, std::align_val_t) noexcept {
    if (!p) return;
    if (!IsEngineHeapPointer(p)) {
        std::free(p);
        return;
    }
    dodoe::Memory::DeallocatePersistent(p, static_cast<dodoe::Size_t>(size), dodoe::AllocTag::Misc);
}

void operator delete(void* p, const std::nothrow_t&) noexcept {
    if (!p) return;
    if (!IsEngineHeapPointer(p)) {
        std::free(p);
        return;
    }
    dodoe::Memory::DeallocatePersistent(p, EngineHeapSize(p), dodoe::AllocTag::Misc);
}

void operator delete[](void* p, const std::nothrow_t&) noexcept {
    if (!p) return;
    if (!IsEngineHeapPointer(p)) {
        std::free(p);
        return;
    }
    dodoe::Memory::DeallocatePersistent(p, EngineHeapSize(p), dodoe::AllocTag::Misc);
}

void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept {
    if (!p) return;
    if (!IsEngineHeapPointer(p)) {
        std::free(p);
        return;
    }
    dodoe::Memory::DeallocatePersistent(p, EngineHeapSize(p), dodoe::AllocTag::Misc);
}

void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept {
    if (!p) return;
    if (!IsEngineHeapPointer(p)) {
        std::free(p);
        return;
    }
    dodoe::Memory::DeallocatePersistent(p, EngineHeapSize(p), dodoe::AllocTag::Misc);
}
