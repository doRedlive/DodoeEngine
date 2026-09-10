// do@Redlive

#include "debug_imgui.h"

#if defined(DODOE_DEBUG_ENABLED) && defined(DODOE_IMGUI_ENABLED)

#include "imgui/imgui.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/memory/memory.h"
#include "runtime/core/meta/component_db.h"
#include "runtime/function/audio/audio_clip.h"
#include "runtime/function/physics/physics_world.h"
#include "runtime/function/render/render_frame/frame_staging_allocator.h"
#include "runtime/function/render/render_graph/render_graph_debug.h"
#include "runtime/function/render/render_graph/render_graph_transient_pool.h"
#include "runtime/function/render/mesh_draw/mesh_pass_registry.h"
#include "runtime/function/script/script_system.h"
#include "runtime/function/time/time_system.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/mesh_renderer_component.h"
#include "runtime/function/world/world.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/material/material_system.h"
#include "runtime/resource/parser/texture_blob.h"

#include <mimalloc.h>

#ifdef DO_PLATFORM_WINDOWS
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

#include <cstdint>
#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <thread>

namespace dodoe {

    namespace {
        using UUIDSet = UnorderedSet<UUID>;

        bool DrawJsonValue(Json& value, bool read_only);

        bool DrawJsonMember(const String& name, Json& value, bool read_only) {
            ImGui::PushID(name.c_str());
            const bool immutable_id = read_only && name == "id";
            bool changed = false;

            if (value.is_object() || value.is_array()) {
                const bool open = ImGui::TreeNodeEx("##value", ImGuiTreeNodeFlags_SpanAvailWidth,
                                                    "%s", name.c_str());
                if (open) {
                    changed = DrawJsonValue(value, read_only);
                    ImGui::TreePop();
                }
            }
            else {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(name.c_str());
                ImGui::SameLine();
                if (immutable_id) {
                    ImGui::TextDisabled("%s", value.dump().c_str());
                }
                else {
                    changed = DrawJsonValue(value, false);
                }
            }

            ImGui::PopID();
            return changed;
        }

        bool DrawJsonValue(Json& value, bool read_only) {
            if (value.is_boolean()) {
                bool v = value.get<bool>();
                if (read_only) {
                    ImGui::TextDisabled("%s", v ? "true" : "false");
                    return false;
                }
                if (ImGui::Checkbox("##value", &v)) {
                    value = v;
                    return true;
                }
                return false;
            }
            if (value.is_number_integer()) {
                ImS64 v = value.get<ImS64>();
                if (read_only) {
                    ImGui::TextDisabled("%lld", static_cast<long long>(v));
                    return false;
                }
                if (ImGui::InputScalar("##value", ImGuiDataType_S64, &v)) {
                    value = static_cast<int64_t>(v);
                    return true;
                }
                return false;
            }
            if (value.is_number_unsigned()) {
                ImU64 v = value.get<ImU64>();
                if (read_only) {
                    ImGui::TextDisabled("%llu", static_cast<unsigned long long>(v));
                    return false;
                }
                if (ImGui::InputScalar("##value", ImGuiDataType_U64, &v)) {
                    value = static_cast<uint64_t>(v);
                    return true;
                }
                return false;
            }
            if (value.is_number_float()) {
                double v = value.get<double>();
                if (read_only) {
                    ImGui::TextDisabled("%.6g", v);
                    return false;
                }
                if (ImGui::InputDouble("##value", &v, 0.1, 1.0, "%.6g")) {
                    value = v;
                    return true;
                }
                return false;
            }
            if (value.is_string()) {
                std::array<char, 1024> buffer{};
                const std::string current = value.get<std::string>();
                std::snprintf(buffer.data(), buffer.size(), "%s", current.c_str());
                if (read_only) {
                    ImGui::TextDisabled("%s", current.c_str());
                    return false;
                }
                if (ImGui::InputText("##value", buffer.data(), buffer.size())) {
                    value = std::string(buffer.data());
                    return true;
                }
                return false;
            }
            if (value.is_object()) {
                bool changed = false;
                for (auto& [name, child] : value.items()) {
                    changed |= DrawJsonMember(String(name.c_str()), child, read_only);
                }
                return changed;
            }
            if (value.is_array()) {
                bool changed = false;
                for (std::size_t i = 0; i < value.size(); ++i) {
                    ImGui::PushID(static_cast<int>(i));
                    if (value[i].is_object() || value[i].is_array()) {
                        const bool open = ImGui::TreeNodeEx("##array", ImGuiTreeNodeFlags_SpanAvailWidth,
                                                            "[%zu]", i);
                        if (open) {
                            changed |= DrawJsonValue(value[i], read_only);
                            ImGui::TreePop();
                        }
                    }
                    else {
                        ImGui::Text("[%zu]", i);
                        ImGui::SameLine();
                        changed |= DrawJsonValue(value[i], read_only);
                    }
                    ImGui::PopID();
                }
                return changed;
            }

            ImGui::TextDisabled("null");
            return false;
        }

