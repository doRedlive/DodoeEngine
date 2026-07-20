// do@Redlive

#pragma once

#include "dopch.h"

#include "mesh_draw_command.h"
#include "mesh_batch.h"
#include "../render_scene/primitive_scene_info.h"
#include "../material/material.h"

namespace dodoe {

    namespace CacheHashUtils {

        inline Size_t ComputeBatchHash(const MeshBatchElement& element) {
            Size_t h = 0;
            h ^= reinterpret_cast<Size_t>(element.vertex_buffer.get());
            h ^= reinterpret_cast<Size_t>(element.index_buffer.get()) << 7;
            h ^= static_cast<Size_t>(element.index_count) << 13;
            h ^= static_cast<Size_t>(element.index_offset) << 17;
            h ^= static_cast<Size_t>(element.vertex_offset) << 23;
            h ^= static_cast<Size_t>(element.instance_count) << 29;
            return h;
        }

        inline Size_t ComputeMaterialHash(const Ref<Material>& material,
                                          const UInt32 base_color_tex_idx,
                                          const UInt32 metallic_roughness_tex_idx) {
            Size_t h = 0;
            h ^= reinterpret_cast<Size_t>(material.get());
            h ^= static_cast<Size_t>(base_color_tex_idx) << 7;
            h ^= static_cast<Size_t>(metallic_roughness_tex_idx) << 13;
            return h;
        }

        inline Size_t ComputePassHash(const MeshPassType pass_type) {
            return static_cast<Size_t>(pass_type);
        }

        inline MeshDrawCommandCacheKey MakeCacheKey(const MeshBatchElement& element,
                                                     const Ref<Material>& material,
                                                     const UInt32 base_color_tex_idx,
                                                     const UInt32 metallic_roughness_tex_idx,
                                                     const MeshPassType pass_type) {
            return MeshDrawCommandCacheKey{
                .batch_hash    = ComputeBatchHash(element),
                .material_hash = ComputeMaterialHash(material, base_color_tex_idx, metallic_roughness_tex_idx),
                .pass_hash     = ComputePassHash(pass_type),
            };
        }

    } // namespace CacheHashUtils

    class MeshDrawCommandCache {
        UnorderedMap<MeshDrawCommandCacheKey, UInt32> m_key_to_index;
        DynamicArray<MeshDrawCommand> m_commands;

    public:
        MeshDrawCommandCache() = default;

        [[nodiscard]] UInt32 findOrCreate(const MeshDrawCommandCacheKey& key, MeshDrawCommand&& cmd) {
            const auto it = m_key_to_index.find(key);
            if (it != m_key_to_index.end()) {
                return it->second;
            }
            const UInt32 index = static_cast<UInt32>(m_commands.size());
            m_commands.push_back(std::move(cmd));
            m_key_to_index[key] = index;
            return index;
        }

        void invalidate() {
            m_key_to_index.clear();
            m_commands.clear();
        }

        [[nodiscard]] const MeshDrawCommand& getCommand(const UInt32 index) const {
            return m_commands[index];
        }

        [[nodiscard]] const DynamicArray<MeshDrawCommand>& getCommands() const {
            return m_commands;
        }

        [[nodiscard]] Bool isEmpty() const {
            return m_commands.empty();
        }
    };

} // dodoe
