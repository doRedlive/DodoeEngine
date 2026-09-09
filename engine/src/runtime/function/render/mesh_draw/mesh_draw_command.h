// do@Redlive

#pragma once

#include "dopch.h"
#include "mesh_pass_type.h"
#include "mesh_draw_types.h"
#include "runtime/function/graphics/gfx.h"
#include "runtime/function/render/shader/shader_parameter.h"

namespace dodoe {

    enum class CommandLifetime : UInt8 {
        Cached,
        Frame
    };

    struct MeshDrawSortKey {
        Size_t pipeline{0};
        Size_t material{0};
        Size_t binding_set{0};
        Size_t vertex_buffer{0};
        Size_t index_buffer{0};
        UInt32 index_count{0};
        UInt32 start_index{0};
        Int32 base_vertex{0};
        UInt32 depth_bucket{0};

        [[nodiscard]] Bool operator==(const MeshDrawSortKey& other) const {
            return pipeline == other.pipeline && material == other.material &&
                binding_set == other.binding_set && vertex_buffer == other.vertex_buffer && index_buffer == other.index_buffer &&
                index_count == other.index_count && start_index == other.start_index &&
                base_vertex == other.base_vertex &&
                depth_bucket == other.depth_bucket;
        }

        [[nodiscard]] Bool operator<(const MeshDrawSortKey& other) const {
            if (pipeline != other.pipeline) return pipeline < other.pipeline;
            if (material != other.material) return material < other.material;
            if (binding_set != other.binding_set) return binding_set < other.binding_set;
            if (vertex_buffer != other.vertex_buffer) return vertex_buffer < other.vertex_buffer;
            if (index_buffer != other.index_buffer) return index_buffer < other.index_buffer;
            if (index_count != other.index_count) return index_count < other.index_count;
            if (start_index != other.start_index) return start_index < other.start_index;
            if (base_vertex != other.base_vertex) return base_vertex < other.base_vertex;
            return depth_bucket < other.depth_bucket;
        }
    };

    struct MeshDrawCommandCacheKey {
        Size_t batch_hash{0};
        Size_t material_hash{0};
        UInt64 material_revision{0};
        Size_t pass_hash{0};
        Size_t pipeline_hash{0};

        Bool operator==(const MeshDrawCommandCacheKey& other) const {
            return batch_hash == other.batch_hash &&
                   material_hash == other.material_hash &&
                   material_revision == other.material_revision &&
                   pass_hash == other.pass_hash &&
                   pipeline_hash == other.pipeline_hash;
        }
    };

    class MeshDrawCommand {
        MeshPassType m_pass_type{MeshPassType::Opaque};
        GfxGraphicsPipelineHandle m_pipeline{};
        StaticArray<GfxBindingSetHandle, ShaderParameterBinder::kShaderParameterSetCount> m_binding_sets{};
        DynamicArray<GfxVertexBufferBinding> m_vertex_bindings{};
        GfxIndexBufferBinding m_index_binding{};
        GfxDrawArguments m_draw_args{};
        Size_t m_material_sort_id{0};

    public:
        [[nodiscard]] MeshPassType getPassType() const { return m_pass_type; }
        [[nodiscard]] const GfxGraphicsPipelineHandle& getPipeline() const { return m_pipeline; }
        [[nodiscard]] const StaticArray<GfxBindingSetHandle, ShaderParameterBinder::kShaderParameterSetCount>& getBindingSets() const { return m_binding_sets; }
        [[nodiscard]] const DynamicArray<GfxVertexBufferBinding>& getVertexBindings() const { return m_vertex_bindings; }
        [[nodiscard]] const GfxIndexBufferBinding& getIndexBinding() const { return m_index_binding; }
        [[nodiscard]] const GfxDrawArguments& getDrawArguments() const { return m_draw_args; }
        [[nodiscard]] Size_t getMaterialSortId() const { return m_material_sort_id; }

