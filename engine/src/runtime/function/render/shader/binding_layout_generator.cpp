// do@Redlive

#include "binding_layout_generator.h"

namespace dodoe {

    void BindingLayoutGenerator::MergeReflection(const ShaderReflectionData& refl,
                                                  DynamicArray<MergedBinding>& bindings) {
        for (const auto& cb : refl.constant_buffers) {
            Bool found = false;
            for (auto& existing : bindings) {
                if (existing.slot == cb.slot && existing.kind == ShaderResourceKind::ConstantBuffer) {
                    existing.visibility = existing.visibility | refl.stage;
                    found = true;
                    break;
                }
            }
            if (!found) {
                bindings.push_back({cb.slot, ShaderResourceKind::ConstantBuffer, refl.stage, 1});
            }
        }

        for (const auto& tex : refl.textures) {
            Bool found = false;
            for (auto& existing : bindings) {
                if (existing.slot == tex.slot && existing.kind == tex.kind) {
                    existing.visibility = existing.visibility | refl.stage;
                    if (tex.array_size > existing.array_size) {
                        existing.array_size = tex.array_size;
                    }
                    found = true;
                    break;
                }
            }
            if (!found) {
                bindings.push_back({tex.slot, tex.kind, refl.stage, tex.array_size});
            }
        }

        for (const auto& smp : refl.samplers) {
            Bool found = false;
            for (auto& existing : bindings) {
                if (existing.slot == smp.slot && existing.kind == ShaderResourceKind::Sampler) {
                    existing.visibility = existing.visibility | refl.stage;
                    found = true;
                    break;
                }
            }
            if (!found) {
                bindings.push_back({smp.slot, ShaderResourceKind::Sampler, refl.stage, 1});
            }
        }

        if (refl.uses_push_constants && refl.push_constant_size > 0) {
            Bool found = false;
            for (auto& existing : bindings) {
                if (existing.kind == ShaderResourceKind::PushConstants) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                bindings.push_back({0, ShaderResourceKind::PushConstants, refl.stage, refl.push_constant_size});
            }
        }
    }

    GeneratedBindingLayout BindingLayoutGenerator::Generate(const ShaderReflectionData& reflection) {
        DynamicArray<const ShaderReflectionData*> stages;
        stages.push_back(&reflection);
        return Generate(stages);
    }

    GeneratedBindingLayout BindingLayoutGenerator::Generate(const ShaderReflectionData& vs_reflection,
                                                             const ShaderReflectionData& ps_reflection) {
        DynamicArray<const ShaderReflectionData*> stages;
        stages.push_back(&vs_reflection);
        stages.push_back(&ps_reflection);
        return Generate(stages);
    }

    GeneratedBindingLayout BindingLayoutGenerator::Generate(const DynamicArray<const ShaderReflectionData*>& stages) {
        GeneratedBindingLayout result;
        DynamicArray<MergedBinding> merged;

        for (const auto* refl : stages) {
            if (!refl || !refl->valid()) {
                continue;
            }
            MergeReflection(*refl, merged);
        }

        if (merged.empty()) {
            return result;
        }

        GfxShaderType visibility = GfxShaderType::None;
        for (const auto& binding : merged) {
            visibility = visibility | binding.visibility;
        }
        result.desc.setVisibility(visibility);

        for (const auto& binding : merged) {
            GfxBindingLayoutItem item = ShaderResourceKindToBindingItem(binding.kind, binding.slot, binding.array_size);
            result.desc.addItem(item);
        }

        result.valid = true;
        return result;
    }

    GfxBindingLayoutHandle BindingLayoutGenerator::CreateLayout(const GeneratedBindingLayout& generated,
                                                                 GfxDeviceHandle device) {
        if (!generated.valid || !device) {
            return {};
        }
        return device->createBindingLayout(generated.desc);
    }

    GfxBindingLayoutHandle BindingLayoutGenerator::CreateLayout(const GeneratedBindingLayout& generated,
                                                                 DrawCommandList& cmd_list) {
        if (!generated.valid) {
            return {};
        }
        return cmd_list.createBindingLayout(generated.desc);
    }

} // namespace dodoe
