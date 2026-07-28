#include "material_system.h"

#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_service/binding_set_cache.h"
#include "runtime/function/render/texture/texture_manager.h"
#include "runtime/function/render/texture/texture.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/core/math/math.h"

namespace dodoe {

    Size_t MaterialTemplateDesc::computeHash() const {
        Size_t h = 0;
        hash_combine(h, name);
        hash_combine(h, shader_name);
        for (const auto& def : param_defs) {
            hash_combine(h, def.name);
            hash_combine(h, static_cast<UInt8>(def.type));
        }
        for (const auto& [key, val] : permutation_defaults) {
            hash_combine(h, key);
            hash_combine(h, val);
        }
        return h;
    }

    Bool MaterialSystem::initialize(const MaterialSystemCreateInfo& info) {
        m_shader_library = info.shader_library;
        m_binding_layout_cache = info.binding_layout_cache;
        m_binding_set_cache = info.binding_set_cache;
        m_texture_manager = info.texture_manager;
        if (!m_shader_library || !m_binding_layout_cache || !m_binding_set_cache || !m_texture_manager) {
            return false;
        }
        registerBuiltinTemplates();
        return true;
    }

    void MaterialSystem::registerBuiltinTemplates() {
        MaterialTemplateDesc gbuffer;
        gbuffer.name = "GBuffer";
        gbuffer.shader_name = "GBuffer";
        gbuffer.param_defs.push_back({"base_color_texture", "Base Color", MaterialParamType::Texture2D});
        gbuffer.param_defs.push_back({"metallic_roughness_texture", "Metallic Roughness", MaterialParamType::Texture2D});
        gbuffer.param_defs.push_back({"normal_texture", "Normal", MaterialParamType::Texture2D});
        gbuffer.param_defs.push_back({"emissive_texture", "Emissive", MaterialParamType::Texture2D});
        registerTemplate(gbuffer);
    }

    void MaterialSystem::shutdown() {
        m_instances.clear();
        m_templates.clear();
        m_shader_library = nullptr;
        m_binding_layout_cache = nullptr;
        m_binding_set_cache = nullptr;
        m_texture_manager = nullptr;
        m_global_revision = 0;
    }

    Bool MaterialSystem::registerTemplate(const MaterialTemplateDesc& desc) {
        if (desc.name.empty()) {
            DO_ERROR("MaterialSystem::registerTemplate empty name");
            return false;
        }
        if (m_templates.contains(desc.name)) {
            DO_ERROR("MaterialSystem::registerTemplate duplicate name: {}", desc.name);
            return false;
        }

        MaterialTemplate tpl{};
        tpl.desc = desc;
        m_templates[desc.name] = std::move(tpl);
        ++m_global_revision;
        return true;
    }

    const MaterialTemplate* MaterialSystem::findTemplate(const String& name) const {
        auto it = m_templates.find(name);
        if (it != m_templates.end()) {
            return &it->second;
        }
        return nullptr;
    }

    const MaterialTemplate* MaterialSystem::findTemplateByShader(const String& shader_name) const {
        for (const auto& [name, tpl] : m_templates) {
            if (tpl.desc.shader_name == shader_name) {
                return &tpl;
            }
        }
        return nullptr;
    }

    Bool MaterialSystem::createInstance(const MaterialInstanceDesc& desc) {
        if (desc.name.empty()) {
            DO_ERROR("MaterialSystem::createInstance empty name");
            return false;
        }
        if (!m_templates.contains(desc.template_name)) {
            DO_ERROR("MaterialSystem::createInstance template not found: {}", desc.template_name);
            return false;
        }
        if (m_instances.contains(desc.name)) {
            DO_ERROR("MaterialSystem::createInstance duplicate name: {}", desc.name);
            return false;
        }

        MaterialInstance inst{};
        inst.desc = desc;
        inst.tpl = &m_templates[desc.template_name];
        m_instances[desc.name] = std::move(inst);
        ++m_global_revision;
        return true;
    }