        void DrawNativeComponent(Entity& entity, const ComponentDB::Entry& entry) {
            ImGui::PushID(static_cast<int>(entry.type));
            if (!ImGui::CollapsingHeader(entry.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::PopID();
                return;
            }

            void* component = entry.get(entity);
            if (!component || !entry.writeJson) {
                ImGui::TextDisabled("No editable fields");
                ImGui::PopID();
                return;
            }

            Json fields = entry.writeJson(component);
            const bool changed = DrawJsonValue(fields, entry.name == "IDComponent");
            if (changed && entry.readJson && entry.readJson(component, fields) && entry.markDirty) {
                entry.markDirty(entity);
            }
            ImGui::PopID();
        }

        void DrawManagedComponents(Entity entity) {
            auto* script_system = GetScriptSystem();
            auto* runtime = script_system ? script_system->getScriptRuntime() : nullptr;
            if (!runtime) return;

            static uint64_t s_cached_uuid = 0;
            static DynamicArray<Pair<String, Json>> s_cached_components;
            static Bool s_cache_valid = false;

            const uint64_t entity_uuid = static_cast<uint64_t>(entity.uuid());
            if (entity_uuid != s_cached_uuid || !s_cache_valid) {
                s_cached_uuid = entity_uuid;
                s_cache_valid = runtime->getEntityManagedComponentFields(entity_uuid, s_cached_components);
            }

            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("Managed");
            ImGui::SameLine();
            if (ImGui::SmallButton("Refresh##managed")) {
                s_cache_valid = runtime->getEntityManagedComponentFields(entity_uuid, s_cached_components);
            }

            if (!s_cache_valid) return;

            for (auto& [type_name, fields] : s_cached_components) {
                ImGui::PushID(type_name.c_str());
                String title = type_name;
                const auto dot = title.find_last_of('.');
                if (dot != String::npos) {
                    title = title.substr(dot + 1);
                }
                if (ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (DrawJsonValue(fields, false)) {
                        runtime->setEntityManagedComponentFields(entity_uuid, type_name, fields);
                    }
                }
                ImGui::PopID();
            }
        }

        void RenderWorldStateControls() {
            World* world = GetWorld();
            if (!world) return;

            const WorldState state = world->getState();

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("State:");
            ImGui::SameLine();
            switch (state) {
                case WorldState::Runtime:
                    ImGui::TextColored(ImVec4(0.35f, 0.9f, 0.45f, 1.0f), "Playing");
                break;
                case WorldState::Pause:
                    ImGui::TextColored(ImVec4(0.95f, 0.8f, 0.25f, 1.0f), "Paused");
                break;
                case WorldState::Simulation:
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Stopped");
                break;
            }

            ImGui::BeginDisabled(state == WorldState::Runtime);
            if (ImGui::Button("Play")) {
                world->setState(WorldState::Runtime);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(state != WorldState::Runtime);
            if (ImGui::Button("Pause")) {
                world->setState(WorldState::Pause);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(state == WorldState::Simulation);
            if (ImGui::Button("Stop")) {
                world->setState(WorldState::Simulation);
            }
            ImGui::EndDisabled();

            TimeSystem* time_system = GetTimeSystem();
            if (time_system) {
                float time_scale = time_system->getTimeScale();
                if (ImGui::SliderFloat("Time Scale", &time_scale, 0.0f, 2.0f, "%.2fx")) {
                    time_system->setTimeScale(time_scale);
                }
            }
        }

        String FormatBytes(Size_t bytes) {
            static constexpr const char* kUnits[] = {"B", "KB", "MB", "GB", "TB"};
            static constexpr int kUnitCount = static_cast<int>(std::size(kUnits));
            double value = static_cast<double>(bytes);
            int unit = 0;
            while (value >= 1024.0 && unit < kUnitCount - 1) {
                value /= 1024.0;
                ++unit;
            }
            std::array<char, 48> buffer{};
            std::snprintf(buffer.data(), buffer.size(), "%.2f %s", value, kUnits[unit]);
            return String(buffer.data());
        }

#ifdef DO_PLATFORM_WINDOWS
        UInt64 QueryProcessHeapInUse() {
            UInt64 in_use = 0;
            DWORD heap_count = GetProcessHeaps(0, nullptr);
            if (heap_count > 0 && heap_count < 1024) {
                std::array<HANDLE, 1024> heaps{};
                if (GetProcessHeaps(heap_count, heaps.data()) == heap_count) {
                    for (DWORD i = 0; i < heap_count; ++i) {
                        HeapLock(heaps[i]);
                        PROCESS_HEAP_ENTRY entry{};
                        while (HeapWalk(heaps[i], &entry)) {
                            if (entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) {
                                in_use += entry.cbData;
                            }
                        }
                        HeapUnlock(heaps[i]);
                    }
                }
            }
            return in_use;
        }

        void RenderProcessMemorySection() {
            PROCESS_MEMORY_COUNTERS_EX pmc{};
            pmc.cb = sizeof(pmc);
            if (!K32GetProcessMemoryInfo(GetCurrentProcess(),
                                         reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                                         sizeof(pmc))) {
                ImGui::TextDisabled("Failed to query process memory");
                return;
            }

            static UInt64 s_last_page_faults = 0;
            static UInt64 s_page_fault_delta = 0;
            const UInt64 page_faults = pmc.PageFaultCount;
            s_page_fault_delta = (s_last_page_faults != 0) ? (page_faults - s_last_page_faults) : 0;
            s_last_page_faults = page_faults;

            MEMORYSTATUSEX mem_status{};
            mem_status.dwLength = sizeof(mem_status);
            GlobalMemoryStatusEx(&mem_status);
            const UInt64 commit_charge = mem_status.ullTotalPageFile - mem_status.ullAvailPageFile;

            ImGui::Text("Working Set:      %s", FormatBytes(pmc.WorkingSetSize).c_str());
            ImGui::Text("Peak Working Set: %s", FormatBytes(pmc.PeakWorkingSetSize).c_str());
            ImGui::Text("Private Bytes:    %s", FormatBytes(pmc.PrivateUsage).c_str());
            ImGui::Text("Pagefile Usage:   %s", FormatBytes(pmc.PagefileUsage).c_str());
            ImGui::Text("Page Faults:      %llu (+%llu)",
                        static_cast<unsigned long long>(page_faults),
                        static_cast<unsigned long long>(s_page_fault_delta));
            ImGui::Text("System Commit:   %s / %s",
                        FormatBytes(commit_charge).c_str(),
                        FormatBytes(mem_status.ullTotalPageFile).c_str());

            static constexpr float kHeapWalkInterval = 0.5f;
            static float s_heap_walk_elapsed = 0.0f;
            static UInt64 s_heap_in_use = 0;
            s_heap_walk_elapsed += ImGui::GetIO().DeltaTime;
            if (s_heap_walk_elapsed >= kHeapWalkInterval) {
                s_heap_walk_elapsed = 0.0f;
                s_heap_in_use = QueryProcessHeapInUse();
            }
            ImGui::Text("Process Heaps In-Use: %s", FormatBytes(s_heap_in_use).c_str());
        }
#else
        void RenderProcessMemorySection() {
            ImGui::TextDisabled("Process memory stats available on Windows only");
        }
#endif

        struct MimallocStats {
            Size_t live_allocated = 0;
            Size_t committed = 0;
            Size_t peak_commit = 0;
            Size_t reserved = 0;
            Size_t area_count = 0;
            Size_t heap_committed = 0;
        };

        struct MimallocVisitArg {
            MimallocStats stats;
            const void* areas[256];
        };

        bool MimallocLiveVisit(const mi_heap_t*, const mi_heap_area_t*, void*, size_t block_size, void* arg) {
            static_cast<MimallocVisitArg*>(arg)->stats.live_allocated += block_size;
            return true;
        }

        bool MimallocAreaVisit(const mi_heap_t*, const mi_heap_area_t* area, void*, size_t, void* arg) {
            MimallocVisitArg* a = static_cast<MimallocVisitArg*>(arg);
            for (Size_t i = 0; i < a->stats.area_count; ++i) {
                if (a->areas[i] == area) {
                    return true;
                }
            }
            if (a->stats.area_count < 256) {
                a->areas[a->stats.area_count++] = area;
                a->stats.reserved += area->reserved;
                a->stats.heap_committed += area->committed;
            }
            return true;
        }

        void MiStatsCaptureFn(const char* msg, void* arg) {
            static_cast<String*>(arg)->append(msg);
        }

        MimallocStats QueryMimallocStats() {
            MimallocStats stats;

            MimallocVisitArg live{};
            mi_heap_visit_blocks(mi_heap_get_default(), false, &MimallocLiveVisit, &live);
            mi_heap_visit_blocks(mi_heap_get_backing(), false, &MimallocLiveVisit, &live);
            stats.live_allocated = live.stats.live_allocated;

            MimallocVisitArg areas{};
            mi_heap_visit_blocks(mi_heap_get_default(), true, &MimallocAreaVisit, &areas);
            mi_heap_visit_blocks(mi_heap_get_backing(), true, &MimallocAreaVisit, &areas);
            stats.reserved = areas.stats.reserved;
            stats.area_count = areas.stats.area_count;
            stats.heap_committed = areas.stats.heap_committed;

            size_t elapsed_msecs = 0, user_msecs = 0, system_msecs = 0;
            size_t current_rss = 0, peak_rss = 0, current_commit = 0, peak_commit = 0, page_faults = 0;
            mi_process_info(&elapsed_msecs, &user_msecs, &system_msecs,
                            &current_rss, &peak_rss, &current_commit, &peak_commit, &page_faults);
            stats.committed = current_commit;
            stats.peak_commit = peak_commit;
            return stats;
        }

        void RenderMimallocSection() {
            const MimallocStats stats = QueryMimallocStats();

            ImGui::Text("Live Allocated:  %s", FormatBytes(stats.live_allocated).c_str());
            ImGui::Text("Heap Committed:  %s", FormatBytes(stats.heap_committed).c_str());
            ImGui::Text("OS Commit:       %s", FormatBytes(stats.committed).c_str());
            ImGui::Text("Peak Commit:     %s", FormatBytes(stats.peak_commit).c_str());
            ImGui::Text("Reserved:        %s", FormatBytes(stats.reserved).c_str());
            ImGui::Text("Arena Areas:     %llu", static_cast<unsigned long long>(stats.area_count));

            if (ImGui::Button("Force Collect")) {
                mi_collect(true);
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset Stats")) {
                mi_stats_reset();
            }
        }

        void RenderEngineMemorySection() {
            static constexpr const char* kTierNames[] = {"Persistent", "Frame", "Scratch"};
            static constexpr const char* kTagNames[] = {"Object", "RenderCmd", "Texture", "Resource", "Misc"};
            static constexpr int kTierCount = static_cast<int>(AllocTier::Count);
            static constexpr int kTagCount = static_cast<int>(AllocTag::Count);

            if (ImGui::BeginTable("engine_memory", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Tier");
                ImGui::TableSetupColumn("Tag");
                ImGui::TableSetupColumn("Current");
                ImGui::TableSetupColumn("Peak");
                ImGui::TableSetupColumn("Allocs");
                ImGui::TableSetupColumn("Frees");
                ImGui::TableHeadersRow();

                Size_t total_current = 0;
                Size_t total_peak = 0;
                for (int t = 0; t < kTierCount; ++t) {
                    for (int g = 0; g < kTagCount; ++g) {
                        const TierStats& stats = Memory::GetStats(static_cast<AllocTier>(t), static_cast<AllocTag>(g));
                        const Size_t current = stats.current_bytes.load(std::memory_order_relaxed);
                        const Size_t peak = stats.peak_bytes.load(std::memory_order_relaxed);
                        total_current += current;
                        total_peak += peak;

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(kTierNames[t]);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(kTagNames[g]);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%s", FormatBytes(current).c_str());
                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%s", FormatBytes(peak).c_str());
                        ImGui::TableSetColumnIndex(4);
                        ImGui::Text("%llu", static_cast<unsigned long long>(
                            stats.alloc_count.load(std::memory_order_relaxed)));
                        ImGui::TableSetColumnIndex(5);
                        ImGui::Text("%llu", static_cast<unsigned long long>(
                            stats.dealloc_count.load(std::memory_order_relaxed)));
                    }
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Total");
                ImGui::TableSetColumnIndex(2);
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s", FormatBytes(total_current).c_str());
                ImGui::TableSetColumnIndex(3);
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s", FormatBytes(total_peak).c_str());
                ImGui::EndTable();
            }

            const ThreadAllocatorStats thread_stats = Memory::GetThreadAllocatorStats();
            ImGui::Separator();
            ImGui::TextUnformatted("Thread Allocators");
            ImGui::Text("Allocators:        %llu", static_cast<unsigned long long>(thread_stats.allocator_count));
            ImGui::Text("Frame Used:        %s", FormatBytes(thread_stats.frame_used_bytes).c_str());
            ImGui::Text("Frame Reserved:    %s (%llu blocks)",
                        FormatBytes(thread_stats.frame_reserved_bytes).c_str(),
                        static_cast<unsigned long long>(thread_stats.frame_block_count));
            ImGui::Text("Scratch Used:      %s", FormatBytes(thread_stats.scratch_used_bytes).c_str());
            ImGui::Text("Scratch Reserved:  %s (%llu blocks)",
                        FormatBytes(thread_stats.scratch_reserved_bytes).c_str(),
                        static_cast<unsigned long long>(thread_stats.scratch_block_count));
            ImGui::TextDisabled("Tier current bytes are logical allocations; reserved bytes show allocator backing capacity");

            ImGui::Separator();
            ImGui::TextUnformatted("Pools");
            if (ImGui::BeginTable("engine_pools", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Tag");
                ImGui::TableSetupColumn("Block");
                ImGui::TableSetupColumn("Align");
                ImGui::TableSetupColumn("Chunks");
                ImGui::TableSetupColumn("Reserved");
                ImGui::TableSetupColumn("Used");
                ImGui::TableSetupColumn("Free");
                ImGui::TableHeadersRow();

                for (int g = 0; g < kTagCount; ++g) {
                    const PoolRuntimeStats pool_stats = Memory::GetPoolRuntimeStats(static_cast<AllocTag>(g));
                    if (!pool_stats.registered) {
                        continue;
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(kTagNames[g]);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%llu", static_cast<unsigned long long>(pool_stats.block_size));
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%llu", static_cast<unsigned long long>(pool_stats.block_align));
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%llu", static_cast<unsigned long long>(pool_stats.chunk_count));
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%s", FormatBytes(pool_stats.chunk_bytes).c_str());
                    ImGui::TableSetColumnIndex(5);
                    ImGui::Text("%llu", static_cast<unsigned long long>(pool_stats.used_blocks));
                    ImGui::TableSetColumnIndex(6);
                    ImGui::Text("%llu", static_cast<unsigned long long>(pool_stats.free_blocks));
                }
                ImGui::EndTable();
            }

            if (ImGui::Button("Reset Stats")) {
                Memory::ResetAllStats();
            }
            ImGui::SameLine();
            if (ImGui::Button("Dump All")) {
                Memory::DumpAll();
            }
        }

        void RenderFrameStagingSection() {
            const auto stats = FrameStagingAllocator::QueryGlobalStats();
            ImGui::Text("Allocators:   %llu", static_cast<unsigned long long>(stats.allocator_count));
            ImGui::Text("Used Bytes:   %s", FormatBytes(static_cast<Size_t>(stats.used_bytes)).c_str());
            ImGui::Text("Total Bytes:  %s", FormatBytes(static_cast<Size_t>(stats.total_bytes)).c_str());
            ImGui::Text("Peak Used:    %s", FormatBytes(static_cast<Size_t>(stats.peak_used_bytes)).c_str());
            ImGui::Text("Stalls:       %llu", static_cast<unsigned long long>(stats.stall_count));
            ImGui::Text("Overflows:    %llu", static_cast<unsigned long long>(stats.overflow_count));
        }

        void RenderAudioSection() {
            const auto stats = AudioClip::QueryMemoryStats();
            ImGui::Text("Clips:        %llu", static_cast<unsigned long long>(stats.clip_count));
            ImGui::Text("PCM Bytes:    %s", FormatBytes(static_cast<Size_t>(stats.pcm_bytes)).c_str());
            ImGui::Text("Peak PCM:     %s", FormatBytes(static_cast<Size_t>(stats.peak_pcm_bytes)).c_str());
            ImGui::Text("Readers:      %llu", static_cast<unsigned long long>(stats.reader_count));
            ImGui::Text("Reader Bytes: %s", FormatBytes(static_cast<Size_t>(stats.reader_bytes)).c_str());
        }

        void RenderPhysicsSection() {
            const auto stats = PhysicsWorld::QueryMemoryStats();
            ImGui::Text("Worlds:              %llu", static_cast<unsigned long long>(stats.world_count));
            ImGui::Text("Temp Allocator:      %s", FormatBytes(static_cast<Size_t>(stats.temp_allocator_bytes)).c_str());
            ImGui::Text("Peak Temp Allocator: %s", FormatBytes(static_cast<Size_t>(stats.peak_temp_allocator_bytes)).c_str());
            ImGui::Text("Bodies:              %llu", static_cast<unsigned long long>(stats.body_count));
            ImGui::Text("Peak Bodies:         %llu", static_cast<unsigned long long>(stats.peak_body_count));
            ImGui::Text("Shapes:              %llu", static_cast<unsigned long long>(stats.shape_count));
            ImGui::Text("Peak Shapes:         %llu", static_cast<unsigned long long>(stats.peak_shape_count));
        }

        void RenderTextureSection() {
            const auto stats = TextureBlob::QueryMemoryStats();
            ImGui::Text("Blobs:        %llu", static_cast<unsigned long long>(stats.blob_count));
            ImGui::Text("Pixel Bytes:  %s", FormatBytes(static_cast<Size_t>(stats.pixel_bytes)).c_str());
            ImGui::Text("Peak Pixels:  %s", FormatBytes(static_cast<Size_t>(stats.peak_pixel_bytes)).c_str());
        }

        static Bool s_csv_export_ok = false;
        static String s_csv_export_path{};

        String CsvTimestamp() {
            const auto now = std::chrono::system_clock::now();
            const std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::tm local{};
#ifdef DO_PLATFORM_WINDOWS
            localtime_s(&local, &t);
#else
            localtime_r(&t, &local);
#endif
            std::array<char, 32> buffer{};
            std::strftime(buffer.data(), buffer.size(), "%Y%m%d_%H%M%S", &local);
            return String(buffer.data());
        }

        const char* PipelineTypeName(RenderingPipelineType type) {
            switch (type) {
            case RenderingPipelineType::Forward: return "Forward";
            case RenderingPipelineType::ForwardPlus: return "ForwardPlus";
            case RenderingPipelineType::Deferred: return "Deferred";
            case RenderingPipelineType::DeferredPlus: return "DeferredPlus";
            case RenderingPipelineType::Only2D: return "Only2D";
            case RenderingPipelineType::OnlyGUI: return "OnlyGUI";
            default: return "None";
            }
        }

        const char* CullingPathName(CullingPath path) {
            switch (path) {
            case CullingPath::CpuOnly: return "CpuOnly";
            case CullingPath::GpuOnly: return "GpuOnly";
            case CullingPath::CpuThenGpuVerify: return "CpuThenGpuVerify";
            default: return "Unknown";
            }
        }

        String ExportTelemetryJson(const RenderFrameScheduler& scheduler) {
            const String sampled_at = CsvTimestamp();
            FsPath out_path = std::filesystem::current_path()
                / ("render_telemetry_" + sampled_at + ".json").c_str();
            std::ofstream fout(out_path);
            if (!fout.is_open()) {
                return {};
            }
            const auto& collector = scheduler.getTelemetry();
            const Size_t count = collector.getCount();
            fout << "[";
            for (Size_t i = 0; i < count; ++i) {
                fout << collector.previous(static_cast<UInt32>(i)).toJSON();
                if (i + 1 < count) fout << ",";
            }
            fout << "]";
            fout.close();
            return String(out_path.string().c_str());
        }

        void RenderRendererCompare() {
            auto* render_system = GetRenderSystem();
            if (!render_system) return;

            if (!ImGui::CollapsingHeader("Renderer Compare", ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }

            const Bool baseline = RenderSettings::IsEnableBaselineRender();
            ImGui::TextColored(baseline ? ImVec4(1.0f, 0.75f, 0.35f, 1.0f) : ImVec4(0.45f, 0.90f, 0.50f, 1.0f),
                "Path: %s", baseline ? "BaselineRenderer" : "RenderGraph (MeshDraw)");
            ImGui::Text("Backend: %s  Pipeline: %s", RenderSettings::GetRenderBackendApiTypeStr().c_str(),
                PipelineTypeName(RenderSettings::GetRenderingPipelineType()));
            const auto culling = RenderSettings::GetFeatureSettings().culling_path;
            const auto& resolved = RenderSettings::GetResolvedFeatures();
            ImGui::Text("Culling: %s  GpuDriven: %s  Bindless: %s  AsyncCompute: %s",
                CullingPathName(culling),
                resolved.gpu_driven_active ? "on" : "off",
                resolved.bindless_active ? "on" : "off",
                resolved.async_compute_active ? "on" : "off");
            if (!resolved.gpu_driven_fallback_reason.empty()) {
                ImGui::TextDisabled("GpuDriven fallback: %s", resolved.gpu_driven_fallback_reason.c_str());
            }

            ImGui::Separator();
            ImGui::TextUnformatted("Frame Telemetry");
            auto* scheduler = render_system->getFrameScheduler();
            if (!scheduler) {
                ImGui::TextDisabled("No frame scheduler");
            }
            else {
                const auto& collector = scheduler->getTelemetry();
                if (collector.getCount() == 0) {
                    ImGui::TextDisabled("No samples (perf counters disabled?)");
                }
                else {
                    Float sum = 0.0f;
                    Float max_ms = 0.0f;
                    for (Size_t i = 0; i < collector.getCount(); ++i) {
                        const Float ms = collector.previous(static_cast<UInt32>(i)).render_thread_ms;
                        sum += ms;
                        if (ms > max_ms) max_ms = ms;
                    }
                    const FrameTelemetry& cur = collector.current();
                    const Float avg = sum / static_cast<Float>(collector.getCount());
                    ImGui::Text("Render ms: %.3f (avg %.3f / max %.3f, %d frames)",
                        cur.render_thread_ms, avg, max_ms,
                        static_cast<int>(collector.getCount()));
                    ImGui::Text("Draw Calls: %u (indirect %u)  Dispatches: %u",
                        cur.draw_call_count, cur.indirect_draw_call_count, cur.dispatch_count);
                    ImGui::Text("Barriers: %u  Drawn Instances: %llu",
                        cur.barrier_count,
                        static_cast<unsigned long long>(cur.drawn_instance_count));
                    ImGui::Text("Upload Bytes: %llu  Stalls: %u  Overflows: %u",
                        static_cast<unsigned long long>(cur.upload_bytes),
                        cur.upload_stall_count, cur.upload_overflow_count);
                }
            }

            ImGui::Separator();
            ImGui::TextUnformatted("MeshDraw Stats (MeshPassRegistry)");
            if (auto* shared_service = render_system->getSharedRenderService()) {
                if (auto* registry = shared_service->getMeshPassRegistry()) {
                    static constexpr const char* kPassNames[] = {"Opaque", "Shadow", "Transparent"};
                    if (ImGui::BeginTable("MeshDrawStatsTable", 6,
                                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                        ImGui::TableSetupColumn("Pass");
                        ImGui::TableSetupColumn("Raw");
                        ImGui::TableSetupColumn("Draws");
                        ImGui::TableSetupColumn("Cached");
                        ImGui::TableSetupColumn("Dynamic");
                        ImGui::TableSetupColumn("Buckets");
                        ImGui::TableHeadersRow();
                        for (Size_t t = 0; t < static_cast<Size_t>(MeshPassType::Count); ++t) {
                            const auto* storage = registry->getCommandStorage(static_cast<MeshPassType>(t));
                            if (!storage) continue;
                            Size_t raw = 0;
                            Size_t sources = 0;
                            Size_t cached = 0;
                            Size_t dynamic = 0;
                            Size_t buckets = 0;
                            for (const auto& draw_list : storage->getDrawLists()) {
                                raw += draw_list.pre_merge_source_count;
                                sources += draw_list.sources.size();
                                cached += draw_list.cached_instances.size();
                                dynamic += draw_list.dynamic_instances.size();
                                buckets += draw_list.gpu_buckets.size();
                            }
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(kPassNames[t]);
                            ImGui::TableNextColumn();
                            ImGui::Text("%llu", static_cast<unsigned long long>(raw));
                            ImGui::TableNextColumn();
                            ImGui::Text("%llu", static_cast<unsigned long long>(sources));
                            ImGui::TableNextColumn();
                            ImGui::Text("%llu", static_cast<unsigned long long>(cached));
                            ImGui::TableNextColumn();
                            ImGui::Text("%llu", static_cast<unsigned long long>(dynamic));
                            ImGui::TableNextColumn();
                            ImGui::Text("%llu", static_cast<unsigned long long>(buckets));
                        }
                        ImGui::EndTable();
                    }
                }
                else {
                    ImGui::TextDisabled("No mesh pass registry");
                }
            }
            else {
                ImGui::TextDisabled("No shared render service");
            }

            if (!baseline) {
                ImGui::Separator();
                ImGui::TextUnformatted("RenderGraph Snapshot");
                const auto snapshot = RenderGraphDebug::snapshot();
                if (snapshot) {
                    ImGui::Text("Passes: %u (culled %u)  Resources: %u  Levels: %u",
                        snapshot->pass_count, snapshot->culled_count,
                        snapshot->resource_count, snapshot->level_count);
                }
                else {
                    ImGui::TextDisabled("No snapshot (open the Render Graph window to refresh)");
                }
            }

            ImGui::Separator();
            static Bool s_telemetry_export_ok = false;
            static String s_telemetry_export_path{};
            if (ImGui::Button("Dump Telemetry JSON") && scheduler) {
                s_telemetry_export_path = ExportTelemetryJson(*scheduler);
                s_telemetry_export_ok = !s_telemetry_export_path.empty();
            }
            if (!s_telemetry_export_path.empty()) {
                if (s_telemetry_export_ok) {
                    ImGui::Text("Dumped: %s", s_telemetry_export_path.c_str());
                }
                else {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Dump failed");
                }
            }
        }

        Bool ExportMemoryCsv() {
            const String sampled_at = CsvTimestamp();
            String filename = "memory_stats_" + sampled_at + ".csv";
            FsPath out_path = std::filesystem::current_path() / filename.c_str();

            std::ofstream fout(out_path);
            if (!fout.is_open()) {
                return false;
            }

            String mi_global_text;
            mi_stats_print_out(&MiStatsCaptureFn, &mi_global_text);
            const FsPath mi_text_path = std::filesystem::current_path()
                / ("memory_mi_stats_" + sampled_at + ".txt").c_str();
            std::ofstream mi_text_out(mi_text_path);
            if (mi_text_out.is_open()) {
                mi_text_out << mi_global_text;
                mi_text_out.close();
            }

            fout << "sampled_at,category,key,current_bytes,peak_bytes,allocs,frees\n";

#ifdef DO_PLATFORM_WINDOWS
            PROCESS_MEMORY_COUNTERS_EX pmc{};
            pmc.cb = sizeof(pmc);
            if (K32GetProcessMemoryInfo(GetCurrentProcess(),
                                        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                                        sizeof(pmc))) {
                fout << sampled_at << ",Process,working_set," << pmc.WorkingSetSize << "," << pmc.PeakWorkingSetSize << ",,\n";
                fout << sampled_at << ",Process,private_bytes," << pmc.PrivateUsage << ",,,\n";
                fout << sampled_at << ",Process,pagefile_usage," << pmc.PagefileUsage << ",,,\n";
                fout << sampled_at << ",Process,page_faults," << pmc.PageFaultCount << ",,,\n";
            }
            MEMORYSTATUSEX mem_status{};
            mem_status.dwLength = sizeof(mem_status);
            if (GlobalMemoryStatusEx(&mem_status)) {
                fout << sampled_at << ",Process,system_commit_charge,"
                     << (mem_status.ullTotalPageFile - mem_status.ullAvailPageFile) << ",,,\n";
            }
            fout << sampled_at << ",Process,heap_in_use," << QueryProcessHeapInUse() << ",,,\n";
#endif

            const MimallocStats mi_stats = QueryMimallocStats();
            fout << sampled_at << ",Mimalloc,live_allocated," << mi_stats.live_allocated << ",,,\n";
            fout << sampled_at << ",Mimalloc,heap_committed," << mi_stats.heap_committed << ",,,\n";
            fout << sampled_at << ",Mimalloc,committed," << mi_stats.committed << ",,,\n";
            fout << sampled_at << ",Mimalloc,peak_commit," << mi_stats.peak_commit << ",,,\n";
            fout << sampled_at << ",Mimalloc,reserved," << mi_stats.reserved << ",,,\n";
            fout << sampled_at << ",Mimalloc,area_count," << mi_stats.area_count << ",,,\n";

            static constexpr const char* kTierNames[] = {"Persistent", "Frame", "Scratch"};
            static constexpr const char* kTagNames[] = {"Object", "RenderCmd", "Texture", "Resource", "Misc"};
            static constexpr int kTierCount = static_cast<int>(AllocTier::Count);
            static constexpr int kTagCount = static_cast<int>(AllocTag::Count);

            Size_t total_current = 0;
            Size_t total_peak = 0;
            for (int t = 0; t < kTierCount; ++t) {
                for (int g = 0; g < kTagCount; ++g) {
                    const TierStats& stats = Memory::GetStats(static_cast<AllocTier>(t), static_cast<AllocTag>(g));
                    const Size_t current = stats.current_bytes.load(std::memory_order_relaxed);
                    const Size_t peak = stats.peak_bytes.load(std::memory_order_relaxed);
                    total_current += current;
                    total_peak += peak;
                    fout << sampled_at << ",Engine," << kTierNames[t] << '/' << kTagNames[g] << ','
                         << current << ',' << peak << ','
                         << stats.alloc_count.load(std::memory_order_relaxed) << ','
                         << stats.dealloc_count.load(std::memory_order_relaxed) << '\n';
                }
            }
            fout << sampled_at << ",Engine,total," << total_current << ',' << total_peak << ",,\n";
            fout << sampled_at << ",Engine,frame_used," << Memory::FrameUsedBytesTotal() << ",,,\n";
            const ThreadAllocatorStats thread_stats = Memory::GetThreadAllocatorStats();
            fout << sampled_at << ",Engine,thread_allocators," << thread_stats.allocator_count << ",,,\n";
            fout << sampled_at << ",Engine,thread_frame_reserved," << thread_stats.frame_reserved_bytes << ",,,\n";
            fout << sampled_at << ",Engine,thread_scratch_reserved," << thread_stats.scratch_reserved_bytes << ",,,\n";
            for (int g = 0; g < kTagCount; ++g) {
                const PoolRuntimeStats pool_stats = Memory::GetPoolRuntimeStats(static_cast<AllocTag>(g));
                if (!pool_stats.registered) {
                    continue;
                }
                fout << sampled_at << ",Pools," << kTagNames[g] << "_chunk_bytes," << pool_stats.chunk_bytes << ",,,\n";
                fout << sampled_at << ",Pools," << kTagNames[g] << "_used_blocks," << pool_stats.used_blocks << ",,,\n";
                fout << sampled_at << ",Pools," << kTagNames[g] << "_free_blocks," << pool_stats.free_blocks << ",,,\n";
            }

            const auto frame_staging_stats = FrameStagingAllocator::QueryGlobalStats();
            fout << sampled_at << ",FrameStaging,total_bytes," << frame_staging_stats.total_bytes << ",,,\n";
            fout << sampled_at << ",FrameStaging,used_bytes," << frame_staging_stats.used_bytes << ",,,\n";
            fout << sampled_at << ",FrameStaging,peak_used_bytes," << frame_staging_stats.peak_used_bytes << ",,,\n";
            fout << sampled_at << ",FrameStaging,stalls," << frame_staging_stats.stall_count << ",,,\n";
            fout << sampled_at << ",FrameStaging,overflows," << frame_staging_stats.overflow_count << ",,,\n";

            const auto transient_pool_stats = RenderGraphTransientPool::QueryGlobalStats();
            fout << sampled_at << ",TransientPool,texture_count," << transient_pool_stats.texture_count << ",,,\n";
            fout << sampled_at << ",TransientPool,texture_bytes," << transient_pool_stats.texture_bytes << ",,,\n";
            fout << sampled_at << ",TransientPool,buffer_count," << transient_pool_stats.buffer_count << ",,,\n";
            fout << sampled_at << ",TransientPool,buffer_bytes," << transient_pool_stats.buffer_bytes << ",,,\n";

            const auto audio_stats = AudioClip::QueryMemoryStats();
            fout << sampled_at << ",Audio,clip_count," << audio_stats.clip_count << ",,,\n";
            fout << sampled_at << ",Audio,pcm_bytes," << audio_stats.pcm_bytes << ",,,\n";
            fout << sampled_at << ",Audio,peak_pcm_bytes," << audio_stats.peak_pcm_bytes << ",,,\n";
            fout << sampled_at << ",Audio,reader_count," << audio_stats.reader_count << ",,,\n";
            fout << sampled_at << ",Audio,reader_bytes," << audio_stats.reader_bytes << ",,,\n";

            const auto physics_stats = PhysicsWorld::QueryMemoryStats();
            fout << sampled_at << ",Physics,world_count," << physics_stats.world_count << ",,,\n";
            fout << sampled_at << ",Physics,temp_allocator_bytes," << physics_stats.temp_allocator_bytes << ",,,\n";
            fout << sampled_at << ",Physics,body_count," << physics_stats.body_count << ",,,\n";
            fout << sampled_at << ",Physics,shape_count," << physics_stats.shape_count << ",,,\n";

            const auto texture_stats = TextureBlob::QueryMemoryStats();
            fout << sampled_at << ",TextureBlob,blob_count," << texture_stats.blob_count << ",,,\n";
            fout << sampled_at << ",TextureBlob,pixel_bytes," << texture_stats.pixel_bytes << ",,,\n";
            fout << sampled_at << ",TextureBlob,peak_pixel_bytes," << texture_stats.peak_pixel_bytes << ",,,\n";

            if (auto* script_system = GetScriptSystem()) {
                if (auto* script_runtime = script_system->getScriptRuntime()) {
                    ScriptGcInfo gc_info;
                    if (script_runtime->fetchScriptGcInfo(gc_info)) {
                        fout << sampled_at << ",ScriptGC,heap_allocated," << gc_info.heap_allocated_bytes << ",,,\n";
                        fout << sampled_at << ",ScriptGC,heap_size," << gc_info.heap_size_bytes << ",,,\n";
                        fout << sampled_at << ",ScriptGC,memory_load," << gc_info.memory_load_bytes << ",,,\n";
                        fout << sampled_at << ",ScriptGC,gen0_collections," << gc_info.gen0_collections << ",,,\n";
                        fout << sampled_at << ",ScriptGC,gen1_collections," << gc_info.gen1_collections << ",,,\n";
                        fout << sampled_at << ",ScriptGC,gen2_collections," << gc_info.gen2_collections << ",,,\n";
                        fout << sampled_at << ",ScriptGC,assembly_count," << gc_info.assembly_count << ",,,\n";
                        fout << sampled_at << ",ScriptGC,object_registry_count," << gc_info.object_registry_count << ",,,\n";
                        fout << sampled_at << ",ScriptGC,instance_type_cache_count," << gc_info.instance_type_cache_count << ",,,\n";
                        fout << sampled_at << ",ScriptGC,entity_handle_total," << gc_info.entity_handle_total << ",,,\n";
                    }
                }
            }

            fout.close();
            s_csv_export_path = out_path.string();
            return true;
        }

        void RenderMemoryPanel() {
            ImGui::Begin("Memory");

            if (ImGui::Button("Export CSV")) {
                s_csv_export_ok = ExportMemoryCsv();
            }
            if (!s_csv_export_path.empty()) {
                if (s_csv_export_ok) {
                    ImGui::Text("Exported: %s", s_csv_export_path.c_str());
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Export failed");
                }
            }
            ImGui::Separator();

            if (ImGui::CollapsingHeader("Process", ImGuiTreeNodeFlags_DefaultOpen)) {
                RenderProcessMemorySection();
            }

            if (ImGui::CollapsingHeader("Mimalloc", ImGuiTreeNodeFlags_DefaultOpen)) {
                RenderMimallocSection();
            }

            if (ImGui::CollapsingHeader("Engine Allocators", ImGuiTreeNodeFlags_DefaultOpen)) {
                RenderEngineMemorySection();
            }
            if (ImGui::CollapsingHeader("Frame Staging", ImGuiTreeNodeFlags_DefaultOpen)) {
                RenderFrameStagingSection();
            }
            if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen)) {
                RenderAudioSection();
            }
            if (ImGui::CollapsingHeader("Physics3D", ImGuiTreeNodeFlags_DefaultOpen)) {
                RenderPhysicsSection();
            }
            if (ImGui::CollapsingHeader("TextureBlob", ImGuiTreeNodeFlags_DefaultOpen)) {
                RenderTextureSection();
            }

            ImGui::End();
        }
    }

    void DebugImGui::RegisterDebugPanel() {
        if (s_registered) return;
        GetDebugger()->addImGuiRenderFunc("DebugImGui", OnImGuiRender);
        s_registered = true;
    }

    void DebugImGui::UnregisterDebugPanel() {
        if (!s_registered) return;
        GetDebugger()->removeImGuiRenderFunc("DebugImGui");
        s_registered = false;
    }

    void DebugImGui::RequestViewportPick(Int32 x, Int32 y) {
        s_pick_x.store(x, std::memory_order_relaxed);
        s_pick_y.store(y, std::memory_order_relaxed);
        s_pick_requested.store(1, std::memory_order_release);
    }

    Bool DebugImGui::ConsumePickRequest(Int32& out_x, Int32& out_y) {
        UInt32 expected = 1;
        if (!s_pick_requested.compare_exchange_strong(expected, 0, std::memory_order_acq_rel)) {
            return false;
        }
        out_x = s_pick_x.load(std::memory_order_relaxed);
        out_y = s_pick_y.load(std::memory_order_relaxed);
        return true;
    }

    void DebugImGui::SubmitPickResult(UInt64 entity_uuid) {
        s_pick_result.store(entity_uuid, std::memory_order_relaxed);
        s_pick_result_valid.store(1, std::memory_order_release);
    }

    Bool DebugImGui::ConsumePickResult(UInt64& out_entity_uuid) {
        UInt32 expected = 1;
        if (!s_pick_result_valid.compare_exchange_strong(expected, 0, std::memory_order_acq_rel)) {
            return false;
        }
        out_entity_uuid = s_pick_result.load(std::memory_order_relaxed);
        return true;
    }

    void DebugImGui::ApplyPickedEntity(UInt64 entity_uuid) {
        if (entity_uuid == 0) {
            s_selectedEntity = Entity{};
            return;
        }
        Scene* scene = GetWorld() ? GetWorld()->getActiveScene() : nullptr;
        if (!scene) {
            return;
        }
        Entity entity = scene->tryGetEntityByUUID(UUID(entity_uuid));
        if (entity.valid()) {
            s_selectedEntity = entity;
        }
    }

    void DebugImGui::OnImGuiRender() {
#ifndef DODOE_EDITOR_ENABLED
        const ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse &&
            io.MousePos.x >= 0.0f && io.MousePos.y >= 0.0f) {
            RequestViewportPick(static_cast<Int32>(io.MousePos.x), static_cast<Int32>(io.MousePos.y));
        }
        UInt64 picked_uuid = 0;
        if (ConsumePickResult(picked_uuid)) {
            ApplyPickedEntity(picked_uuid);
        }
        RenderHierarchyPanel();
        RenderInspectorPanel();
        RenderMemoryPanel();
        RenderDebuggerPanel();
#endif//DODOE_EDITOR_ENABLED;
    }

    void DebugImGui::RenderEntityMaterials(Entity entity) {
        auto* render_system = GetRenderSystem();
        auto* shared_service = render_system ? render_system->getSharedRenderService() : nullptr;
        auto* material_system = shared_service ? shared_service->getMaterialSystem() : nullptr;
        if (!material_system) {
            return;
        }

        static constexpr std::array<const char*, 4> kTextureSlotNames = {
            "Base Color", "Metallic Roughness", "Normal", "Emissive"
        };

        auto draw_instance_editor = [&](const String& owner_label, const String& instance_name,
                                        const MaterialInstance& instance, const String& section_label) {
            ImGui::PushID(instance_name.c_str());
            ImGui::SeparatorText(owner_label.c_str());
            ImGui::Text("%s (template: %s)",
                        section_label.c_str(),
                        instance.tpl ? instance.tpl->desc.name.c_str() : "<none>");
            if (!instance.resolved) {
                ImGui::TextColored(ImVec4(0.95f, 0.8f, 0.25f, 1.0f), "Not resolved");
            }

            MaterialParamValue value{};

            Float metallic = instance.metallic;
            if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f, "%.2f")) {
                value.f[0] = metallic;
                material_system->setInstanceParam(instance_name, "metallic", value);
            }

            Float roughness = instance.roughness;
            if (ImGui::SliderFloat("Roughness", &roughness, 0.04f, 1.0f, "%.2f")) {
                value.f[0] = roughness;
                material_system->setInstanceParam(instance_name, "roughness", value);
            }

            Float ao = instance.ao;
            if (ImGui::SliderFloat("Ambient Occlusion", &ao, 0.0f, 1.0f, "%.2f")) {
                value.f[0] = ao;
                material_system->setInstanceParam(instance_name, "ao", value);
            }

            Float emissive[3] = {instance.emissive.x, instance.emissive.y, instance.emissive.z};
            if (ImGui::ColorEdit3("Emissive", emissive, ImGuiColorEditFlags_Float)) {
                value.f[0] = emissive[0];
                value.f[1] = emissive[1];
                value.f[2] = emissive[2];
                material_system->setInstanceParam(instance_name, "emissive", value);
            }

            for (Size_t slot = 0; slot < kTextureSlotNames.size(); ++slot) {
                const bool has_texture = slot < instance.textures.size() && instance.textures[slot] != nullptr;
                if (!has_texture) {
                    ImGui::TextDisabled("  %s: fallback", kTextureSlotNames[slot]);
                }
            }

            ImGui::PopID();
        };

        if (!ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
            return;
        }

        if (!s_material_owner_valid || s_material_owner_entity != entity.uuid()) {
            s_material_owner_entity = entity.uuid();
            s_material_owner_valid = true;
            s_material_owner_candidates.clear();
            UnorderedSet<UUID> visited;
            Entity current = entity;
            while (current.valid() && visited.insert(current.uuid()).second &&
                   s_material_owner_candidates.size() < 64) {
                s_material_owner_candidates.push_back(current);
                if (!current.hasComponent<HierarchyComponent>()) {
                    break;
                }
                current = current.getComponent<HierarchyComponent>().parent;
            }
            Scene* scene = GetWorld() ? GetWorld()->getActiveScene() : nullptr;
            DynamicArray<Entity> queue;
            if (entity.hasComponent<HierarchyComponent>()) {
                for (Entity child : entity.getComponent<HierarchyComponent>().children) {
                    queue.push_back(child);
                }
            }
            for (Size_t index = 0; index < queue.size() && s_material_owner_candidates.size() < 64; ++index) {
                Entity child = queue[index];
                if (!child.valid() || !visited.insert(child.uuid()).second) {
                    continue;
                }
                s_material_owner_candidates.push_back(child);
                if (child.hasComponent<HierarchyComponent>()) {
                    for (Entity grandchild : child.getComponent<HierarchyComponent>().children) {
                        queue.push_back(grandchild);
                    }
                }
            }
        }

        bool found_any = false;
        for (Entity owner : s_material_owner_candidates) {
            const String owner_label = owner.hasComponent<IDComponent>()
                ? owner.getComponent<IDComponent>().name : String("Entity");
            const String owner_prefix = String(fmt::format(
                "Mat_{}_", static_cast<UInt64>(owner.uuid())).c_str());
            for (const auto& [instance_name, instance] : material_system->getInstances()) {
                if (instance_name.compare(0, owner_prefix.size(), owner_prefix) != 0) {
                    continue;
                }
                found_any = true;
                draw_instance_editor(owner_label, instance_name, instance,
                                     "Section " + instance_name.substr(owner_prefix.size()));
            }
        }

        if (!found_any) {
            ImGui::TextDisabled("No material instances (total: %zu).",
                                material_system->getInstances().size());
        }
    }

    void DebugImGui::RenderDebuggerPanel() { 
        ImGui::Begin("Dodoe Debugger");
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("FPS: %.1f (%.3f ms)", io.Framerate, 1000.0f / io.Framerate);

        RenderRendererCompare();

        RenderWorldStateControls();
        ImGui::Separator();

        if (ImGui::Button("Reload Scripts")) {
            Bool success = GetScriptSystem()->reloadScripts();
            if (success) DO_INFO("Reload Scripts succeed!");
        }
        ImGui::Separator();
        RenderToolActions();

        ImGui::Separator();
        ImGui::TextUnformatted("Launch Switches");
        ImGui::TextDisabled("--dswitch=<key,...> / --dswitch-off=<key,...>");
        {
            Bool any_explicit = false;
            DebugSwitches::ForEachToken([&any_explicit](const DebugSwitches::Token& token) {
                any_explicit = true;
            });
            for (Size_t i = 0; i < ConfigSwitchRegistry::Count(); ++i) {
                const ConfigSwitchInfo& info = ConfigSwitchRegistry::At(i);
                const Bool on = DebugSwitches::IsEnabled(info.key);
                const ImVec4 color = on
                    ? ImVec4(0.45f, 0.90f, 0.50f, 1.0f)
                    : ImVec4(0.95f, 0.45f, 0.45f, 1.0f);
                ImGui::TextColored(color, "%c %s - %s",
                    on ? '+' : '-', info.key.data(), info.description.data());
            }
            if (any_explicit) {
                ImGui::Separator();
                DebugSwitches::ForEachToken([](const DebugSwitches::Token& token) {
                    const ImVec4 color = token.on
                        ? ImVec4(0.45f, 0.90f, 0.50f, 1.0f)
                        : ImVec4(0.95f, 0.45f, 0.45f, 1.0f);
                    ImGui::TextColored(color, "%c %s (explicit)", token.on ? '+' : '-', token.key.c_str());
                });
            }
        }
        ImGui::End();
    }

    void DebugImGui::RenderToolActions() {
        auto* script_system = GetScriptSystem();
        if (!script_system) return;

        ImGui::TextUnformatted("C# Tool Actions ([ToolMenuItem])");

        DynamicArray<String> actions;
        if (!script_system->listToolActions(actions)) {
            ImGui::TextDisabled("C# script runtime unavailable");
            return;
        }
        if (actions.empty()) {
            ImGui::TextDisabled("No tool actions registered");
            return;
        }

        static String s_tool_error;
        if (!s_tool_error.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", s_tool_error.c_str());
        }

        std::map<String, DynamicArray<String>> groups;
        for (const String& path : actions) {
            const size_t slash = path.find('/');
            const String top = (slash == String::npos) ? path : path.substr(0, slash);
            groups[top].push_back(path);
        }

        for (auto& [top, subpaths] : groups) {
            if (ImGui::TreeNode(top.c_str())) {
                for (const String& path : subpaths) {
                    const size_t slash = path.find('/');
                    const String label = (slash == String::npos) ? path : path.substr(slash + 1);
                    if (ImGui::MenuItem(label.c_str())) {
                        String error;
                        if (script_system->invokeToolAction(path, error)) {
                            s_tool_error.clear();
                        }
                        else {
                            s_tool_error = error;
                        }
                    }
                }
                ImGui::TreePop();
            }
        }
    }

    void DebugImGui::RenderHierarchyPanel() {
        ImGui::Begin("Hierarchy");
        Scene* scene = GetWorld()->getActiveScene();
        if (!scene) {
            ImGui::TextUnformatted("No active scene.");
            ImGui::End();
            return;
        }
        for (const EntityNode& root : BuildEntityTree(*scene)) RenderEntityTreeNode(root);
        ImGui::End();
    }

    DynamicArray<DebugImGui::EntityNode> DebugImGui::BuildEntityTree(Scene& scene) {
        const auto all_entities = scene.getEntities();
        UnorderedMap<UUID, Entity> by_uuid;
        UnorderedMap<UUID, DynamicArray<UUID>> children;
        DynamicArray<UUID> root_ids;
        by_uuid.reserve(all_entities.size());

        for (Entity entity : all_entities) by_uuid.emplace(entity.uuid(), entity);
        for (Entity entity : all_entities) {
            UUID parent_uuid{};
            Entity parent{};
            if (entity.hasComponent<HierarchyComponent>()) {
                parent = entity.getComponent<HierarchyComponent>().parent;
            }
            const bool has_parent = parent.valid() && scene.registry().valid(parent);
            if (has_parent) parent_uuid = parent.uuid();
            if (!has_parent || parent_uuid == entity.uuid() || !by_uuid.contains(parent_uuid)) {
                root_ids.push_back(entity.uuid());
            }
            else {
                children[parent_uuid].push_back(entity.uuid());
            }
        }

        DynamicArray<EntityNode> roots;
        UUIDSet built;
        std::function<EntityNode(const UUID&)> make_node = [&](const UUID& uuid) {
            EntityNode node{by_uuid.at(uuid), {}};
            built.insert(uuid);
            auto it = children.find(uuid);
            if (it != children.end()) {
                for (const UUID& child_uuid : it->second) {
                    if (!built.contains(child_uuid)) node.children.push_back(make_node(child_uuid));
                }
            }
            return node;
        };
        for (const UUID& uuid : root_ids) {
            if (!built.contains(uuid)) roots.push_back(make_node(uuid));
        }
        for (const auto& [uuid, _] : by_uuid) {
            if (!built.contains(uuid)) roots.push_back(make_node(uuid));
        }
        return roots;
    }

    void DebugImGui::RenderEntityTreeNode(const EntityNode& node) {
        Entity entity = node.entity;
        if (!entity.valid()) return;

        ImGui::PushID(static_cast<int>(static_cast<ui32>(entity)));
        const String& name = entity.name();
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (node.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
        if (s_selectedEntity.valid() && entity.uuid() == s_selectedEntity.uuid()) flags |= ImGuiTreeNodeFlags_Selected;

        const bool opened = ImGui::TreeNodeEx("Entity", flags, "%s", name.c_str());
        if (ImGui::IsItemClicked()) s_selectedEntity = entity;
        if (opened) {
            for (const EntityNode& child : node.children) RenderEntityTreeNode(child);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void DebugImGui::RenderInspectorPanel() {
        ImGui::Begin("Inspector");
        if (!s_selectedEntity.valid()) {
            ImGui::TextUnformatted("No entity selected.");
            ImGui::End();
            return;
        }

        Entity entity = s_selectedEntity;
        auto& db = ComponentDB::self();
        for (const auto& entry : db.entries()) {
            if (entry.contains(entity)) DrawNativeComponent(entity, entry);
        }
        DrawManagedComponents(entity);
        RenderEntityMaterials(entity);
        ImGui::End();
    }

}

#endif//DOODE_DEBUG_ENABLED
