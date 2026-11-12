// do@Redlive

#include "render_graph.h"

#include "runtime/function/graphics/gfx_context.h"
#include "runtime/core/thread/wait_group.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/framebuffer_cache.h"

#include <cstdio>

namespace dodoe {

    namespace {

        void AddEdge(DynamicArray<DynamicArray<Size_t>>& edges, DynamicArray<Int32>& indegree, const Size_t from, const Size_t to) {
            if (from == to) {
                return;
            }
            edges[from].push_back(to);
            indegree[to] += 1;
        }

        GfxResourceStates accessToRequiredState(const RenderGraphAccessType access_type, const RenderGraphPipelineStage stage) {
            if (access_type == RenderGraphAccessType::ReadWrite) {
                return GfxResourceStates::UnorderedAccess;
            }
            if (access_type == RenderGraphAccessType::Read) {
                if (stage == RenderGraphPipelineStage::Copy) {
                    return GfxResourceStates::CopySource;
                }
                return GfxResourceStates::ShaderResource;
            }
            if (stage == RenderGraphPipelineStage::RenderTarget) {
                return GfxResourceStates::RenderTarget;
            }
            if (stage == RenderGraphPipelineStage::DepthStencil) {
                return GfxResourceStates::DepthWrite;
            }
            if (stage == RenderGraphPipelineStage::Copy) {
                return GfxResourceStates::CopyDest;
            }
            return GfxResourceStates::ShaderResource;
        }
    } // namespace

    void RenderGraph::addPass(const Ref<RenderGraphPass>& pass) {
        DO_ASSERT(!m_compiled, "Cannot add pass after compile");
        DO_ASSERT(pass, "RenderGraph::addPass requires valid pass");
        m_passes.push_back(pass);
    }

    UInt32 RenderGraph::addSubgraph(const String& name) {
        const UInt32 index = static_cast<UInt32>(m_subgraph_names.size());
        m_subgraph_names.push_back(name);
        return index;
    }

    UInt32 RenderGraph::addTextureResource(const RenderGraphTextureDesc& desc, const String& name) {
        DO_ASSERT(!m_compiled, "Cannot add resource after compile");
        RenderGraphResourceRecord record{};
        record.type = RenderGraphResourceType::Texture;
        record.source = RenderGraphResourceSource::Transient;
        record.name = name;
        record.texture_desc = desc;
        m_resources.push_back(record);
        return static_cast<UInt32>(m_resources.size() - 1);
    }

    UInt32 RenderGraph::addBufferResource(const RenderGraphBufferDesc& desc, const String& name) {
        DO_ASSERT(!m_compiled, "Cannot add resource after compile");
        RenderGraphResourceRecord record{};
        record.type = RenderGraphResourceType::Buffer;
        record.source = RenderGraphResourceSource::Transient;
        record.name = name;
        record.buffer_desc = desc;
        m_resources.push_back(record);
        return static_cast<UInt32>(m_resources.size() - 1);
    }

    UInt32 RenderGraph::addImportedTextureResource(const GfxTextureHandle& texture, const String& name) {
        DO_ASSERT(!m_compiled, "Cannot add resource after compile");
        DO_ASSERT(texture != nullptr, "RenderGraph::addImportedTextureResource requires valid texture");
        for (Size_t i = 0; i < m_resources.size(); ++i) {
            if (m_resources[i].source == RenderGraphResourceSource::ImportedTexture &&
                m_resources[i].imported_texture.get() == texture.get()) return static_cast<UInt32>(i);
        }
        RenderGraphResourceRecord record{};
        record.type = RenderGraphResourceType::Texture;
        record.source = RenderGraphResourceSource::ImportedTexture;
        record.name = name;
        record.imported_texture = texture;
        m_resources.push_back(record);
        return static_cast<UInt32>(m_resources.size() - 1);
    }

