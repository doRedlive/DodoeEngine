// do@Redlive

#pragma once

#include "dopch.h"

#include "mesh_processor_base.h"
#include "mesh_pass_command_storage.h"

namespace dodoe {

    class DescriptorTableManager;
    class BindingLayoutCache;
    class BindingSetCache;

    struct MeshPassDefinition {
        MeshPassType pass_type{MeshPassType::Opaque};
        Scope<MeshPassProcessor> processor{nullptr};
        MeshPassCommandStorage command_storage{};
    };

    class MeshPassRegistry {
        StaticArray<Scope<MeshPassDefinition>, static_cast<Size_t>(MeshPassType::Count)> m_definitions{};

    public:
        Bool initialize(DescriptorTableManager* descriptor_table,
                        BindingLayoutCache& binding_layout_cache,
                        BindingSetCache& binding_set_cache);
        void shutdown();

        template <typename T, typename... Args>
        T* registerPass(const MeshPassType pass_type, Args&&... args) {
            auto processor = create_scope<T>(std::forward<Args>(args)...);
            T* result = processor.get();
            auto definition = create_scope<MeshPassDefinition>();
            definition->pass_type = pass_type;
            definition->processor = std::move(processor);
            m_definitions[static_cast<Size_t>(pass_type)] = std::move(definition);
            return result;
        }

        [[nodiscard]] MeshPassProcessor* find(MeshPassType pass_type) const;
        [[nodiscard]] MeshPassDefinition* findDefinition(MeshPassType pass_type);
        [[nodiscard]] const MeshPassDefinition* findDefinition(MeshPassType pass_type) const;
        [[nodiscard]] MeshPassCommandStorage* getCommandStorage(MeshPassType pass_type);
        [[nodiscard]] const MeshPassCommandStorage* getCommandStorage(MeshPassType pass_type) const;
    };

} // namespace dodoe
