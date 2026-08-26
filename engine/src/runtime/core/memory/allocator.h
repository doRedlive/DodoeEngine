// do@Redlive

#pragma once

#include "runtime/core/base.h"

#include <cstdlib>
#include <mimalloc.h>
#include <mutex>
#include <new>

namespace dodoe {

    class IAllocator {
    public:
        virtual ~IAllocator() = default;
        virtual void* allocate(Size_t size, Size_t align) = 0;
        virtual void  deallocate(void* p, Size_t size) = 0;
        virtual Bool  owns(const void* p) const = 0;
        [[nodiscard]] virtual const char* name() const = 0;
    };

    class MallocAllocator : public IAllocator {
    public:
        static MallocAllocator& instance();

        void* allocate(Size_t size, Size_t align) override;
        void  deallocate(void* p, Size_t size) override;
        Bool  owns(const void* p) const override { return true; }
        [[nodiscard]] const char* name() const override { return "Malloc"; }

    private:
        friend class Memory;
        MallocAllocator() = default;
    };

    class LinearAllocator : public IAllocator {
        struct Block {
            UInt8* data{nullptr};
            Size_t size{0};
            Size_t offset{0};

            Block() = default;
            explicit Block(Size_t sz) : data(static_cast<UInt8*>(mi_malloc(sz))), size(sz), offset(0) {}
            ~Block() { mi_free(data); data = nullptr; }
            Block(Block&& other) noexcept : data(other.data), size(other.size), offset(other.offset) {
                other.data = nullptr; other.size = 0; other.offset = 0;
            }
            Block& operator=(Block&& other) noexcept {
                if (this != &other) { mi_free(data); data = other.data; size = other.size; offset = other.offset; other.data = nullptr; other.size = 0; other.offset = 0; }
                return *this;
            }
            Block(const Block&) = delete;
            Block& operator=(const Block&) = delete;
        };

        std::vector<Block> m_blocks{};
        Size_t m_default_block_size{4096};
        Size_t m_used_byte_size{0};
        std::recursive_mutex m_mutex{};

    public:
        inline static constexpr Size_t kDefaultBlockSize = 4096;

        LinearAllocator() = default;
        explicit LinearAllocator(Size_t default_block_size) : m_default_block_size(default_block_size) {}
        ~LinearAllocator() override { release(); }

        LinearAllocator(const LinearAllocator&) = delete;
        LinearAllocator& operator=(const LinearAllocator&) = delete;
        LinearAllocator(LinearAllocator&& other) noexcept;
        LinearAllocator& operator=(LinearAllocator&& other) noexcept;

        void* allocate(Size_t size, Size_t align) override;
        void  deallocate(void* p, Size_t size) override;
        Bool  owns(const void* p) const override;
        [[nodiscard]] const char* name() const override { return "Linear"; }

        void reset();
        void resetTo(Size_t byte_offset);
        void release();
        void reserve(Size_t byte_size);
        void transferFrom(LinearAllocator&& other);

        [[nodiscard]] Size_t usedByteSize() const { return m_used_byte_size; }
        [[nodiscard]] Size_t blockCount() const { return m_blocks.size(); }
        [[nodiscard]] Size_t defaultBlockSize() const { return m_default_block_size; }

    private:
        void createBlock(Size_t minimum_size);
        static Size_t alignUp(Size_t value, Size_t alignment);
    };

    class PoolAllocator : public IAllocator {
        struct FreeNode {
            FreeNode* next{nullptr};
        };

        Size_t m_block_size{0};
        Size_t m_block_align{0};
        FreeNode* m_free_list{nullptr};
        std::vector<UInt8*> m_chunks{};
        Size_t m_chunk_size{65536};
        std::mutex m_mutex{};

    public:
        PoolAllocator() = default;
        PoolAllocator(Size_t block_size, Size_t block_align);
        ~PoolAllocator() override;

        PoolAllocator(const PoolAllocator&) = delete;
        PoolAllocator& operator=(const PoolAllocator&) = delete;
        PoolAllocator(PoolAllocator&& other) noexcept;
        PoolAllocator& operator=(PoolAllocator&& other) noexcept;

        void* allocate(Size_t size, Size_t align) override;
        void  deallocate(void* p, Size_t size) override;
        Bool  owns(const void* p) const override;
        [[nodiscard]] const char* name() const override { return "Pool"; }

        [[nodiscard]] Size_t blockSize() const { return m_block_size; }

    private:
        void refill();
    };

} // namespace dodoe
