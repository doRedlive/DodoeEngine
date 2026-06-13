// do@Redlive

#include "render_graph.h"

namespace dodoe {
    namespace {
        class WaitGroup {
            std::mutex m_mutex{};
            std::condition_variable m_cv{};
            Int32 m_count{0};

        public:
            explicit WaitGroup(const Int32 count) : m_count(count) { }

            void done() {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_count--;
                }
                m_cv.notify_one();
            }

            void wait() {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this] { return m_count <= 0; });
            }
        };

        void addEdge(DynamicArray<DynamicArray<Size_t>>& edges, DynamicArray<Int32>& indegree, const Size_t from, const Size_t to) {
            if (from == to) {
                return;
            }
            edges[from].push_back(to);
            indegree[to] += 1;
        }
    }

    void RenderGraph::addPass(const Ref<RenderGraphPass>& pass) {
        DO_ASSERT(!m_compiled, "Cannot add pass after compile");
        DO_ASSERT(pass, "RenderGraph::addPass requires valid pass");
        m_passes.push_back(pass);
    }

    UInt32 RenderGraph::addTextureResource(const RenderGraphTextureDesc& desc, const String& name) {
        DO_ASSERT(!m_compiled, "Cannot add resource after compile");
        RenderGraphResourceRecord record{};
        record.type = RenderGraphResourceType::Texture;
        record.name = name;
        record.texture_desc = desc;
        m_resources.push_back(record);
        return static_cast<UInt32>(m_resources.size() - 1);
    }

    UInt32 RenderGraph::addBufferResource(const RenderGraphBufferDesc& desc, const String& name) {
        DO_ASSERT(!m_compiled, "Cannot add resource after compile");
        RenderGraphResourceRecord record{};
        record.type = RenderGraphResourceType::Buffer;
        record.name = name;
        record.buffer_desc = desc;
        m_resources.push_back(record);
        return static_cast<UInt32>(m_resources.size() - 1);
    }

    void RenderGraph::compile() {
        m_levels.clear();
        m_culled_passes.clear();
        m_compiled = false;

        const Size_t pass_count = m_passes.size();
        DynamicArray<DynamicArray<Size_t>> edges(pass_count);
        DynamicArray<Int32> indegree(pass_count, 0);
        m_culled_passes.resize(pass_count, false);

        for (auto& resource : m_resources) {
            resource.producer_pass_index = -1;
            resource.first_pass_index = -1;
            resource.last_pass_index = -1;
            resource.reader_passes.clear();
        }

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
                        addEdge(edges, indegree, static_cast<Size_t>(resource.producer_pass_index), pass_index);
                    }
                    resource.reader_passes.push_back(static_cast<UInt32>(pass_index));
                    continue;
                }

                if (resource.producer_pass_index >= 0) {
                    addEdge(edges, indegree, static_cast<Size_t>(resource.producer_pass_index), pass_index);
                }

                for (const auto reader_index : resource.reader_passes) {
                    addEdge(edges, indegree, reader_index, pass_index);
                }

                resource.reader_passes.clear();
                resource.producer_pass_index = static_cast<Int32>(pass_index);
            }
        }

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

        m_compiled = true;
    }

    DrawCommandList RenderGraph::execute(ThreadPool& pool, const RenderGraphExecuteContext& context) {
        DO_ASSERT(m_compiled, "RenderGraph must be compiled before execute");
        DO_ASSERT(context.gfx_context != nullptr, "RenderGraphContext gfx_context is null");

        RenderGraphResourceRegistry resource_registry{};
        resource_registry.initialize(m_resources, *context.gfx_context, context.swapchain_image_index);
        DrawCommandList merged_command_list{};

        for (const auto& level : m_levels) {
            WaitGroup wg(static_cast<Int32>(level.size()));
            DynamicArray<DrawCommandList> level_command_lists(level.size());
            for (Size_t level_index = 0; level_index < level.size(); level_index++) {
                const auto pass = m_passes[level[level_index]];
                pool.enqueue([pass, &context, &resource_registry, &level_command_lists, level_index, &wg] {
                    RenderGraphPassContext pass_context(context, resource_registry);
                    RenderGraphCommandList command_list(pass_context, level_command_lists[level_index]);
                    pass->execute(pass_context, command_list);
                    wg.done();
                });
            }
            wg.wait();

            for (auto& level_command_list : level_command_lists) {
                merged_command_list.append(std::move(level_command_list));
            }
        }

        return merged_command_list;
    }

    void RenderGraph::reset() {
        m_passes.clear();
        m_resources.clear();
        m_levels.clear();
        m_culled_passes.clear();
        m_compiled = false;
    }

} // dodoe
