// do@Redlive

#pragma once

#include "dopch.h"

#include "cached_mesh_draw_command.h"
#include "mesh_draw_list.h"

namespace dodoe {

    struct MeshPassThreadLocalCommandStorage {
        DynamicArray<MeshDrawCommandSource> sources;

        void reset() {
            sources.clear();
        }
    };

    class MeshPassCommandStorage {
        MeshDrawCommandCache m_cached_commands{};
        GfxGraphicsPipelineHandle m_pipeline{};
        DynamicArray<MeshDrawList> m_draw_lists{};

    public:
        void reset() {
            m_cached_commands.invalidate();
            m_pipeline = nullptr;
            m_draw_lists.clear();
        }

        void prepare(const GfxGraphicsPipelineHandle& pipeline,
                     const Size_t view_count,
                     const UInt64 /*content_revision*/ = 0) {
            if (m_pipeline.get() != pipeline.get()) {
                m_cached_commands.invalidate();
                m_pipeline = pipeline;
            }
            m_draw_lists.resize(view_count);
        }

        MeshDrawList& beginView(const Size_t view_index) {
            DO_ASSERT(view_index < m_draw_lists.size(), "MeshPassCommandStorage view index out of range");
            auto& draw_list = m_draw_lists[view_index];
            draw_list.reset();
            draw_list.cached_commands = &m_cached_commands.getCommands();
            return draw_list;
        }

        void sort() {
            for (auto& draw_list : m_draw_lists) {
                draw_list.sort();
            }
        }

        void materializeSources() {
            for (auto& draw_list : m_draw_lists) {
                draw_list.materializeSources(m_cached_commands);
            }
        }

        void mergeThreadLocal(const Size_t view_index,
                              const DynamicArray<MeshPassThreadLocalCommandStorage>& local_storages) {
            DO_ASSERT(view_index < m_draw_lists.size(), "MeshPassCommandStorage merge view index out of range");
            if (view_index >= m_draw_lists.size()) {
                return;
            }
            auto& destination = m_draw_lists[view_index].sources;
            for (const auto& local : local_storages) {
                destination.insert(destination.end(), local.sources.begin(), local.sources.end());
            }
        }

        [[nodiscard]] MeshDrawCommandCache& getCache() { return m_cached_commands; }
        [[nodiscard]] const MeshDrawCommandCache& getCache() const { return m_cached_commands; }
        [[nodiscard]] DynamicArray<MeshDrawList>& getDrawLists() { return m_draw_lists; }
        [[nodiscard]] const DynamicArray<MeshDrawList>& getDrawLists() const { return m_draw_lists; }

        [[nodiscard]] DynamicArray<MeshDrawCommandSource>& getSources(const Size_t view_index) {
            DO_ASSERT(view_index < m_draw_lists.size(), "MeshPassCommandStorage source view index out of range");
            return m_draw_lists[view_index].sources;
        }
        [[nodiscard]] const DynamicArray<MeshDrawCommandSource>& getSources(const Size_t view_index) const {
            DO_ASSERT(view_index < m_draw_lists.size(), "MeshPassCommandStorage source view index out of range");
            return m_draw_lists[view_index].sources;
        }

        [[nodiscard]] const DynamicArray<MeshDrawGpuBucket>& getGpuBuckets(const Size_t view_index) const {
            static const DynamicArray<MeshDrawGpuBucket> empty_buckets{};
            return view_index < m_draw_lists.size() ? m_draw_lists[view_index].gpu_buckets : empty_buckets;
        }

    };

} // namespace dodoe