        [[nodiscard]] Bool canMergeWith(const MeshDrawCommand& other) const {
            if (m_pass_type != MeshPassType::Opaque || other.m_pass_type != MeshPassType::Opaque ||
                m_pipeline.get() != other.m_pipeline.get() ||
                m_vertex_bindings.size() != other.m_vertex_bindings.size() ||
                m_index_binding != other.m_index_binding ||
                m_material_sort_id != other.m_material_sort_id) {
                return false;
            }
            for (Size_t i = 0; i < m_binding_sets.size(); ++i) {
                if (m_binding_sets[i].get() != other.m_binding_sets[i].get()) {
                    return false;
                }
            }
            for (Size_t i = 0; i < m_vertex_bindings.size(); ++i) {
                if (m_vertex_bindings[i] != other.m_vertex_bindings[i]) {
                    return false;
                }
            }

            const auto& lhs = m_draw_args;
            const auto& rhs = other.m_draw_args;
            const Bool same_index_range = lhs.vertexCount == rhs.vertexCount &&
                lhs.startIndexLocation == rhs.startIndexLocation &&
                lhs.startVertexLocation == rhs.startVertexLocation;
            const Bool contiguous_index_range = lhs.startIndexLocation + lhs.vertexCount ==
                rhs.startIndexLocation && lhs.startVertexLocation == rhs.startVertexLocation &&
                lhs.instanceCount == rhs.instanceCount;
            return same_index_range || contiguous_index_range;
        }

        void mergeDrawArguments(const MeshDrawCommand& other) {
            if (!canMergeWith(other)) {
                return;
            }
            if (m_draw_args.vertexCount == other.m_draw_args.vertexCount &&
                m_draw_args.startIndexLocation == other.m_draw_args.startIndexLocation) {
                m_draw_args.setInstanceCount(m_draw_args.instanceCount + other.m_draw_args.instanceCount);
            } else {
                m_draw_args.setVertexCount(m_draw_args.vertexCount + other.m_draw_args.vertexCount);
            }
        }

        [[nodiscard]] MeshDrawSortKey getSortKey(const UInt32 depth_bucket = 0) const {
            Size_t binding_set_hash = 0;
            for (const auto& binding_set : m_binding_sets) {
                binding_set_hash ^= reinterpret_cast<Size_t>(binding_set.get()) +
                    (binding_set_hash << 6) + (binding_set_hash >> 2);
            }
            return MeshDrawSortKey{
                reinterpret_cast<Size_t>(m_pipeline.get()),
                m_material_sort_id,
                binding_set_hash,
                m_vertex_bindings.empty() ? 0 : reinterpret_cast<Size_t>(m_vertex_bindings.front().buffer),
                reinterpret_cast<Size_t>(m_index_binding.buffer),
                m_draw_args.vertexCount,
                m_draw_args.startIndexLocation,
                static_cast<Int32>(m_draw_args.startVertexLocation),
                depth_bucket};
        }

        [[nodiscard]] MeshDrawSortKey getBucketKey() const { return getSortKey(0); }

        void setPassType(const MeshPassType pass_type) { m_pass_type = pass_type; }
        void setPipeline(GfxGraphicsPipelineHandle pipeline) { m_pipeline = std::move(pipeline); }
        void setBindingSet(const ShaderParameterSet set, const GfxBindingSetHandle& binding_set) {
            m_binding_sets[static_cast<Size_t>(set)] = binding_set;
        }
        void addVertexBinding(const GfxVertexBufferBinding& binding) { m_vertex_bindings.push_back(binding); }
        void setIndexBinding(const GfxIndexBufferBinding& binding) { m_index_binding = binding; }
        void setDrawArguments(const GfxDrawArguments& args) { m_draw_args = args; }
        void setMaterialSortId(const Size_t material_sort_id) { m_material_sort_id = material_sort_id; }
    };

    struct MeshDrawInstance {
        UInt32 cmd_index{0};
        UInt32 shader_data_index{std::numeric_limits<UInt32>::max()};
        UInt64 instance_offset{0};
        Float sort_depth{0.0f};
        UInt32 depth_bucket{0};

        [[nodiscard]] Bool hasShaderData() const {
            return shader_data_index != std::numeric_limits<UInt32>::max();
        }
    };

    struct MeshDrawCommandSource {
        MeshDrawCommand command{};
        MeshDrawInstance instance{};
        PrimitiveMeshDrawShaderData shader_data{};
        CommandLifetime lifetime{CommandLifetime::Frame};
        MeshDrawCommandCacheKey cache_key{};
        Bool has_cache_key{false};
        UInt8 cascade_mask{0xFF};
    };

} // dodoe

template <>
struct std::hash<dodoe::MeshDrawCommandCacheKey> {
    dodoe::Size_t operator()(const dodoe::MeshDrawCommandCacheKey& k) const {
        return k.batch_hash ^ (k.material_hash << 1) ^
            static_cast<dodoe::Size_t>(k.material_revision << 2) ^
            (k.pass_hash << 3) ^ (k.pipeline_hash << 5);
    }
};
