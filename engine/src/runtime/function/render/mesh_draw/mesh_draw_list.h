// do@Redlive

#pragma once

#include "dopch.h"

#include "mesh_draw_command.h"
#include "mesh_draw_types.h"
#include "cached_mesh_draw_command.h"

namespace dodoe {

    struct MeshDrawGpuBucket {
        MeshDrawSortKey key{};
        MeshDrawCommand command{};
        UInt32 first_source{0};
        UInt32 source_count{0};
    };

    inline Bool IsSamePrimitiveShaderData(const PrimitiveMeshDrawShaderData& lhs,
                                          const PrimitiveMeshDrawShaderData& rhs) {
        return lhs.draw_data.x == rhs.draw_data.x && lhs.draw_data.y == rhs.draw_data.y &&
            lhs.draw_data.z == rhs.draw_data.z && lhs.draw_data.w == rhs.draw_data.w &&
            lhs.material_data.x == rhs.material_data.x && lhs.material_data.y == rhs.material_data.y &&
            lhs.material_data.z == rhs.material_data.z && lhs.material_data.w == rhs.material_data.w;
    }

    inline void MergeOpaqueMeshDrawSources(DynamicArray<MeshDrawCommandSource>& sources) {
        if (sources.size() < 2) {
            return;
        }

        DynamicArray<MeshDrawCommandSource> merged;
        merged.reserve(sources.size());
        for (auto& source : sources) {
            if (!merged.empty()) {
                auto& previous = merged.back();
                const auto& previous_args = previous.command.getDrawArguments();
                const auto& current_args = source.command.getDrawArguments();
                const UInt64 expected_offset = previous.instance.instance_offset +
                    static_cast<UInt64>(previous_args.instanceCount) * sizeof(InstanceSceneData);
                const Bool same_index_range = previous_args.vertexCount == current_args.vertexCount &&
                    previous_args.startIndexLocation == current_args.startIndexLocation;
                const Bool contiguous_index_range = previous_args.startIndexLocation +
                    previous_args.vertexCount == current_args.startIndexLocation;
                const Bool canMergeInstances = same_index_range &&
                    source.instance.instance_offset == expected_offset;
                const Bool canMergeIndexRange = contiguous_index_range &&
                    source.instance.instance_offset == previous.instance.instance_offset &&
                    previous_args.instanceCount == current_args.instanceCount;
                if (previous.command.canMergeWith(source.command) &&
                    IsSamePrimitiveShaderData(previous.shader_data, source.shader_data) &&
                    (canMergeInstances || canMergeIndexRange)) {
                    previous.command.mergeDrawArguments(source.command);
                    if (previous.lifetime != source.lifetime) {
                        previous.lifetime = CommandLifetime::Frame;
                    }
                    continue;
                }
            }
            merged.push_back(std::move(source));
        }

        sources = std::move(merged);
        for (UInt32 index = 0; index < sources.size(); ++index) {
            sources[index].instance.cmd_index = index;
        }
    }

    struct MeshDrawList {
        DynamicArray<MeshDrawCommandSource> sources;
        DynamicArray<MeshDrawGpuBucket> gpu_buckets;
        DynamicArray<MeshDrawInstance> cached_instances;
        DynamicArray<MeshDrawInstance> dynamic_instances;
        DynamicArray<MeshDrawCommand> frame_commands;
        DynamicArray<PrimitiveMeshDrawShaderData> cached_shader_data;
        DynamicArray<PrimitiveMeshDrawShaderData> dynamic_shader_data;
        const DynamicArray<MeshDrawCommand>* cached_commands{nullptr};

        void materializeSources(MeshDrawCommandCache& cache) {
            cached_instances.clear();
            dynamic_instances.clear();
            frame_commands.clear();
            cached_shader_data.clear();
            dynamic_shader_data.clear();
            cached_commands = &cache.getCommands();

            for (auto& source : sources) {
                if (source.lifetime == CommandLifetime::Cached && source.has_cache_key) {
                    source.instance.cmd_index = cache.findOrCreate(
                        source.cache_key, MeshDrawCommand(source.command));
                    source.instance.shader_data_index = static_cast<UInt32>(cached_shader_data.size());
                    cached_shader_data.push_back(source.shader_data);
                    cached_instances.push_back(source.instance);
                } else {
                    source.instance.cmd_index = static_cast<UInt32>(frame_commands.size());
                    frame_commands.push_back(source.command);
                    source.instance.shader_data_index = static_cast<UInt32>(dynamic_shader_data.size());
                    dynamic_shader_data.push_back(source.shader_data);
                    dynamic_instances.push_back(source.instance);
                }
            }
        }

        void reset() {
            sources.clear();
            gpu_buckets.clear();
            cached_instances.clear();
            dynamic_instances.clear();
            frame_commands.clear();
            cached_shader_data.clear();
            dynamic_shader_data.clear();
            cached_commands = nullptr;
        }

        void sort() {
            if (sources.size() > 1) {
                std::stable_sort(sources.begin(), sources.end(),
                    [](const MeshDrawCommandSource& lhs, const MeshDrawCommandSource& rhs) {
                        const Bool transparent = lhs.command.getPassType() == MeshPassType::Transparent;
                        if (transparent && lhs.instance.sort_depth != rhs.instance.sort_depth) {
                            return lhs.instance.sort_depth > rhs.instance.sort_depth;
                        }
                        if (transparent && lhs.instance.depth_bucket != rhs.instance.depth_bucket) {
                            return lhs.instance.depth_bucket > rhs.instance.depth_bucket;
                        }
                        return lhs.command.getSortKey(transparent ? lhs.instance.depth_bucket : 0) <
                            rhs.command.getSortKey(transparent ? rhs.instance.depth_bucket : 0);
                    });
                for (UInt32 source_index = 0; source_index < sources.size(); ++source_index) {
                    sources[source_index].instance.cmd_index = source_index;
                }
                if (sources.front().command.getPassType() == MeshPassType::Opaque) {
                    MergeOpaqueMeshDrawSources(sources);
                }
            }
            gpu_buckets.clear();
            for (UInt32 source_index = 0; source_index < sources.size(); ++source_index) {
                const auto key = sources[source_index].command.getBucketKey();
                if (gpu_buckets.empty() || !(gpu_buckets.back().key == key)) {
                    gpu_buckets.push_back(MeshDrawGpuBucket{
                        key, sources[source_index].command, source_index, 1});
                } else {
                    ++gpu_buckets.back().source_count;
                }
            }
        }
    };


} // namespace dodoe
