// do@Redlive

#include "srp_bridge.h"

#include "runtime/function/render/mesh_draw/mesh_phase_registry.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_graph/render_graph_pass.h"
#include "runtime/function/render/render_graph/render_graph_resource.h"
#include "runtime/function/render/render_pipeline/render_pass.h"
#include "runtime/function/render/render_pipeline/renderer.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/render_target_system.h"
#include "runtime/function/render/render_service/render_target_handle.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    namespace {

        GfxFormat ToGfxFormat(const SrpFormat format) {
            switch (format) {
            case SrpFormat::RGBA8_UNORM:  return GfxFormat::RGBA8_UNORM;
            case SrpFormat::RGBA16_FLOAT: return GfxFormat::RGBA16_FLOAT;
            case SrpFormat::RGBA32_FLOAT: return GfxFormat::RGBA32_FLOAT;
            case SrpFormat::RG16_FLOAT:   return GfxFormat::RG16_FLOAT;
            case SrpFormat::D32:          return GfxFormat::D32;
            }
            return GfxFormat::RGBA8_UNORM;
        }

        struct SrpRasterPassParameters {
            Int32 execute_id{0};
        };

    } // namespace

    SrpBridge& SrpBridge::Self() {
        static SrpBridge instance{};
        return instance;
    }

    void SrpBridge::setScriptCall(const ScriptCallFn call) {
        m_call = call;
        m_pipeline_created = false;
        if (m_renderer) {
            m_renderer->installManagedSrpFeatures();
        }
    }

    void SrpBridge::attachRenderer(BaseRenderer* renderer) {
        m_renderer = renderer;
        m_services = renderer->getSharedService();
        MeshPhaseRegistry::Self().registerBuiltinPhases();
    }

    void SrpBridge::detachRenderer(BaseRenderer* renderer) {
        if (m_renderer != renderer) {
            return;
        }
        m_renderer = nullptr;
        m_services = nullptr;
        m_managed_installed = false;
    }

    Bool SrpBridge::ensurePipeline(BaseRenderer* renderer) {
        attachRenderer(renderer);
        if (m_pipeline_created) {
            return true;
        }
        if (!m_call) {
            return false;
        }

        const char* pipeline_type = "Deferred";
        Int32 pipeline_id = 0;
        void* create_args[1] = { const_cast<char*>(pipeline_type) };
        void* create_result[1] = { &pipeline_id };
        m_call(ScriptCommand::SrpCreatePipeline, create_args, create_result);
        m_pipeline_id = pipeline_id;

        Int32 feature_count = 0;
        void* count_args[1] = { &m_pipeline_id };
        void* count_result[1] = { &feature_count };
        m_call(ScriptCommand::SrpGetFeatureCount, count_args, count_result);

        m_feature_ids.clear();
        m_feature_phases.clear();
        for (Int32 index = 0; index < feature_count; index++) {
            Int32 feature_id = 0;
            Int32 feature_phase = 0;
            void* feature_args[2] = { &m_pipeline_id, &index };
            void* feature_result[2] = { &feature_id, &feature_phase };
            m_call(ScriptCommand::SrpGetFeature, feature_args, feature_result);
            m_feature_ids.push_back(feature_id);
            m_feature_phases.push_back(feature_phase);
        }

        m_pipeline_created = true;
        return true;
    }

    void SrpBridge::shutdownPipeline() {
        if (!m_call || !m_pipeline_created) {
            return;
        }
        void* args[1] = { &m_pipeline_id };
        m_call(ScriptCommand::SrpShutdownPipeline, args, nullptr);
        m_pipeline_created = false;
        m_managed_installed = false;
        m_feature_ids.clear();
        m_feature_phases.clear();
        m_render_target_names.clear();
    }

    void SrpBridge::beginFrame() {
        if (!m_call || !m_pipeline_created) {
            return;
        }
        m_call(ScriptCommand::SrpClearExecuteCallbacks, nullptr, nullptr);
    }

    Bool SrpBridge::getFeature(const Int32 index, Int32& out_id, Int32& out_phase) const {
        if (index < 0 || index >= static_cast<Int32>(m_feature_ids.size())) {
            return false;
        }
        out_id = m_feature_ids[static_cast<Size_t>(index)];
        out_phase = m_feature_phases[static_cast<Size_t>(index)];
        return true;
    }

    void SrpBridge::featureInitialize(const Int32 feature_id) {
        if (!m_call) {
            return;
        }
        void* args[1] = { &feature_id };
        m_call(ScriptCommand::SrpFeatureInitialize, args, nullptr);
    }

    void SrpBridge::featureOnResize(const Int32 feature_id, const UInt32 width, const UInt32 height) {
        if (!m_call) {
            return;
        }
        UInt32 w = width;
        UInt32 h = height;
        void* args[3] = { &feature_id, &w, &h };
        m_call(ScriptCommand::SrpFeatureOnResize, args, nullptr);
    }

    void SrpBridge::featureDispose(const Int32 feature_id) {
        if (!m_call) {
            return;
        }
        void* args[1] = { &feature_id };
        m_call(ScriptCommand::SrpFeatureDispose, args, nullptr);
    }

    void SrpBridge::featureAddPasses(const Int32 feature_id, RenderGraphBuilder& graph,
                                     const RenderPassBuildContext& context) {
        if (!m_call) {
            return;
        }
        UInt64 graph_handle = reinterpret_cast<UInt64>(&graph);
        UInt64 view_handle = reinterpret_cast<UInt64>(&context.view);
        void* args[3] = { &feature_id, &graph_handle, &view_handle };
        m_current_graph = &graph;
        m_build_context = &context;
        m_call(ScriptCommand::SrpFeatureAddPasses, args, nullptr);
        m_current_graph = nullptr;
        m_build_context = nullptr;
    }

    void SrpBridge::executePass(const Int32 execute_id, RenderGraphPassContext& context,
                                DrawCommandList& command_list) {
        if (!m_call) {
            return;
        }
        Int32 id = execute_id;
        void* args[1] = { &id };
        m_pass_context = &context;
        m_command_list = &command_list;
        m_call(ScriptCommand::SrpExecute, args, nullptr);
        m_pass_context = nullptr;
        m_command_list = nullptr;
    }

    void SrpBridge::drawRenderers(const SrpMeshDrawSettings& settings) {
        MeshPassType pass_type = MeshPassType::Opaque;
        if (!MeshPhaseRegistry::Self().find(settings.phase, pass_type)) {
            return;
        }
        IMeshPhaseProvider* provider = m_renderer->findMeshPhaseProvider(pass_type);
        if (!provider) {
            return;
        }
        provider->drawPhase(*m_pass_context, *m_command_list);
    }

    Int32 SrpBridge::createRenderTarget(const String& name, const SrpFormat format,
                                        const Float scale_x, const Float scale_y) {
        RenderTargetDesc desc{};
        desc.name = name;
        desc.scale_policy = RenderTargetScalePolicy::Relative;
        desc.scale_x = scale_x;
        desc.scale_y = scale_y;
        desc.color_attachments.push_back({ToGfxFormat(format), name, GfxColor(0.0f, 0.0f, 0.0f, 1.0f)});
        m_services->getRenderTargetSystem()->create(name, desc);

        for (Size_t index = 0; index < m_render_target_names.size(); index++) {
            if (m_render_target_names[index] == name) {
                return static_cast<Int32>(index);
            }
        }
        m_render_target_names.push_back(name);
        return static_cast<Int32>(m_render_target_names.size() - 1);
    }

    Int32 SrpBridge::findRenderTarget(const String& name) {
        for (Size_t index = 0; index < m_render_target_names.size(); index++) {
            if (m_render_target_names[index] == name) {
                return static_cast<Int32>(index);
            }
        }
        if (!m_services->getRenderTargetSystem()->find(name)) {
            return -1;
        }
        m_render_target_names.push_back(name);
        return static_cast<Int32>(m_render_target_names.size() - 1);
    }

    void SrpBridge::resizeRenderTarget(const Int32 id, const UInt32 width, const UInt32 height) {
        RenderTargetHandle* handle = getRenderTarget(id);
        handle->resolve(width, height, *m_services->getGfxContext(), 0);
    }

    RenderTargetHandle* SrpBridge::getRenderTarget(const Int32 id) const {
        return m_services->getRenderTargetSystem()->find(m_render_target_names[static_cast<Size_t>(id)]);
    }

    Bool SrpBridge::isRenderTargetValid(const Int32 id) const {
        return id >= 0 && static_cast<Size_t>(id) < m_render_target_names.size();
    }

    UInt32 SrpBridge::createGraphTexture(const String& name, const UInt32 width, const UInt32 height,
                                         const SrpFormat format, const Bool depth, const UInt32 sample_count) {
        RenderGraphTextureDesc desc = depth
            ? MakeDepthTarget2D(width, height, ToGfxFormat(format), name, sample_count)
            : MakeRenderTarget2D(width, height, ToGfxFormat(format), name, sample_count);
        return m_current_graph->createTexture(desc, name).index;
    }

    UInt32 SrpBridge::importGraphTexture(const Int32 render_target, const String& name, const Bool depth) {
        RenderTargetHandle* handle = getRenderTarget(render_target);
        const GfxTextureHandle texture = depth ? handle->getDepthTexture() : handle->getColorTexture(0);
        return m_current_graph->importTexture(texture, name).index;
    }

    UInt32 SrpBridge::importGraphBackBuffer(const String& name) {
        return m_current_graph->importBackBuffer(name).index;
    }

    void SrpBridge::addRasterPass(const SrpRasterPassDesc& desc) {
        m_current_graph->addPass<SrpRasterPassParameters>(
            desc.name,
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [desc](RenderGraphPassBuilder& builder, SrpRasterPassParameters& parameters) {
                parameters.execute_id = desc.execute_id;
                for (Int32 index = 0; index < desc.color_count; index++) {
                    RenderGraphAttachmentInfo attachment{};
                    attachment.load_op = static_cast<LoadOp>(desc.color_loads[index]);
                    attachment.clear_color = GfxColor(
                        desc.color_clears[index * 4 + 0], desc.color_clears[index * 4 + 1],
                        desc.color_clears[index * 4 + 2], desc.color_clears[index * 4 + 3]);
                    RenderGraphTextureHandle handle{};
                    handle.index = static_cast<UInt32>(desc.color_handles[index]);
                    builder.writeColor(handle, attachment);
                }
                if (desc.depth_handle >= 0) {
                    RenderGraphAttachmentInfo attachment{};
                    attachment.load_op = static_cast<LoadOp>(desc.depth_load);
                    RenderGraphTextureHandle handle{};
                    handle.index = static_cast<UInt32>(desc.depth_handle);
                    builder.writeDepth(handle, attachment);
                }
                for (Int32 index = 0; index < desc.read_texture_count; index++) {
                    RenderGraphTextureHandle handle{};
                    handle.index = static_cast<UInt32>(desc.read_texture_handles[index]);
                    builder.readTexture(handle);
                }
            },
            [](const SrpRasterPassParameters& parameters, const RenderGraphPassContext& context,
               DrawCommandList& command_list) {
                SrpBridge::Self().executePass(parameters.execute_id,
                    const_cast<RenderGraphPassContext&>(context), command_list);
            });
    }

} // dodoe