    UInt32 RenderGraph::addImportedBufferResource(const GfxBufferHandle& buffer, const String& name) {
        DO_ASSERT(!m_compiled, "Cannot add resource after compile");
        DO_ASSERT(buffer != nullptr, "RenderGraph::addImportedBufferResource requires valid buffer");
        for (Size_t i = 0; i < m_resources.size(); ++i) {
            if (m_resources[i].source == RenderGraphResourceSource::ImportedBuffer &&
                m_resources[i].imported_buffer.get() == buffer.get()) return static_cast<UInt32>(i);
        }
        RenderGraphResourceRecord record{};
        record.type = RenderGraphResourceType::Buffer;
        record.source = RenderGraphResourceSource::ImportedBuffer;
        record.name = name;
        record.imported_buffer = buffer;
        m_resources.push_back(record);
        return static_cast<UInt32>(m_resources.size() - 1);
    }

    UInt32 RenderGraph::addBackBufferResource(const String& name) {
        DO_ASSERT(!m_compiled, "Cannot add resource after compile");
        for (Size_t i = 0; i < m_resources.size(); ++i) {
            if (m_resources[i].source == RenderGraphResourceSource::ImportedBackBuffer) return static_cast<UInt32>(i);
        }
        RenderGraphResourceRecord record{};
        record.type = RenderGraphResourceType::Texture;
        record.source = RenderGraphResourceSource::ImportedBackBuffer;
        record.name = name;
        m_resources.push_back(record);
        return static_cast<UInt32>(m_resources.size() - 1);
    }

    void RenderGraph::validateAccesses() {
        DynamicArray<Bool> initialized(m_resources.size(), false);
        for (Size_t i = 0; i < m_resources.size(); ++i) initialized[i] = m_resources[i].isImported();
        for (Size_t i = 0; i < m_passes.size(); ++i) {
            if (m_culled_passes[i]) continue;
            const auto& pass = m_passes[i];
            // Uploads execute before all raster accesses regardless of declaration order.
            for (const auto& upload : pass->getUploads()) initialized[upload.buffer.index] = true;
            for (const auto& slot : pass->getColorSlots())
                DO_ASSERT(slot.load_op != LoadOp::Load || initialized[slot.texture.index],
                    "RenderGraph: Load requires initialized color contents");
            for (const auto& access : pass->getAccesses()) {
                if (access.access_type == RenderGraphAccessType::Read)
                    DO_ASSERT(initialized[access.resource_index], "RenderGraph: resource read before its first write");
                else initialized[access.resource_index] = true;
            }
        }
    }

    void RenderGraph::deriveBarriers() {
        // Declare the required state at each pass boundary. The RHI tracks the
        // actual state, including earlier submissions and upload transitions.
        for (Size_t index = 0; index < m_passes.size(); ++index) {
            auto& pass = m_passes[index];
            DynamicArray<RenderGraphBarrier> barriers;
            if (!m_culled_passes[index]) {
                for (const auto& access : pass->getAccesses()) {
                    if (access.upload) continue;
                    RenderGraphBarrier barrier{};
                    barrier.resource_index = access.resource_index;
                    barrier.resource_type = m_resources[access.resource_index].type;
                    barrier.to_state = access.required_state != GfxResourceStates::Unknown
                        ? access.required_state : accessToRequiredState(access.access_type, access.stage);
                    barrier.subresource = access.subresource;
                    barriers.push_back(barrier);
                }
            }
            pass->setAutoBarriers(std::move(barriers));
        }
    }

