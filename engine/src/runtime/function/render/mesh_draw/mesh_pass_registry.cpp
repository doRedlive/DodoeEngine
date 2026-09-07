// do@Redlive

#include "mesh_pass_registry.h"

#include "lit_mesh_processor.h"
#include "shadow_mesh_processor.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_service/binding_set_cache.h"
#include "runtime/function/render/shader/descriptor_table_manager.h"

namespace dodoe {

    Bool MeshPassRegistry::initialize(DescriptorTableManager* descriptor_table,
                                      BindingLayoutCache& binding_layout_cache,
                                      BindingSetCache& binding_set_cache) {
        GfxBindingSetHandle descriptor_binding_set{};
        if (descriptor_table && descriptor_table->getDescriptorTable()) {
            descriptor_binding_set = create_ref<GfxBindingSet>(
                cutie::BindingSetHandle(descriptor_table->getDescriptorTable()));
        }

        const auto opaque = registerPass<LitMeshProcessor>(MeshPassType::Opaque,
            MeshPassType::Opaque, descriptor_binding_set, binding_layout_cache, binding_set_cache);
        const auto transparent = registerPass<LitMeshProcessor>(MeshPassType::Transparent,
            MeshPassType::Transparent, descriptor_binding_set, binding_layout_cache, binding_set_cache);
        const auto shadow = registerPass<ShadowMeshProcessor>(MeshPassType::Shadow,
            binding_layout_cache, binding_set_cache);
        return opaque && transparent && shadow;
    }

    void MeshPassRegistry::shutdown() {
        for (auto& definition : m_definitions) {
            if (definition) {
                definition->command_storage.reset();
                if (definition->processor) {
                    definition->processor->reset();
                    definition->processor.reset();
                }
                definition.reset();
            }
        }
    }

    MeshPassProcessor* MeshPassRegistry::find(const MeshPassType pass_type) const {
        const auto* definition = findDefinition(pass_type);
        return definition && definition->processor ? definition->processor.get() : nullptr;
    }

    MeshPassDefinition* MeshPassRegistry::findDefinition(const MeshPassType pass_type) {
        const auto index = static_cast<Size_t>(pass_type);
        return index < m_definitions.size() ? m_definitions[index].get() : nullptr;
    }

    const MeshPassDefinition* MeshPassRegistry::findDefinition(const MeshPassType pass_type) const {
        const auto index = static_cast<Size_t>(pass_type);
        return index < m_definitions.size() ? m_definitions[index].get() : nullptr;
    }

    MeshPassCommandStorage* MeshPassRegistry::getCommandStorage(const MeshPassType pass_type) {
        auto* definition = findDefinition(pass_type);
        return definition ? &definition->command_storage : nullptr;
    }

    const MeshPassCommandStorage* MeshPassRegistry::getCommandStorage(const MeshPassType pass_type) const {
        const auto* definition = findDefinition(pass_type);
        return definition ? &definition->command_storage : nullptr;
    }

} // namespace dodoe
