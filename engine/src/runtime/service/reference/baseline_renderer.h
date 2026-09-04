// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/render/render_scene/sprite_scene_info.h"

namespace dodoe {

    class ShaderLibrary;
    class SharedRenderService;
    class RenderViewFamily;
    class RenderView;
    class RenderScene;
    struct MaterialInstance;

    struct BaselineRendererCreateInfo {
        GfxDeviceHandle device{};
        const ShaderLibrary* shader_library{nullptr};
        SharedRenderService* shared_render_service{nullptr};
    };

    class BaselineRenderer final : public Managed<BaselineRenderer, BaselineRendererCreateInfo> {
        friend class Managed<BaselineRenderer, BaselineRendererCreateInfo>;
        GfxDeviceHandle m_device{};
        cutie::CommandListHandle m_command_list{};
        const ShaderLibrary* m_shader_library{nullptr};
        SharedRenderService* m_shared_render_service{nullptr};
        cutie::SamplerHandle m_sampler{};
        cutie::BindingLayoutHandle m_sprite_cb_binding_layout{};
        cutie::BindingLayoutHandle m_sprite_material_binding_layout{};
        cutie::InputLayoutHandle m_sprite_input_layout{};
        cutie::GraphicsPipelineHandle m_sprite_pipeline{};
        cutie::BufferHandle m_instance_buffer{};
        cutie::BufferHandle m_vp_buffer{};
        UInt32 m_instance_capacity{0};
        cutie::GraphicsPipelineHandle m_mesh_pipeline{};
        cutie::InputLayoutHandle m_mesh_input_layout{};
        cutie::BindingLayoutHandle m_mesh_global_binding_layout{};
        cutie::BindingLayoutHandle m_mesh_view_binding_layout{};
        cutie::BindingLayoutHandle m_mesh_material_binding_layout{};
        cutie::BindingLayoutHandle m_mesh_pass_binding_layout{};
        cutie::BindingLayoutHandle m_mesh_primitive_binding_layout{};
        cutie::BufferHandle m_mesh_global_cb{};
        cutie::BufferHandle m_mesh_view_cb{};
        cutie::BufferHandle m_mesh_primitive_cb{};
        cutie::BufferHandle m_mesh_pass_cb{};
        cutie::BufferHandle m_mesh_instance_buffer{};
        UInt32 m_mesh_instance_capacity{0};
        cutie::BindingSetHandle m_mesh_global_binding_set{};
        cutie::BindingSetHandle m_mesh_view_binding_set{};
        cutie::BindingSetHandle m_mesh_primitive_binding_set{};
        cutie::BindingSetHandle m_mesh_pass_binding_set{};
        UnorderedMap<String, cutie::BindingSetHandle> m_mesh_material_binding_sets{};
        cutie::BindingLayoutHandle m_present_binding_layout{};
        cutie::GraphicsPipelineHandle m_present_pipeline{};
        GfxTextureHandle m_scene_color{};
        GfxTextureHandle m_scene_depth{};
        GfxFramebufferHandle m_scene_framebuffer{};
        Vector2i m_scene_rt_extent{0, 0};
        Bool m_material_warning_logged{false};
        UInt64 m_frame_counter{0};

    public:
        void render(GfxContext& gfx, UInt32 swapchain_image_index, RenderViewFamily& view_family, RenderScene& scene);

    private:
        Bool initialize(const BaselineRendererCreateInfo& info);
        void shutdown();
        Bool createSpriteResources();
        Bool createMeshResources();
        Bool createPresentResources();
        Bool ensureRenderTarget(const Vector2i& extent);
        Bool ensureSpritePipeline(const cutie::FramebufferInfo& framebuffer_info);
        Bool ensureMeshPipeline(const cutie::FramebufferInfo& framebuffer_info);
        Bool ensurePresentPipeline(const cutie::FramebufferInfo& framebuffer_info);
        Bool ensureInstanceCapacity(UInt32 instance_count);
        Bool ensureMeshInstanceCapacity(UInt32 instance_count);
        void collectSpriteInstances(RenderView& view, RenderScene& scene, DynamicArray<SpriteInstance>& out_instances);
        void renderSprites(RenderView& view, RenderScene& scene, cutie::IFramebuffer* framebuffer, const GfxViewportState& viewport_state);
        void setupMeshExtension(RenderView& view, RenderViewFamily& view_family);
        Bool updateMeshPassBindingSet(RenderScene& scene);
        cutie::BindingSetHandle resolveMeshMaterialBindingSet(const MaterialInstance* material_instance);
        void renderMeshes(RenderView& view, RenderScene& scene, const GfxViewportState& viewport_state);
    };

} // dodoe