    void RenderGraph::cullUnreachablePasses(const DynamicArray<DynamicArray<Size_t>>& edges) {
        DynamicArray<DynamicArray<Size_t>> predecessors(m_passes.size());
        for (Size_t from = 0; from < edges.size(); ++from)
            for (const auto to : edges[from]) predecessors[to].push_back(from);

        DynamicArray<Bool> reachable(m_passes.size(), false);
        DynamicArray<Size_t> pending;
        for (Size_t i = 0; i < m_passes.size(); ++i)
            if (HasAnyFlags(m_passes[i]->getFlags(), RenderGraphPassFlags::NeverCull)) pending.push_back(i);
        for (const auto& resource : m_resources) {
            if ((resource.is_exported || resource.source == RenderGraphResourceSource::ImportedBackBuffer) &&
                !resource.writer_passes.empty()) pending.push_back(resource.writer_passes.back());
        }
        while (!pending.empty()) {
            const auto index = pending.back();
            pending.pop_back();
            if (reachable[index]) continue;
            reachable[index] = true;
            for (const auto predecessor : predecessors[index]) pending.push_back(predecessor);
        }
        for (auto& resource : m_resources) resource.is_live = resource.is_exported;
        for (Size_t i = 0; i < m_passes.size(); ++i) {
            m_culled_passes[i] = !reachable[i];
            if (reachable[i])
                for (const auto& access : m_passes[i]->getAccesses()) m_resources[access.resource_index].is_live = true;
        }
    }

    void RenderGraph::resetResourceTracking() {
        for (auto& resource : m_resources) {
            resource.writer_passes.clear();
            resource.first_pass_index = -1;
            resource.last_pass_index = -1;
            resource.reader_passes.clear();
        }
    }

    void RenderGraph::buildDependencyGraph(DynamicArray<DynamicArray<Size_t>>& edges, DynamicArray<Int32>& indegree) {
        const Size_t pass_count = m_passes.size();
        DynamicArray<DynamicArray<UInt32>> readers_since_write(m_resources.size());

        for (Size_t pass_index = 0; pass_index < pass_count; pass_index++) {
            const auto& pass = m_passes[pass_index];
            const auto& accesses = pass->getAccesses();

            if (accesses.empty() && !HasAnyFlags(pass->getFlags(), RenderGraphPassFlags::NeverCull)) {
                m_culled_passes[pass_index] = true;
                continue;
            }

            for (const auto& access : accesses) {
                DO_ASSERT(access.resource_index < m_resources.size(), "RenderGraph access resource index out of range");
                auto& resource = m_resources[access.resource_index];

                if (resource.first_pass_index < 0) {
                    resource.first_pass_index = static_cast<Int32>(pass_index);
                }
                resource.last_pass_index = static_cast<Int32>(pass_index);

                if (access.access_type == RenderGraphAccessType::Read) {
                    if (!resource.writer_passes.empty()) {
                        AddEdge(edges, indegree, static_cast<Size_t>(resource.writer_passes.back()), pass_index);
                    }
                    resource.reader_passes.push_back(static_cast<UInt32>(pass_index));
                    readers_since_write[access.resource_index].push_back(static_cast<UInt32>(pass_index));
                    continue;
                }

                if (!resource.writer_passes.empty()) {
                    AddEdge(edges, indegree, static_cast<Size_t>(resource.writer_passes.back()), pass_index);
                }

                for (const auto reader_index : readers_since_write[access.resource_index]) {
                    AddEdge(edges, indegree, reader_index, pass_index);
                }

                readers_since_write[access.resource_index].clear();
                resource.writer_passes.push_back(static_cast<UInt32>(pass_index));
            }
        }
    }

    void RenderGraph::topologicalSort(const DynamicArray<DynamicArray<Size_t>>& edges, const DynamicArray<Int32>& indegree) {
        const Size_t pass_count = m_passes.size();
        DynamicArray<Int32> indegree_work = indegree;
        DynamicArray<Size_t> current_level{};
        current_level.reserve(pass_count);

        for (Size_t i = 0; i < pass_count; i++) {
            if (indegree_work[i] == 0) {
                current_level.push_back(i);
            }
        }

        Size_t processed = 0;
        while (processed < pass_count) {
            DO_ASSERT(!current_level.empty(), "RenderGraph has cyclic dependency");

            DynamicArray<Size_t> next_level{};
            DynamicArray<Size_t> execution_level{};
            for (const auto node : current_level) {
                if (!m_culled_passes[node]) {
                    execution_level.push_back(node);
                }
                for (const auto to : edges[node]) {
                    indegree_work[to] -= 1;
                    if (indegree_work[to] == 0) {
                        next_level.push_back(to);
                    }
                }
            }

            if (!execution_level.empty()) {
                m_levels.push_back(execution_level);
            }

            processed += current_level.size();
            current_level = std::move(next_level);
        }
    }

