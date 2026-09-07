// do@Redlive

#include "cached_mesh_draw_command.h"

#include "mesh_batch.h"
#include "runtime/function/render/material/material_system.h"

namespace dodoe {

    namespace CacheHashUtils {

        namespace {

            Size_t ComputeBatchHash(const MeshBatchElement& element) {
                Size_t h = 0;
                h ^= reinterpret_cast<Size_t>(element.vertex_buffer.get());
                h ^= reinterpret_cast<Size_t>(element.index_buffer.get()) << 7;
                h ^= static_cast<Size_t>(element.index_count) << 13;
                h ^= static_cast<Size_t>(element.index_offset) << 17;
                h ^= static_cast<Size_t>(element.vertex_offset) << 23;
                h ^= static_cast<Size_t>(element.instance_count) << 29;
                return h;
            }

            Size_t ComputeMaterialHash(const MaterialInstance* mi) {
                return reinterpret_cast<Size_t>(mi);
            }

            Size_t ComputePassHash(const MeshPassType pass_type) {
                return static_cast<Size_t>(pass_type);
            }

        } // namespace

        MeshDrawCommandCacheKey MakeCacheKey(const MeshBatchElement& element,
                                             const MaterialInstance* mi,
                                             const MeshPassType pass_type,
                                             const GfxGraphicsPipelineHandle& pipeline) {
            return MeshDrawCommandCacheKey{
                .batch_hash    = ComputeBatchHash(element),
                .material_hash = ComputeMaterialHash(mi),
                .material_revision = mi ? mi->revision : 0,
                .pass_hash     = ComputePassHash(pass_type),
                .pipeline_hash = reinterpret_cast<Size_t>(pipeline.get()),
            };
        }

    } // namespace CacheHashUtils

    MeshDrawCommandCacheKey MeshDrawCommandCache::MakeLooseKey(const MeshDrawCommandCacheKey& key) {
        MeshDrawCommandCacheKey loose = key;
        loose.material_revision = 0;
        return loose;
    }

    MeshDrawCommandCache::~MeshDrawCommandCache() = default;

    UInt32 MeshDrawCommandCache::findOrCreate(const MeshDrawCommandCacheKey& key, MeshDrawCommand&& cmd) {
        const auto exact = m_key_to_index.find(key);
        if (exact != m_key_to_index.end()) {
            return exact->second;
        }

        const auto loose_key = MakeLooseKey(key);
        const auto loose = m_loose_key_to_index.find(loose_key);
        if (loose != m_loose_key_to_index.end()) {
            const UInt32 index = loose->second;
            m_key_to_index.erase(m_keys[index]);
            m_keys[index] = key;
            m_commands[index] = std::move(cmd);
            m_key_to_index[key] = index;
            return index;
        }

        const UInt32 index = static_cast<UInt32>(m_commands.size());
        m_commands.push_back(std::move(cmd));
        m_keys.push_back(key);
        m_key_to_index[key] = index;
        m_loose_key_to_index[loose_key] = index;
        return index;
    }

    void MeshDrawCommandCache::invalidate() {
        m_key_to_index.clear();
        m_loose_key_to_index.clear();
        m_keys.clear();
        m_commands.clear();
    }

} // dodoe
