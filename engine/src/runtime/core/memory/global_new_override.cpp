// do@Redlive

#include "memory.h"

#include <mimalloc.h>

namespace dodoe {

    void* operator new(std::size_t n) {
        return Memory::AllocatePersistent(n, alignof(std::max_align_t), AllocTag::Misc);
    }

    void* operator new[](std::size_t n) {
        return Memory::AllocatePersistent(n, alignof(std::max_align_t), AllocTag::Misc);
    }

    void operator delete(void* p) noexcept {
        if (!p) return;
        std::size_t n = mi_malloc_size(p);
        Memory::DeallocatePersistent(p, n, AllocTag::Misc);
    }

    void operator delete[](void* p) noexcept {
        if (!p) return;
        std::size_t n = mi_malloc_size(p);
        Memory::DeallocatePersistent(p, n, AllocTag::Misc);
    }

    void operator delete(void* p, std::size_t n) noexcept {
        if (!p) return;
        Memory::DeallocatePersistent(p, n, AllocTag::Misc);
    }

    void operator delete[](void* p, std::size_t n) noexcept {
        if (!p) return;
        Memory::DeallocatePersistent(p, n, AllocTag::Misc);
    }

} // namespace dodoe