    void RenderGraph::compile() {
        DO_PROFILE_SCOPE_CATEGORY("RenderGraph::compile", "frame");
        m_levels.clear();
        m_culled_passes.clear();
        m_compiled = false;

        const Size_t pass_count = m_passes.size();
        DynamicArray<DynamicArray<Size_t>> edges(pass_count);
        DynamicArray<Int32> indegree(pass_count, 0);
        m_culled_passes.resize(pass_count, false);

        resetResourceTracking();
        buildDependencyGraph(edges, indegree);

        cullUnreachablePasses(edges);
        validateAccesses();
        deriveBarriers();

        topologicalSort(edges, indegree);

        m_compiled = true;
    }

    void RenderGraph::execute(ThreadPool& pool, const RenderGraphExecuteContext& context, DrawCommandList& out_commands) {
        DO_PROFILE_SCOPE_CATEGORY("RenderGraph::execute", "frame");
        DO_ASSERT(m_compiled, "RenderGraph must be compiled before execute");
        DO_ASSERT(context.gfx_context != nullptr, "RenderGraphContext gfx_context is null");

        DO_ASSERT(context.transient_resource_pool != nullptr,
                  "RenderGraphExecuteContext transient resource pool is null");
        if (context.shared_render_service) {
            context.transient_resource_pool->flushEvictions(
                context.shared_render_service->getFramebufferCache(),
                context.shared_render_service->getBindingSetCache());
        }
        RenderGraphResourceResolver resource_resolver(
            m_resources, *context.gfx_context, context.swapchain_image_index,
            out_commands, context.transient_resource_pool);

        const bool direct_mode = context.gfx_context->getOpenGLBackend() != nullptr;

        struct PreparedPassAttachments {
            GfxFramebufferHandle framebuffer{};
            GfxFramebufferInfo framebuffer_info{};
            Bool has_attachments{false};
            Bool uses_swapchain{false};
        };

        auto preparePassAttachments = [&](const Ref<RenderGraphPass>& pass) {
            PreparedPassAttachments prepared{};
            if (!pass->hasRenderTargetSlots()) return prepared;

            for (const auto& slot : pass->getColorSlots()) {
                if (m_resources[slot.texture.index].source != RenderGraphResourceSource::ImportedBackBuffer) continue;
                DO_ASSERT(pass->getColorSlots().size() == 1 && !pass->getDepthSlot().has_value(),
                    "Swapchain framebuffer cannot be combined with other attachments.");
                DO_ASSERT(slot.load_op != LoadOp::Clear,
                    "Swapchain framebuffer clear must be issued explicitly by the pass.");
                const auto framebuffer = context.gfx_context->getSwapchainFramebuffer(context.swapchain_image_index);
                DO_ASSERT(framebuffer != nullptr, "RenderGraph swapchain framebuffer is unavailable.");
                prepared.framebuffer = framebuffer;
                prepared.framebuffer_info = framebuffer->getFramebufferInfo();
                prepared.has_attachments = true;
                prepared.uses_swapchain = true;
                return prepared;
            }

            GfxFramebufferDesc fb_desc{};
            Bool has_attachments = false;

            for (const auto& slot : pass->getColorSlots()) {
                const auto tex = resource_resolver.getTexture(slot.texture);
                fb_desc.addColorAttachment(tex);
                has_attachments = true;
            }

            if (pass->getDepthSlot().has_value()) {
                const auto& ds = pass->getDepthSlot().value();
                const auto tex = resource_resolver.getTexture(ds.texture);
                fb_desc.setDepthAttachment(tex);
                has_attachments = true;
            }

            if (!has_attachments) return prepared;

            if (context.shared_render_service) {
                auto* fb_cache = context.shared_render_service->getFramebufferCache();
                if (fb_cache) {
                    FramebufferCacheKey key{};
                    for (const auto& tex : fb_desc.colors()) {
                        key.color_refs.push_back({tex.get(), 0, 0, 0});
                    }
                    if (fb_desc.depth()) {
                        key.depth_ref = {fb_desc.depth().get(), 0, 0, 0};
                    }
                    const auto first = !fb_desc.colors().empty() ? fb_desc.colors().front() : fb_desc.depth();
                    if (first) key.sample_count = first->getDesc().sampleCount;
                    prepared.framebuffer = fb_cache->getOrCreate(out_commands, key, fb_desc);
                } else {
                    prepared.framebuffer = out_commands.createFramebuffer(fb_desc);
                }
            } else {
                prepared.framebuffer = out_commands.createFramebuffer(fb_desc);
            }
            prepared.framebuffer_info = GfxFramebufferInfo(fb_desc);
            {
                const GfxTextureHandle first_attachment = !fb_desc.colors().empty() ? fb_desc.colors().front() : fb_desc.depth();
                if (first_attachment && first_attachment->getDesc().sampleCount > 1) {
                    prepared.framebuffer_info.setSampleCount(first_attachment->getDesc().sampleCount);
                }
            }
            prepared.has_attachments = true;
            return prepared;
        };

        auto recordPassAttachments = [&](const Ref<RenderGraphPass>& pass,
                                         const PreparedPassAttachments& prepared,
                                         RenderGraphPassContext& pass_context,
                                         DrawCommandList& cmd) {
            if (!prepared.has_attachments) return;

            if (!prepared.uses_swapchain) {
                for (const auto& slot : pass->getColorSlots()) {
                    if (slot.load_op == LoadOp::Clear) {
                        cmd.clearTextureFloat(resource_resolver.getTexture(slot.texture),
                                              GfxAllSubresources, slot.clear_color);
                    }
                }

                if (pass->getDepthSlot().has_value()) {
                    const auto& ds = pass->getDepthSlot().value();
                    if (ds.depth_load_op == LoadOp::Clear) {
                        cmd.clearDepthStencilTexture(resource_resolver.getTexture(ds.texture),
                                                     GfxAllSubresources, true, ds.clear_depth, false, 0);
                    }
                }
            }

            pass_context.setFramebuffer(prepared.framebuffer);
            pass_context.setFramebufferInfo(prepared.framebuffer_info);
        };

        auto recordPass = [&](const Ref<RenderGraphPass>& pass, const PreparedPassAttachments& attachments,
                              DrawCommandList& commands) {
            GfxRenderScope render_scope;
            RenderGraphPassContext pass_context(context, resource_resolver);
            commands.beginMarker(pass->getName().c_str());
            for (const auto& upload : pass->getUploads())
                commands.setBufferState(resource_resolver.getBuffer(upload.buffer), GfxResourceStates::CopyDest);
            if (!pass->getUploads().empty()) commands.commitBarriers();
            for (const auto& upload : pass->getUploads())
                commands.writeBuffer(resource_resolver.getBuffer(upload.buffer), upload.data.data(),
                                     upload.data.size(), upload.offset);
            for (const auto& barrier : pass->getPreBarriers()) {
                if (barrier.resource_type == RenderGraphResourceType::Texture) {
                    const auto texture = resource_resolver.getTexture({barrier.resource_index});
                    if (texture) commands.setTextureState(texture, GfxAllSubresources, barrier.to_state);
                } else {
                    commands.setBufferState(resource_resolver.getBuffer({barrier.resource_index}), barrier.to_state);
                }
            }
            if (!pass->getPreBarriers().empty()) commands.commitBarriers();
            recordPassAttachments(pass, attachments, pass_context, commands);
            pass->execute(pass_context, commands);
            commands.endMarker();
        };

        for (const auto& level : m_levels) {
            DynamicArray<PreparedPassAttachments> attachments;
            for (const auto index : level) attachments.push_back(preparePassAttachments(m_passes[index]));
            if (direct_mode || level.size() == 1) {
                for (Size_t i = 0; i < level.size(); ++i) recordPass(m_passes[level[i]], attachments[i], out_commands);
                continue;
            }
            DynamicArray<DrawCommandList> lists(level.size());
            WaitGroup group(static_cast<Int32>(level.size()));
            for (Size_t i = 0; i < level.size(); ++i) {
                lists[i].setDevice(context.gfx_context->getDevice());
                pool.enqueue([&, i] {
                    recordPass(m_passes[level[i]], attachments[i], lists[i]);
                    group.done();
                });
            }
            group.wait();
            for (auto& list : lists) out_commands.append(std::move(list));
        }
        Bool has_exports = false;
        for (Size_t i = 0; i < m_resources.size(); ++i) {
            const auto& resource = m_resources[i];
            if (resource.is_exported && resource.export_final_state != GfxResourceStates::Unknown) {
                out_commands.setTextureState(resource_resolver.getTexture({static_cast<UInt32>(i)}),
                    GfxAllSubresources, resource.export_final_state);
                has_exports = true;
            }
        }
        if (has_exports) out_commands.commitBarriers();
        if (context.shared_render_service) {
            context.transient_resource_pool->flushEvictions(
                context.shared_render_service->getFramebufferCache(),
                context.shared_render_service->getBindingSetCache());
        }
    }