    const MaterialInstance* MaterialSystem::findInstance(const String& name) const {
        auto it = m_instances.find(name);
        if (it != m_instances.end()) {
            return &it->second;
        }
        return nullptr;
    }

    void MaterialSystem::setInstanceParam(const String& instance_name,
                                          const String& param_name,
                                          MaterialParamValue value) {
        auto it = m_instances.find(instance_name);
        if (it == m_instances.end()) {
            DO_ERROR("MaterialSystem::setInstanceParam instance not found: {}", instance_name);
            return;
        }
        it->second.desc.param_overrides[param_name] = value;
        it->second.revision++;
        ++m_global_revision;
    }

    Bool MaterialSystem::resolveTemplate(const String& name) {
        auto it = m_templates.find(name);
        if (it == m_templates.end()) {
            DO_ERROR("MaterialSystem::resolveTemplate not found: {}", name);
            return false;
        }

        if (it->second.resolved) {
            return true;
        }

        if (!m_shader_library) {
            DO_ERROR("MaterialSystem::resolveTemplate ShaderLibrary not set");
            return false;
        }

        auto& tpl = it->second;

        const String vs_name = tpl.desc.shader_name + "VS";
        const String ps_name = tpl.desc.shader_name + "PS";

        const auto* vs_handle = m_shader_library->findShader(vs_name);
        const auto* ps_handle = m_shader_library->findShader(ps_name);

        if (!vs_handle) {
            DO_ERROR("MaterialSystem::resolveTemplate vertex shader not found: {}", vs_name);
            return false;
        }
        if (!ps_handle) {
            DO_ERROR("MaterialSystem::resolveTemplate pixel shader not found: {}", ps_name);
            return false;
        }

        tpl.vertex_shader = *vs_handle;
        tpl.pixel_shader = *ps_handle;

        if (m_binding_layout_cache) {
            GfxBindingLayoutDesc layout_desc{};
            layout_desc.setVisibility(GfxShaderType::All);

            UInt32 cb_slot = 0;
            UInt32 tex_slot = 0;
            UInt32 sampler_slot = 0;

            const auto* vs_refl = m_shader_library->getReflection(vs_name);
            const auto* ps_refl = m_shader_library->getReflection(ps_name);

            auto addReflection = [&](const ShaderReflectionData* refl) {
                if (!refl) return;
                for (const auto& cb : refl->constant_buffers) {
                    layout_desc.addItem(GfxBindingLayoutItem::VolatileConstantBuffer(cb.slot));
                    cb_slot = std::max(cb_slot, cb.slot + 1);
                }
                for (const auto& tex : refl->textures) {
                    layout_desc.addItem(
                        ShaderResourceKindToBindingItem(tex.kind, tex.slot, tex.array_size));
                    tex_slot = std::max(tex_slot, tex.slot + 1);
                }
                for (const auto& samp : refl->samplers) {
                    layout_desc.addItem(GfxBindingLayoutItem::Sampler(samp.slot));
                    sampler_slot = std::max(sampler_slot, samp.slot + 1);
                }
            };

            addReflection(vs_refl);
            addReflection(ps_refl);

            tpl.binding_layout = m_binding_layout_cache->getOrCreate(layout_desc);
        }

        tpl.resolved = true;
        ++tpl.revision;
        ++m_global_revision;
        return true;
    }

