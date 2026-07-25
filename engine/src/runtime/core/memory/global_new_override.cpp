// do@Redlive

#include "memory.h"

#include <mimalloc.h>

void* operator new(std::size_t n) {
    return dodoe::Memory::AllocatePersistent(n, alignof(std::max_align_t), dodoe::AllocTag::Misc);
}

void* operator new[](std::size_t n) {
    return dodoe::Memory::AllocatePersistent(n, alignof(std::max_align_t), dodoe::AllocTag::Misc);
}

void operator delete(void* p) noexcept {
    if (!p) return;
    std::size_t n = mi_malloc_size(p);
    dodoe::Memory::DeallocatePersistent(p, n, dodoe::AllocTag::Misc);
}

void operator delete[](void* p) noexcept {
    if (!p) return;
    std::size_t n = mi_malloc_size(p);
    dodoe::Memory::DeallocatePersistent(p, n, dodoe::AllocTag::Misc);
}

void operator delete(void* p, std::size_t n) noexcept {
    if (!p) return;
    dodoe::Memory::DeallocatePersistent(p, n, dodoe::AllocTag::Misc);
}

void operator delete[](void* p, std::size_t n) noexcept {
    if (!p) return;
    dodoe::Memory::DeallocatePersistent(p, n, dodoe::AllocTag::Misc);
}