    void RenderGraph::reset() {
        m_passes.clear();
        m_resources.clear();
        m_levels.clear();
        m_culled_passes.clear();
        m_subgraph_names.clear();
        m_compiled = false;
    }

    String RenderGraph::dumpToJSON() const {
        std::ostringstream oss;
        oss << "{\n  \"passes\": [\n";
        for (Size_t i = 0; i < m_passes.size(); i++) {
            if (i > 0) oss << ",\n";
            oss << "    { \"name\": \"" << m_passes[i]->getName()
                << "\", \"culled\": " << (m_culled_passes[i] ? "true" : "false")
                << ", \"async\": " << (HasAnyFlags(m_passes[i]->getFlags(), RenderGraphPassFlags::AsyncCompute) ? "true" : "false")
                << ", \"accesses\": [";
            const auto& accesses = m_passes[i]->getAccesses();
            for (Size_t j = 0; j < accesses.size(); j++) {
                if (j > 0) oss << ", ";
                oss << "\"" << m_resources[accesses[j].resource_index].name << "\"";
            }
            oss << "], \"barriers\": " << m_passes[i]->getPreBarriers().size() << " }";
        }
        oss << "\n  ],\n  \"resources\": [\n";
        for (Size_t i = 0; i < m_resources.size(); i++) {
            if (i > 0) oss << ",\n";
            oss << "    { \"name\": \"" << m_resources[i].name
                << "\", \"exported\": " << (m_resources[i].is_exported ? "true" : "false")
                << ", \"imported\": " << (m_resources[i].isImported() ? "true" : "false")
                << ", \"first_pass\": " << m_resources[i].first_pass_index
                << ", \"last_pass\": " << m_resources[i].last_pass_index << " }";
        }
        oss << "\n  ]\n}\n";
        return String(oss.str().data(), oss.str().size());
    }

    String RenderGraph::dumpToDOT() const {
        std::ostringstream oss;
        oss << "digraph RenderGraph {\n  rankdir=LR;\n";
        for (Size_t i = 0; i < m_passes.size(); i++) {
            const char* color = m_culled_passes[i] ? "gray" : "black";
            oss << "  p" << i << " [label=\"" << m_passes[i]->getName()
                << "\", color=" << color << "];\n";
        }
        for (Size_t i = 0; i < m_passes.size(); i++) {
            const auto& accesses = m_passes[i]->getAccesses();
            for (const auto& access : accesses) {
                if (access.access_type != RenderGraphAccessType::Read) {
                    const auto& resource = m_resources[access.resource_index];
                    for (const auto writer_idx : resource.writer_passes) {
                        oss << "  p" << writer_idx << " -> p" << i
                            << " [label=\"" << resource.name << "\"];\n";
                    }
                }
            }
        }
        oss << "}\n";
        return String(oss.str().data(), oss.str().size());
    }

} // dodoe
