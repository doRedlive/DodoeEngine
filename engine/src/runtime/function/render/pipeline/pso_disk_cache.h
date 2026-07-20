// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    class PsoDiskCache {
    public:
        static constexpr UInt32 kMagic = 0x50534F43;
        static constexpr UInt32 kVersion = 1;

        struct EntryHeader {
            UInt64 key_hash;
            UInt64 data_size;
        };

        Bool beginLoad(const String& cache_path, UInt64 expected_shader_hash);
        Bool isLoaded() const { return m_loaded; }

        DynamicArray<UInt8>* find(UInt64 key_hash);

        void insert(UInt64 key_hash, const DynamicArray<UInt8>& pso_blob);
        void flushToDisk();

        void invalidateAll();

    private:
        String m_cache_path;
        UnorderedMap<UInt64, DynamicArray<UInt8>> m_cache_data;
        DynamicArray<Pair<UInt64, DynamicArray<UInt8>>> m_pending_writes;
        UInt64 m_shader_hash{0};
        Bool m_loaded{false};
    };

} // namespace dodoe
