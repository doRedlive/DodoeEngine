// do@Redlive

#include "material_system.h"

namespace dodoe {

    Bool MaterialSystem::registerTemplate(const MaterialTemplateDesc& desc) {
        if (desc.name.empty()) {
            DO_ERROR("MaterialSystem::registerTemplate empty name");
            return false;
        }
        if (m_templates.contains(desc.name)) {
            DO_ERROR("MaterialSystem::registerTemplate duplicate name: {}", desc.name);
            return false;
        }
        m_templates[desc.name] = desc;
        return true;
    }

    const MaterialTemplateDesc* MaterialSystem::findTemplate(const String& name) const {
        auto it = m_templates.find(name);
        if (it != m_templates.end()) {
            return &it->second;
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
        m_instances[desc.name] = desc;
        return true;
    }

    const MaterialInstanceDesc* MaterialSystem::findInstance(const String& name) const {
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
        it->second.param_overrides[param_name] = value;
    }

    void MaterialSystem::getResolvedParams(const String& instance_name,
                                           UnorderedMap<String, MaterialParamValue>& out_params) const {
        const auto* instance = findInstance(instance_name);
        if (!instance) {
            return;
        }

        const auto* tmpl = findTemplate(instance->template_name);
        if (!tmpl) {
            return;
        }

        for (const auto& def : tmpl->param_defs) {
            out_params[def.name] = def.default_value;
        }

        for (const auto& [name, value] : instance->param_overrides) {
            out_params[name] = value;
        }
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
            Size_t copy_size = var.size < sizeof(MaterialParamValue)
                ? static_cast<Size_t>(var.size) : sizeof(MaterialParamValue);

            if (var.offset + copy_size > cb_reflection.size) {
                DO_ERROR("MaterialSystem::buildConstantBufferData offset out of bounds for {}", var.name);
                continue;
            }

            std::memcpy(out_data.data() + var.offset, value.f, copy_size);
        }

        return true;
    }

} // namespace dodoe
