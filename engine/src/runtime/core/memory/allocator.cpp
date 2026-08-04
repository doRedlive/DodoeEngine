// do@Redlive

#include "allocator.h"

#include <iterator>
#include <mimalloc.h>

namespace dodoe {

    MallocAllocator& MallocAllocator::instance() {
        static MallocAllocator s_instance;
        return s_instance;
    }

    void* MallocAllocator::allocate(Size_t size, Size_t align) {
        if (align > alignof(std::max_align_t)) {
            return mi_aligned_alloc(align, size);
        }
        return mi_malloc(size);
    }

    void MallocAllocator::deallocate(void* p, Size_t size) {
        (void)size;
        mi_free(p);
    }

    LinearAllocator::LinearAllocator(LinearAllocator&& other) noexcept {
        std::lock_guard<std::recursive_mutex> lock(other.m_mutex);
        m_blocks = std::move(other.m_blocks);
        m_default_block_size = other.m_default_block_size;
        m_used_byte_size = other.m_used_byte_size;
        other.m_default_block_size = kDefaultBlockSize;
        other.m_used_byte_size = 0;
    }

    LinearAllocator& LinearAllocator::operator=(LinearAllocator&& other) noexcept {
        if (this != &other) {
            std::lock_guard<std::recursive_mutex> lock_this(m_mutex);
            std::lock_guard<std::recursive_mutex> lock_other(other.m_mutex);
            m_blocks = std::move(other.m_blocks);
            m_default_block_size = other.m_default_block_size;
            m_used_byte_size = other.m_used_byte_size;
            other.m_default_block_size = kDefaultBlockSize;
            other.m_used_byte_size = 0;
        }
        return *this;
    }

    void* LinearAllocator::allocate(Size_t size, Size_t align) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        if (m_blocks.empty()) {
            createBlock(size + align);
        }

        Block* block = &m_blocks.back();
        Size_t aligned_offset = alignUp(block->offset, align);

        if (aligned_offset + size > block->size) {
            createBlock(size + align);
            block = &m_blocks.back();
            aligned_offset = alignUp(block->offset, align);
        }

