#include "debug_imgui.h"

#ifdef DODOE_DEBUG_ENABLED

#include "imgui/imgui.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/memory/memory.h"
#include "runtime/core/meta/component_db.h"
#include "runtime/function/script/script_system.h"
#include "runtime/function/time/time_system.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/world.h"

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

            DynamicArray<Pair<String, Json>> components;
            if (!runtime->getEntityManagedComponentFields(static_cast<uint64_t>(entity.uuid()), components)) {
                return;
            }

            for (auto& [type_name, fields] : components) {
                ImGui::PushID(type_name.c_str());
                String title = type_name;
                const auto dot = title.find_last_of('.');
                if (dot != String::npos) {
                    title = title.substr(dot + 1);
                }
                if (ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (DrawJsonValue(fields, false)) {
                        runtime->setEntityManagedComponentFields(
                            static_cast<uint64_t>(entity.uuid()), type_name, fields);
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
            ImGui::Text("Commit Charge:    %s / %s",
                        FormatBytes(commit_charge).c_str(),
                        FormatBytes(mem_status.ullTotalPageFile).c_str());

            static constexpr float kHeapWalkInterval = 0.5f;
            static float s_heap_walk_elapsed = 0.0f;
            static UInt64 s_heap_in_use = 0;
            s_heap_walk_elapsed += ImGui::GetIO().DeltaTime;
            if (s_heap_walk_elapsed >= kHeapWalkInterval) {
                s_heap_walk_elapsed = 0.0f;
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
                s_heap_in_use = in_use;
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
            Size_t reserved = 0;
            Size_t area_count = 0;
        };

        struct MimallocVisitArg {
            MimallocStats stats;
            const void* areas[256];
        };

        bool MimallocAreaVisit(const mi_heap_t*, const mi_heap_area_t* area, void*, size_t block_size, void* arg) {
            MimallocVisitArg* a = static_cast<MimallocVisitArg*>(arg);
            a->stats.live_allocated += block_size;
            for (Size_t i = 0; i < a->stats.area_count; ++i) {
                if (a->areas[i] == area) {
                    return true;
                }
            }
            if (a->stats.area_count < 256) {
                a->areas[a->stats.area_count++] = area;
                a->stats.committed += area->committed;
                a->stats.reserved += area->reserved;
            }
            return true;
        }

        MimallocStats QueryMimallocStats() {
            MimallocVisitArg arg{};
            mi_heap_visit_blocks(mi_heap_get_default(), false, &MimallocAreaVisit, &arg);
            mi_heap_visit_blocks(mi_heap_get_backing(), false, &MimallocAreaVisit, &arg);
            return arg.stats;
        }

        void RenderMimallocSection() {
            const MimallocStats stats = QueryMimallocStats();

            ImGui::Text("Live Allocated:  %s", FormatBytes(stats.live_allocated).c_str());
            ImGui::Text("Committed:       %s", FormatBytes(stats.committed).c_str());
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

            ImGui::Text("Frame Used (Thread Allocators): %s",
                        FormatBytes(Memory::FrameUsedBytesTotal()).c_str());
            ImGui::TextDisabled("Frame/Scratch current counts outstanding allocs (ring buffer, not reclaimed per frame)");

            if (ImGui::Button("Reset Stats")) {
                Memory::ResetAllStats();
            }
            ImGui::SameLine();
            if (ImGui::Button("Dump All")) {
                Memory::DumpAll();
            }
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

        Bool ExportMemoryCsv() {
            const String sampled_at = CsvTimestamp();
            String filename = "memory_stats_" + sampled_at + ".csv";
            FsPath out_path = std::filesystem::current_path() / filename.c_str();

            std::ofstream fout(out_path);
            if (!fout.is_open()) {
                return false;
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
                fout << sampled_at << ",Process,commit_charge,"
                     << (mem_status.ullTotalPageFile - mem_status.ullAvailPageFile) << ",,,\n";
            }
#endif

            const MimallocStats mi_stats = QueryMimallocStats();
            fout << sampled_at << ",Mimalloc,live_allocated," << mi_stats.live_allocated << ",,,\n";
            fout << sampled_at << ",Mimalloc,committed," << mi_stats.committed << ",,,\n";
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

            RenderEngineMemorySection();

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

    void DebugImGui::OnImGuiRender() {
#ifndef DODOE_EDITOR_ENABLED
        RenderHierarchyPanel();
        RenderInspectorPanel();
        RenderMemoryPanel();
        RenderDebuggerPanel();
#endif//DODOE_EDITOR_ENABLED;
    }

    void DebugImGui::RenderDebuggerPanel() { 
        ImGui::Begin("Dodoe Debugger");
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("FPS: %.1f (%.3f ms)", io.Framerate, 1000.0f / io.Framerate);

        RenderWorldStateControls();
        ImGui::Separator();

        if (ImGui::Button("Reload Scripts")) {
            Bool success = GetScriptSystem()->reloadScripts();
            if (success) DO_INFO("Reload Scripts succeed!");
        }
        ImGui::Separator();
        RenderToolActions();
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
        ImGui::End();
    }

}

#endif//DOODE_DEBUG_ENABLED
