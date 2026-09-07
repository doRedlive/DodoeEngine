// do@Redlive

#pragma once

#include "dopch.h"

#include "mesh_draw_command.h"

namespace dodoe {

    struct MaterialInstance;
    struct MeshBatchElement;

    namespace CacheHashUtils {

        [[nodiscard]] MeshDrawCommandCacheKey MakeCacheKey(const MeshBatchElement& element,
                                                           const MaterialInstance* mi,
                                                           MeshPassType pass_type,
                                                           const GfxGraphicsPipelineHandle& pipeline);

    } // namespace CacheHashUtils

    class MeshDrawCommandCache {
        UnorderedMap<MeshDrawCommandCacheKey, UInt32> m_key_to_index;
        UnorderedMap<MeshDrawCommandCacheKey, UInt32> m_loose_key_to_index;
        DynamicArray<MeshDrawCommandCacheKey> m_keys;
        DynamicArray<MeshDrawCommand> m_commands;

        [[nodiscard]] static MeshDrawCommandCacheKey MakeLooseKey(const MeshDrawCommandCacheKey& key);

    public:
        MeshDrawCommandCache() = default;
        ~MeshDrawCommandCache();

        [[nodiscard]] UInt32 findOrCreate(const MeshDrawCommandCacheKey& key, MeshDrawCommand&& cmd);
        void invalidate();

        [[nodiscard]] const MeshDrawCommand& getCommand(const UInt32 index) const {
            return m_commands[index];
        }

        [[nodiscard]] const DynamicArray<MeshDrawCommand>& getCommands() const {
            return m_commands;
        }

        [[nodiscard]] Bool isEmpty() const {
            return m_commands.empty();
        }

        [[nodiscard]] Size_t size() const { return m_commands.size(); }
    };

} // dodoe