    Bool MaterialSystem::resolveInstance(const String& name) {
        auto it = m_instances.find(name);
        if (it == m_instances.end()) {
            DO_ERROR("MaterialSystem::resolveInstance not found: {}", name);
            return false;
        }

        if (it->second.resolved) {
            return true;
        }

        auto& inst = it->second;

        if (!inst.tpl->resolved) {
            if (!resolveTemplate(inst.tpl->desc.name)) {
                return false;
            }
        }

        const auto& param_defs = inst.tpl->desc.param_defs;

        UnorderedMap<String, MaterialParamValue> resolved_params;
        getResolvedParams(name, resolved_params);

        if (m_texture_manager) {
            inst.textures.clear();
            for (const auto& def : param_defs) {
                auto param_it = resolved_params.find(def.name);
                if (param_it == resolved_params.end()) {
                    continue;
                }
                if (def.type == MaterialParamType::Texture2D ||
                    def.type == MaterialParamType::TextureCube) {
                    resolveTextureSlot(inst, def, param_it->second);
                }
            }
        }

        inst.sampler = GDrawCommandList.createSampler(GfxSamplerDesc());

        if (!RenderSettings::IsBindlessActive() && m_binding_layout_cache && m_binding_set_cache && !inst.textures.empty()) {
            auto texture_layout = m_binding_layout_cache->getOrCreate(
                GfxBindingLayoutDesc()
                    .setVisibility(GfxShaderType::Pixel)
                    .addItem(GfxBindingLayoutItem::Texture_SRV(0))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(1)));

            GfxBindingSetDesc set_desc;
            set_desc.addItem(GfxBindingSetItem::Texture_SRV(0, inst.textures[0]->getRHIHandle()));
            set_desc.addItem(GfxBindingSetItem::Texture_SRV(1,
                (inst.textures.size() > 1 ? inst.textures[1] : inst.textures[0])->getRHIHandle()));
            inst.texture_binding_set = m_binding_set_cache->getOrCreate(
                set_desc,
                texture_layout,
                m_binding_layout_cache->getLayoutGeneration(texture_layout));
        }