        void* memory = block->data + aligned_offset;
        block->offset = aligned_offset + size;
        m_used_byte_size += size;
        return memory;
    }

    void LinearAllocator::deallocate(void* p, Size_t size) {
        (void)p;
        (void)size;
    }

    Bool LinearAllocator::owns(const void* p) const {
        if (!p) return false;
        std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(m_mutex));
        for (const auto& block : m_blocks) {
            if (p >= block.data && p < block.data + block.size) {
                return true;
            }
        }
        return false;
    }

    void LinearAllocator::reset() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (auto& block : m_blocks) {
            block.offset = 0;
        }
        m_used_byte_size = 0;
    }

    void LinearAllocator::resetTo(Size_t byte_offset) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_blocks.empty()) return;
        Block* block = &m_blocks.back();
        block->offset = byte_offset;
        m_used_byte_size = byte_offset;
    }

    void LinearAllocator::release() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_blocks.clear();
        m_used_byte_size = 0;
    }

    void LinearAllocator::reserve(Size_t byte_size) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_blocks.empty() || m_blocks.back().offset + byte_size > m_blocks.back().size) {
            createBlock(byte_size);
        }
    }

    void LinearAllocator::transferFrom(LinearAllocator&& other) {
        if (this == &other) {
            return;
        }
        std::lock_guard<std::recursive_mutex> lock_this(m_mutex);
        std::lock_guard<std::recursive_mutex> lock_other(other.m_mutex);

        if (m_blocks.empty()) {
            m_blocks = std::move(other.m_blocks);
        } else {
            m_blocks.insert(m_blocks.end(),
                            std::make_move_iterator(other.m_blocks.begin()),
                            std::make_move_iterator(other.m_blocks.end()));
            other.m_blocks.clear();
        }
        m_used_byte_size += other.m_used_byte_size;
        other.m_used_byte_size = 0;
    }

    void LinearAllocator::createBlock(Size_t minimum_size) {
        Size_t block_size = std::max(m_default_block_size, minimum_size);

        Block block(block_size);
        m_blocks.push_back(std::move(block));
    }

    Size_t LinearAllocator::alignUp(Size_t value, Size_t alignment) {
        DO_ASSERT(alignment != 0, "LinearAllocator: alignment is zero");
        DO_ASSERT((alignment & (alignment - 1)) == 0, "LinearAllocator: alignment must be power of two");
        Size_t mask = alignment - 1;
        return (value + mask) & ~mask;
    }

    PoolAllocator::PoolAllocator(Size_t block_size, Size_t block_align)
        : m_block_size(block_size)
        , m_block_align(block_align) {
        if (m_block_size < sizeof(FreeNode)) {
            m_block_size = sizeof(FreeNode);
        }
        refill();
    }

    PoolAllocator::~PoolAllocator() {
        for (auto* chunk : m_chunks) {
            delete[] chunk;
        }
        m_chunks.clear();
        m_free_list = nullptr;
    }

    PoolAllocator::PoolAllocator(PoolAllocator&& other) noexcept {
        std::lock_guard<std::mutex> lock(other.m_mutex);
        m_block_size = other.m_block_size;
        m_block_align = other.m_block_align;
        m_free_list = other.m_free_list;
        m_chunks = std::move(other.m_chunks);
        m_chunk_size = other.m_chunk_size;
        other.m_free_list = nullptr;
        other.m_block_size = 0;
        other.m_block_align = 0;
    }

    PoolAllocator& PoolAllocator::operator=(PoolAllocator&& other) noexcept {
        if (this != &other) {
            std::lock_guard<std::mutex> lock_this(m_mutex);
            std::lock_guard<std::mutex> lock_other(other.m_mutex);
            for (auto* chunk : m_chunks) { delete[] chunk; }
            m_chunks.clear();
            m_block_size = other.m_block_size;
            m_block_align = other.m_block_align;
            m_free_list = other.m_free_list;
            m_chunks = std::move(other.m_chunks);
            m_chunk_size = other.m_chunk_size;
            other.m_free_list = nullptr;
            other.m_block_size = 0;
            other.m_block_align = 0;
        }
        return *this;
    }

    void* PoolAllocator::allocate(Size_t size, Size_t align) {
        (void)align;
        if (size > m_block_size) {
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_free_list) {
            refill();
            if (!m_free_list) {
                return nullptr;
            }
        }

        FreeNode* node = m_free_list;
        m_free_list = node->next;
        return static_cast<void*>(node);
    }

    void PoolAllocator::deallocate(void* p, Size_t size) {
        (void)size;
        if (!p) return;

        std::lock_guard<std::mutex> lock(m_mutex);

        FreeNode* node = static_cast<FreeNode*>(p);
        node->next = m_free_list;
        m_free_list = node;
    }

    Bool PoolAllocator::owns(const void* p) const {
        if (!p) return false;
        for (const auto* chunk : m_chunks) {
            if (p >= chunk && p < chunk + m_chunk_size) {
                return true;
            }
        }
        return false;
    }

    void PoolAllocator::refill() {
        const Size_t node_size = std::max(m_block_size, sizeof(FreeNode));
        const Size_t nodes_per_chunk = m_chunk_size / node_size;
        if (nodes_per_chunk == 0) return;

        auto* chunk = new UInt8[m_chunk_size];
        m_chunks.push_back(chunk);

        char* base = reinterpret_cast<char*>(chunk);
        for (Size_t i = 0; i < nodes_per_chunk - 1; ++i) {
            auto* node = reinterpret_cast<FreeNode*>(base + i * node_size);
            node->next = reinterpret_cast<FreeNode*>(base + (i + 1) * node_size);
        }
        auto* last = reinterpret_cast<FreeNode*>(base + (nodes_per_chunk - 1) * node_size);
        last->next = m_free_list;
        m_free_list = reinterpret_cast<FreeNode*>(chunk);
    }

} // namespace dodoe
