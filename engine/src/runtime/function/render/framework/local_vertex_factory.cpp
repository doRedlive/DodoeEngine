// do@Redlive

#include "local_vertex_factory.h"

#include "runtime/function/render/mesh_draw/mesh_draw_types.h"
#include "runtime/function/render/render_scene/sprite_scene_info.h"
#ifdef DODOE_DEBUG
#include "imgui/imgui.h"
#endif

namespace dodoe {

    void LocalVertexFactory::initialize(
        DrawCommandList& command_list,
        const GfxShaderHandle& gbuffer_vertex_shader,
        const GfxShaderHandle& shadow_vertex_shader)
    {
        constexpr Size_t kVertexStride = sizeof(Vector3f) + sizeof(UInt32) + sizeof(Vector2f);
        constexpr Size_t kInstanceStride = sizeof(InstanceSceneData);
        DynamicArray<GfxVertexAttributeDesc> vertex_attributes = {
            GfxVertexAttributeDesc().setName("a_Position").setFormat(GfxFormat::RGB32_FLOAT).setOffset(0).setElementStride(kVertexStride),
            GfxVertexAttributeDesc().setName("a_Normal").setFormat(GfxFormat::RGBA8_SNORM).setOffset(sizeof(Vector3f)).setElementStride(kVertexStride),
            GfxVertexAttributeDesc().setName("a_UV").setFormat(GfxFormat::RG32_FLOAT).setOffset(sizeof(Vector3f) + sizeof(UInt32)).setElementStride(kVertexStride),
            GfxVertexAttributeDesc().setName("TEXCOORD3").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(0).setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("TEXCOORD4").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f)).setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("TEXCOORD5").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f) * 2).setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("TEXCOORD6").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f) * 3).setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("a_InstanceColorTint").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Matrix4f)).setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("a_InstanceParams").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Matrix4f) + sizeof(Vector4f)).setElementStride(kInstanceStride).setIsInstanced(true),
        };

        m_gbuffer_input_layout = command_list.createInputLayout(
            vertex_attributes.data(),
            static_cast<UInt32>(vertex_attributes.size()),
            gbuffer_vertex_shader
        );
        m_shadow_input_layout = command_list.createInputLayout(
            vertex_attributes.data(),
            static_cast<UInt32>(vertex_attributes.size()),
            shadow_vertex_shader
        );
    }

    void LocalVertexFactory::reset() {
        m_shadow_input_layout = nullptr;
        m_gbuffer_input_layout = nullptr;
        m_sprite_input_layout = nullptr;
        m_imgui_input_layout = nullptr;
    }

    GfxInputLayoutHandle LocalVertexFactory::getOrCreateSpriteInputLayout(
        DrawCommandList& command_list,
        GfxShaderHandle sprite_vs)
    {
        if (!m_sprite_input_layout) {
            constexpr UInt32 kQuadVertexStride = 28;
            constexpr UInt32 kSpriteInstanceStride = sizeof(SpriteInstance);
            GfxVertexAttributeDesc sprite_attribs[] = {
                GfxVertexAttributeDesc().setName("POSITION").setFormat(GfxFormat::RGB32_FLOAT).setOffset(0).setElementStride(kQuadVertexStride),
                GfxVertexAttributeDesc().setName("TEXCOORD").setFormat(GfxFormat::RG32_FLOAT).setOffset(12).setElementStride(kQuadVertexStride),
                GfxVertexAttributeDesc().setName("COLOR").setFormat(GfxFormat::RGBA8_UNORM).setOffset(20).setElementStride(kQuadVertexStride),
                GfxVertexAttributeDesc().setName("TEXINDEX").setFormat(GfxFormat::R32_UINT).setOffset(24).setElementStride(kQuadVertexStride),
                GfxVertexAttributeDesc().setName("TEXCOORD1").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(0).setElementStride(kSpriteInstanceStride).setIsInstanced(true),
                GfxVertexAttributeDesc().setName("TEXCOORD2").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(16).setElementStride(kSpriteInstanceStride).setIsInstanced(true),
                GfxVertexAttributeDesc().setName("TEXCOORD3").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(32).setElementStride(kSpriteInstanceStride).setIsInstanced(true),
                GfxVertexAttributeDesc().setName("TEXCOORD4").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(48).setElementStride(kSpriteInstanceStride).setIsInstanced(true),
            };
            m_sprite_input_layout = command_list.createInputLayout(sprite_attribs, 8, sprite_vs);
        }
        return m_sprite_input_layout;
    }

    GfxInputLayoutHandle LocalVertexFactory::getOrCreateImGuiInputLayout(
        DrawCommandList& command_list,
        GfxShaderHandle imgui_vs)
    {
#ifdef DODOE_DEBUG
        if (!m_imgui_input_layout) {
            GfxVertexAttributeDesc attributes[] = {
                GfxVertexAttributeDesc().setName("a_Position").setFormat(GfxFormat::RG32_FLOAT).setOffset(offsetof(ImDrawVert, pos)).setElementStride(sizeof(ImDrawVert)),
                GfxVertexAttributeDesc().setName("a_UV").setFormat(GfxFormat::RG32_FLOAT).setOffset(offsetof(ImDrawVert, uv)).setElementStride(sizeof(ImDrawVert)),
                GfxVertexAttributeDesc().setName("a_Color").setFormat(GfxFormat::RGBA8_UNORM).setOffset(offsetof(ImDrawVert, col)).setElementStride(sizeof(ImDrawVert)),
            };
            m_imgui_input_layout = command_list.createInputLayout(attributes, static_cast<UInt32>(std::size(attributes)), imgui_vs);
        }
        return m_imgui_input_layout;
#else
        (void)command_list;
        (void)imgui_vs;
        return nullptr;
#endif
    }

} // dodoe
