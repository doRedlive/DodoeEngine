// do@Redlive

#include "render_graph.h"

#include "runtime/core/thread/wait_group.h"

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
        RenderGraphResourceRecord record{};
        record.type = RenderGraphResourceType::Texture;
        record.source = RenderGraphResourceSource::ImportedBackBuffer;
        record.name = name;
        m_resources.push_back(record);
        return static_cast<UInt32>(m_resources.size() - 1);
    }

    void RenderGraph::validateAccesses() {
        for (const auto& resource : m_resources) {
            if (resource.first_pass_index < 0) {
                continue;
            }
            if (resource.isImported()) {
                continue;
            }
            const Bool has_writer = resource.producer_pass_index >= 0;
            const Bool has_readers = !resource.reader_passes.empty();
            if (!has_writer && has_readers) {
                DO_ASSERT(false, "RenderGraph validation: uninitialized read - resource read before any write");
            }
            if (has_writer && !has_readers && !resource.is_exported) {
                DO_ASSERT(false, "RenderGraph validation: resource written but never read and not exported");
            }
        }

        for (Size_t pass_index = 0; pass_index < m_passes.size(); pass_index++) {
            if (m_culled_passes[pass_index]) {
                continue;
            }
            const auto& pass = m_passes[pass_index];
            for (const auto& access : pass->getAccesses()) {
                if (access.access_type != RenderGraphAccessType::ReadWrite) {
                    continue;
                }
                const auto& resource = m_resources[access.resource_index];
                for (const auto reader_index : resource.reader_passes) {
                    if (reader_index == pass_index) {
                        continue;
                    }
                    const auto& other_pass = m_passes[reader_index];
                    for (const auto& other_access : other_pass->getAccesses()) {
                        if (other_access.resource_index == access.resource_index &&
                            other_access.access_type == RenderGraphAccessType::ReadWrite) {
                            DO_ASSERT(false, "RenderGraph validation: UAV conflict - two passes use ReadWrite on same resource");
                        }
                    }
                }
            }
        }
    }

    void RenderGraph::deriveBarriers() {
        const Size_t resource_count = m_resources.size();
        DynamicArray<GfxResourceStates> current_states(resource_count, GfxResourceStates::Unknown);

        for (Size_t i = 0; i < resource_count; i++) {
            if (m_resources[i].isImported() || m_resources[i].source == RenderGraphResourceSource::ImportedBackBuffer) {
                current_states[i] = GfxResourceStates::Common;
            }
        }

        for (Size_t pass_index = 0; pass_index < m_passes.size(); pass_index++) {
            if (m_culled_passes[pass_index]) {
                continue;
            }
            auto& pass = m_passes[pass_index];
            DynamicArray<RenderGraphBarrier> pre_barriers{};

            for (const auto& access : pass->getAccesses()) {
                const UInt32 res_idx = access.resource_index;
                const GfxResourceStates required = accessToRequiredState(access.access_type, access.stage);

                if (current_states[res_idx] != required && current_states[res_idx] != GfxResourceStates::Unknown) {
                    RenderGraphBarrier barrier{};
                    barrier.resource_index = res_idx;
                    barrier.resource_type = m_resources[res_idx].type;
                    barrier.from_state = current_states[res_idx];
                    barrier.to_state = required;
                    barrier.subresource = access.subresource;
                    pre_barriers.push_back(barrier);
                }
                current_states[res_idx] = required;
            }

            if (!pre_barriers.empty()) {
                pass->setAutoBarriers(std::move(pre_barriers));
            }
        }
    }

    void RenderGraph::cullUnreachablePasses() {
        const Size_t pass_count = m_passes.size();
        DynamicArray<Bool> reachable_passes(pass_count, false);
        DynamicArray<Bool> reachable_resources(m_resources.size(), false);
        DynamicArray<UInt32> resource_queue{};

        for (Size_t i = 0; i < m_resources.size(); i++) {
            if (m_resources[i].is_exported || m_resources[i].source == RenderGraphResourceSource::ImportedBackBuffer) {
                reachable_resources[i] = true;
                resource_queue.push_back(static_cast<UInt32>(i));
            }
        }

        while (!resource_queue.empty()) {
            const UInt32 res_idx = resource_queue.back();
            resource_queue.pop_back();
            const auto& resource = m_resources[res_idx];

            if (resource.producer_pass_index >= 0) {
                const Size_t producer_idx = static_cast<Size_t>(resource.producer_pass_index);
                if (!reachable_passes[producer_idx]) {
                    reachable_passes[producer_idx] = true;
                    const auto& producer_pass = m_passes[producer_idx];
                    for (const auto& access : producer_pass->getAccesses()) {
                        if (access.access_type != RenderGraphAccessType::Write &&
                            access.access_type != RenderGraphAccessType::ReadWrite) {
                            if (!reachable_resources[access.resource_index]) {
                                reachable_resources[access.resource_index] = true;
                                resource_queue.push_back(access.resource_index);
                            }
                        }
                    }
                }
            }

            for (const auto reader_idx : resource.reader_passes) {
                if (!reachable_passes[reader_idx]) {
                    reachable_passes[reader_idx] = true;
                    const auto& reader_pass = m_passes[reader_idx];
                    for (const auto& access : reader_pass->getAccesses()) {
                        if (access.access_type == RenderGraphAccessType::Write ||
                            access.access_type == RenderGraphAccessType::ReadWrite) {
                            if (!reachable_resources[access.resource_index]) {
                                reachable_resources[access.resource_index] = true;
                                resource_queue.push_back(access.resource_index);
                            }
                        }
                    }
                }
            }
        }

        for (Size_t i = 0; i < pass_count; i++) {
            if (HasAnyFlags(m_passes[i]->getFlags(), RenderGraphPassFlags::NeverCull)) {
                continue;
            }
            if (!reachable_passes[i]) {
                m_culled_passes[i] = true;
            }
        }
    }

    void RenderGraph::resetResourceTracking() {
        for (auto& resource : m_resources) {
            resource.producer_pass_index = -1;
            resource.first_pass_index = -1;
            resource.last_pass_index = -1;
            resource.reader_passes.clear();
        }
    }

    void RenderGraph::buildDependencyGraph(DynamicArray<DynamicArray<Size_t>>& edges, DynamicArray<Int32>& indegree) {
        const Size_t pass_count = m_passes.size();

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
                    if (resource.producer_pass_index >= 0) {
                        AddEdge(edges, indegree, static_cast<Size_t>(resource.producer_pass_index), pass_index);
                    }
                    resource.reader_passes.push_back(static_cast<UInt32>(pass_index));
                    continue;
                }

                if (resource.producer_pass_index >= 0) {
                    AddEdge(edges, indegree, static_cast<Size_t>(resource.producer_pass_index), pass_index);
                }

                for (const auto reader_index : resource.reader_passes) {
                    AddEdge(edges, indegree, reader_index, pass_index);
                }

                resource.reader_passes.clear();
                resource.producer_pass_index = static_cast<Int32>(pass_index);
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
        m_levels.clear();
        m_culled_passes.clear();
        m_compiled = false;

        const Size_t pass_count = m_passes.size();
        DynamicArray<DynamicArray<Size_t>> edges(pass_count);
        DynamicArray<Int32> indegree(pass_count, 0);
        m_culled_passes.resize(pass_count, false);

        resetResourceTracking();
        buildDependencyGraph(edges, indegree);

        validateAccesses();
        deriveBarriers();
        cullUnreachablePasses();

        topologicalSort(edges, indegree);

        m_compiled = true;
    }

    void RenderGraph::execute(ThreadPool& pool, const RenderGraphExecuteContext& context, DrawCommandList& out_commands) {
        DO_ASSERT(m_compiled, "RenderGraph must be compiled before execute");
        DO_ASSERT(context.gfx_context != nullptr, "RenderGraphContext gfx_context is null");

        DO_ASSERT(context.transient_resource_pool != nullptr,
                  "RenderGraphExecuteContext transient resource pool is null");
        RenderGraphResourceResolver resource_resolver(
            m_resources, *context.gfx_context, context.swapchain_image_index,
            out_commands, context.transient_resource_pool);

        const bool direct_mode = out_commands.isImmediate();

        for (Size_t graph_level_index = 0; graph_level_index < m_levels.size(); ++graph_level_index) {
            const auto& level = m_levels[graph_level_index];

            if (direct_mode) {
                for (Size_t i = 0; i < level.size(); ++i) {
                    const auto pass_index = level[i];
                    const auto pass = m_passes[pass_index];
                    RenderGraphPassContext pass_context(context, resource_resolver);
                    for (const auto& barrier : pass->getPreBarriers()) {
                        if (barrier.resource_type == RenderGraphResourceType::Texture) {
                            const auto texture = resource_resolver.getTexture({barrier.resource_index});
                            out_commands.setTextureState(texture, {}, barrier.to_state);
                        } else {
                            const auto buffer = resource_resolver.getBuffer({barrier.resource_index});
                            out_commands.setBufferState(buffer, barrier.to_state);
                        }
                    }
                    if (!pass->getPreBarriers().empty()) {
                        out_commands.commitBarriers();
                    }
                    out_commands.beginMarker(pass->getName().c_str());
                    pass->execute(pass_context, out_commands);
                    out_commands.endMarker();
                }
            } else {
                DynamicArray<DrawCommandList> pass_command_lists(level.size());
                for (auto& cmd_list : pass_command_lists) {
                    cmd_list.setDevice(context.gfx_context->getDevice());
                }

                WaitGroup wg(static_cast<Int32>(level.size()));
                for (Size_t i = 0; i < level.size(); ++i) {
                    const auto pass_index = level[i];
                    const auto pass = m_passes[pass_index];
                    auto* cmd_list = &pass_command_lists[i];

                    pool.enqueue([pass, &context, &resource_resolver, &wg, cmd_list] {
                        RenderGraphPassContext pass_context(context, resource_resolver);
                        for (const auto& barrier : pass->getPreBarriers()) {
                            if (barrier.resource_type == RenderGraphResourceType::Texture) {
                                const auto texture = resource_resolver.getTexture({barrier.resource_index});
                                cmd_list->setTextureState(texture, {}, barrier.to_state);
                            } else {
                                const auto buffer = resource_resolver.getBuffer({barrier.resource_index});
                                cmd_list->setBufferState(buffer, barrier.to_state);
                            }
                        }
                        if (!pass->getPreBarriers().empty()) {
                            cmd_list->commitBarriers();
                        }
                        cmd_list->beginMarker(pass->getName().c_str());
                        pass->execute(pass_context, *cmd_list);
                        cmd_list->endMarker();
                        wg.done();
                    });
                }
                wg.wait();

                for (auto& cmd_list : pass_command_lists) {
                    out_commands.append(std::move(cmd_list));
                }
            }
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
            oss << "    { \"name\": \"" << m_passes[i]->getName().c_str()
                << "\", \"culled\": " << (m_culled_passes[i] ? "true" : "false")
                << ", \"async\": " << (HasAnyFlags(m_passes[i]->getFlags(), RenderGraphPassFlags::AsyncCompute) ? "true" : "false")
                << ", \"accesses\": [";
            const auto& accesses = m_passes[i]->getAccesses();
            for (Size_t j = 0; j < accesses.size(); j++) {
                if (j > 0) oss << ", ";
                oss << "\"" << m_resources[accesses[j].resource_index].name.c_str() << "\"";
            }
            oss << "], \"barriers\": " << m_passes[i]->getPreBarriers().size() << " }";
        }
        oss << "\n  ],\n  \"resources\": [\n";
        for (Size_t i = 0; i < m_resources.size(); i++) {
            if (i > 0) oss << ",\n";
            oss << "    { \"name\": \"" << m_resources[i].name.c_str()
                << "\", \"exported\": " << (m_resources[i].is_exported ? "true" : "false")
                << ", \"imported\": " << (m_resources[i].isImported() ? "true" : "false")
                << ", \"first_pass\": " << m_resources[i].first_pass_index
                << ", \"last_pass\": " << m_resources[i].last_pass_index << " }";
        }
        oss << "\n  ]\n}\n";
        return oss.str();
    }

    String RenderGraph::dumpToDOT() const {
        std::ostringstream oss;
        oss << "digraph RenderGraph {\n  rankdir=LR;\n";
        for (Size_t i = 0; i < m_passes.size(); i++) {
            const char* color = m_culled_passes[i] ? "gray" : "black";
            oss << "  p" << i << " [label=\"" << m_passes[i]->getName().c_str()
                << "\", color=" << color << "];\n";
        }
        for (Size_t i = 0; i < m_passes.size(); i++) {
            const auto& accesses = m_passes[i]->getAccesses();
            for (const auto& access : accesses) {
                if (access.access_type != RenderGraphAccessType::Read) {
                    const auto& resource = m_resources[access.resource_index];
                    if (resource.producer_pass_index >= 0) {
                        oss << "  p" << resource.producer_pass_index << " -> p" << i
                            << " [label=\"" << resource.name.c_str() << "\"];\n";
                    }
                }
            }
        }
        oss << "}\n";
        return oss.str();
    }

} // dodoe