        inst.resolved = true;
        ++inst.revision;
        ++m_global_revision;
        return true;
    }

    void MaterialSystem::resolveAll() {
        for (auto& [name, tpl] : m_templates) {
            resolveTemplate(name);
        }
        for (auto& [name, inst] : m_instances) {
            resolveInstance(name);
        }
    }

    Bool MaterialSystem::getResolvedMaterial(
        const String& instance_name,
        const UnorderedMap<String, UInt32>& permutation_overrides,
        ResolvedMaterial& out_material) {

        const auto* inst = findInstance(instance_name);
        if (!inst) {
            DO_ERROR("MaterialSystem::getResolvedMaterial instance not found: {}", instance_name);
            return false;
        }

        if (!inst->resolved) {
            DO_ERROR("MaterialSystem::getResolvedMaterial instance not resolved: {}", instance_name);
            return false;
        }

        const auto* tpl = inst->tpl;
        if (!tpl || !tpl->resolved) {
            DO_ERROR("MaterialSystem::getResolvedMaterial template not resolved for: {}", instance_name);
            return false;
        }

        out_material.vertex_shader = tpl->vertex_shader;
        out_material.pixel_shader = tpl->pixel_shader;
        out_material.binding_layout = tpl->binding_layout;
        out_material.rasterizer = tpl->desc.rasterizer;
        out_material.depth_stencil = tpl->desc.depth_stencil;
        out_material.blend = tpl->desc.blend;
        out_material.textures = inst->textures;
        out_material.sampler = inst->sampler;
        out_material.revision = tpl->revision ^ inst->revision;

        out_material.parameter_data.clear();

        return true;
    }

    void MaterialSystem::getResolvedParams(const String& instance_name,
                                           UnorderedMap<String, MaterialParamValue>& out_params) const {
        const auto* instance = findInstance(instance_name);
        if (!instance) {
            return;
        }

        const auto* tpl = instance->tpl;
        if (!tpl) {
            return;
        }

        for (const auto& def : tpl->desc.param_defs) {
            out_params[def.name] = def.default_value;
        }

        for (const auto& [name, value] : instance->desc.param_overrides) {
            out_params[name] = value;
        }
    }

    Bool MaterialSystem::resolveTextureSlot(MaterialInstance& instance,
                                            const MaterialParamDef& def,
                                            MaterialParamValue value) {
        if (!m_texture_manager) {
            return false;
        }

        const auto& tex_handle = value.texture;
        if (tex_handle) {
            instance.textures.push_back(tex_handle);
            Int32 desc_index = -1;
            if (auto* tex = findTexture2DByHandle(tex_handle)) {
                desc_index = tex->getDescriptorIndex();
            }
            instance.texture_descriptor_indices.push_back(desc_index);
            return true;
        }

        auto* fallback = m_texture_manager->getFallback();
        if (fallback && fallback->getGpuHandle()) {
            instance.textures.push_back(fallback->getGpuHandle());
            Int32 desc_index = -1;
            if (auto* tex = findTexture2DByHandle(fallback->getGpuHandle())) {
                desc_index = tex->getDescriptorIndex();
            } else if (fallback) {
                desc_index = fallback->getDescriptorIndex();
            }
            instance.texture_descriptor_indices.push_back(desc_index);
        }
        return true;
    }

    Bool MaterialSystem::buildConstantBufferData(const String& instance_name,
                                                  const ShaderCBReflection& cb_reflection,
                                                  DynamicArray<UInt8>& out_data) const {
        UnorderedMap<String, MaterialParamValue> params;
        getResolvedParams(instance_name, params);

        out_data.resize(cb_reflection.size);
        std::memset(out_data.data(), 0, cb_reflection.size);

        for (const auto& var : cb_reflection.variables) {
            auto param_it = params.find(var.name);
            if (param_it == params.end()) {
                continue;
            }

            const auto& value = param_it->second;
            const Size_t copy_size = var.size < sizeof(MaterialParamValue)
                ? static_cast<Size_t>(var.size) : sizeof(MaterialParamValue);

            if (var.offset + copy_size > cb_reflection.size) {
                DO_ERROR("MaterialSystem::buildConstantBufferData offset out of bounds for {}", var.name);
                continue;
            }

            std::memcpy(out_data.data() + var.offset, value.f, copy_size);
        }

        return true;
    }

    const MaterialInstance* MaterialSystem::getOrCreateInstance(
        const String& name,
        const String& template_name,
        const UnorderedMap<String, MaterialParamValue>& param_overrides) {
        if (name.empty() || template_name.empty()) {
            return nullptr;
        }

        auto* existing = findInstance(name);
        if (existing) {
            return existing;
        }

        MaterialInstanceDesc desc;
        desc.name = name;
        desc.template_name = template_name;
        desc.param_overrides = param_overrides;

        if (!createInstance(desc)) {
            return nullptr;
        }

        if (!resolveInstance(name)) {
            return nullptr;
        }

        return findInstance(name);
    }

    void MaterialSystem::invalidateForShader(const String& shader_name) {
        for (auto& [name, tpl] : m_templates) {
            if (tpl.desc.shader_name == shader_name) {
                tpl.resolved = false;
                tpl.vertex_shader = nullptr;
                tpl.pixel_shader = nullptr;
                tpl.binding_layout = nullptr;
                tpl.revision++;
            }
        }

        for (auto& [name, inst] : m_instances) {
            if (inst.tpl && inst.tpl->desc.shader_name == shader_name) {
                inst.resolved = false;
                inst.revision++;
            }
        }

        ++m_global_revision;
    }

    void MaterialSystem::invalidateForTexture(const GfxTextureHandle& texture) {
        for (auto& [name, inst] : m_instances) {
            for (const auto& tex : inst.textures) {
                if (tex.get() == texture.get()) {
                    inst.revision++;
                    break;
                }
            }
        }
        ++m_global_revision;
    }

    void MaterialSystem::invalidateAll() {
        for (auto& [name, tpl] : m_templates) {
            tpl.resolved = false;
        }
        for (auto& [name, inst] : m_instances) {
            inst.resolved = false;
        }
        ++m_global_revision;
    }

    Texture2D* MaterialSystem::findTexture2DByHandle(GfxTextureHandle handle) const {
        if (!m_texture_manager || !handle) {
            return nullptr;
        }

        for (const auto& [id, tex] : m_texture_manager->getTexture2DCache()) {
            if (tex && tex->getGpuHandle() && tex->getGpuHandle().get() == handle.get()) {
                return tex.get();
            }
        }
        return nullptr;
    }

} // namespace dodoe
