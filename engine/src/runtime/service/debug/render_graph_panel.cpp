// do@Redlive

#include "render_graph_panel.h"

#ifdef DODOE_DEBUG_ENABLED

#include "imgui/imgui.h"
#include "imnodes.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_graph/render_graph_debug.h"

#include <utility>
#include <vector>

namespace dodoe {

    namespace {

        constexpr int kPassNodeIdBase = 0x100000;
        constexpr int kPassInputBase = 0x200000;
        constexpr int kPassOutputBase = 0x300000;
        constexpr int kResourceNodeBase = 0x400000;
        constexpr int kResourceInputBase = 0x500000;
        constexpr int kResourceOutputBase = 0x600000;
        constexpr int kLinkIdBase = 0x700000;

        constexpr int kLevelsPerRow = 3;
        constexpr float kLaneWidth = 150.0f;
        constexpr float kRowHeight = 74.0f;
        constexpr float kLayerBandHeight = 360.0f;

        bool s_registered = false;
        bool s_show_resources = true;
        bool s_auto_refresh = true;
        int s_refresh_interval_ms = 500;
        bool s_show_pass_details = true;

        UInt64 s_layout_sequence = 0;
        UInt32 s_layout_pass_count = 0;
        UInt32 s_layout_resource_count = 0;
        bool s_apply_layout = true;
        std::vector<std::pair<int, ImVec2>> s_node_positions{};

        struct LinkInfo {
            int id;
            UInt32 resource_index;
        };
        std::vector<LinkInfo> s_link_infos{};

        const char* AccessTypeName(const RenderGraphAccessType type) {
            switch (type) {
                case RenderGraphAccessType::Read: return "Read";
                case RenderGraphAccessType::Write: return "Write";
                case RenderGraphAccessType::ReadWrite: return "ReadWrite";
            }
            return "?";
        }

        const char* StageName(const RenderGraphPipelineStage stage) {
            switch (stage) {
                case RenderGraphPipelineStage::VertexShader: return "VS";
                case RenderGraphPipelineStage::PixelShader: return "PS";
                case RenderGraphPipelineStage::ComputeShader: return "CS";
                case RenderGraphPipelineStage::Copy: return "Copy";
                case RenderGraphPipelineStage::RenderTarget: return "RT";
                case RenderGraphPipelineStage::DepthStencil: return "DS";
            }
            return "?";
        }

        const char* PassFlagsName(const RenderGraphPassFlags flags) {
            if (HasAnyFlags(flags, RenderGraphPassFlags::AsyncCompute)) return "async-compute";
            if (HasAnyFlags(flags, RenderGraphPassFlags::Compute)) return "compute";
            if (HasAnyFlags(flags, RenderGraphPassFlags::Copy)) return "copy";
            if (HasAnyFlags(flags, RenderGraphPassFlags::Raster)) return "raster";
            return "pass";
        }

        void CompactLabel(const String& label) {
            constexpr Size_t kMaxLabelLength = 16;
            const Size_t length = std::min(label.size(), kMaxLabelLength);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 124.0f);
            ImGui::TextUnformatted(label.c_str(), label.c_str() + length);
            if (label.size() > kMaxLabelLength) {
                ImGui::SameLine(0.0f, 0.0f);
                ImGui::TextUnformatted("...");
            }
            ImGui::PopTextWrapPos();
        }

        void ComputeLayout(const RenderGraphDebugSnapshot& snap) {
            const Bool graph_shape_changed = s_layout_pass_count != snap.pass_count ||
                                             s_layout_resource_count != snap.resource_count;
            if (!s_apply_layout && !graph_shape_changed) {
                s_layout_sequence = snap.sequence;
                return;
            }

            s_node_positions.clear();
            s_node_positions.reserve(snap.pass_count + snap.resource_count);

            UnorderedMap<int, int> row_counters{};
            auto next_pos = [&row_counters](const int level, const int lane_offset) {
                const int layer_row = level / kLevelsPerRow;
                const int column = (level % kLevelsPerRow) * 2 + lane_offset;
                const int lane_key = layer_row * (kLevelsPerRow * 2) + column;
                const int row = row_counters[lane_key]++;
                return ImVec2(static_cast<float>(column) * kLaneWidth,
                              static_cast<float>(layer_row) * kLayerBandHeight +
                                  static_cast<float>(row) * kRowHeight);
            };

            for (UInt32 i = 0; i < snap.pass_count; i++) {
                s_node_positions.emplace_back(kPassNodeIdBase + static_cast<int>(i),
                                              next_pos(static_cast<int>(snap.passes[i].level), 0));
            }

            for (UInt32 r = 0; r < snap.resource_count; r++) {
                const auto& resource = snap.resources[r];
                int slot = -1;
                if (!resource.reader_passes.empty()) {
                    const auto reader = resource.reader_passes.front();
                    if (reader < snap.pass_count) {
                        slot = static_cast<int>(snap.passes[reader].level);
                    }
                } else if (!resource.writer_passes.empty()) {
                    const auto writer = resource.writer_passes.back();
                    if (writer < snap.pass_count) {
                        slot = static_cast<int>(snap.passes[writer].level) + 1;
                    }
                } else if (resource.first_pass_index >= 0) {
                    const auto first_pass = static_cast<UInt32>(resource.first_pass_index);
                    if (first_pass < snap.pass_count) {
                        slot = static_cast<int>(snap.passes[first_pass].level);
                    }
                }
                if (slot < 0) slot = 0;
                s_node_positions.emplace_back(kResourceNodeBase + static_cast<int>(r),
                                              next_pos(slot, 1));
            }

            s_layout_sequence = snap.sequence;
            s_layout_pass_count = snap.pass_count;
            s_layout_resource_count = snap.resource_count;
        }

        void InvalidateLayout() {
            s_layout_sequence = 0;
            s_layout_pass_count = 0;
            s_layout_resource_count = 0;
            s_apply_layout = true;
        }

        ImVec2 GetNodePos(const int node_id) {
            for (const auto& [id, pos] : s_node_positions) {
                if (id == node_id) return pos;
            }
            return ImVec2(0.0f, 0.0f);
        }

        void ApplyInitialNodePosition(const int node_id) {
            if (s_apply_layout) {
                ImNodes::SetNodeGridSpacePos(node_id, GetNodePos(node_id));
            }
        }

        void PassTooltip(const RenderGraphDebugSnapshot& snap, const UInt32 index) {
            const auto& pass = snap.passes[index];
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(pass.name.c_str());
            ImGui::TextDisabled("%s | level %u | %u barriers", PassFlagsName(pass.flags),
                                pass.level, pass.barrier_count);
            if (pass.subgraph_index < snap.subgraph_names.size()) {
                ImGui::TextDisabled("subgraph: %s",
                                    snap.subgraph_names[pass.subgraph_index].c_str());
            }
            for (const auto& access : pass.accesses) {
                if (access.resource_index < snap.resource_count) {
                    ImGui::BulletText("%s [%s %s]",
                                      snap.resources[access.resource_index].name.c_str(),
                                      AccessTypeName(access.access_type),
                                      StageName(access.stage));
                }
            }
            ImGui::EndTooltip();
        }

        void RenderNode(const RenderGraphDebugSnapshot& snap, const UInt32 index) {
            const auto& pass = snap.passes[index];
            const int node_id = kPassNodeIdBase + static_cast<int>(index);
            const Bool tinted = pass.culled;
            const Bool compute_tint =
                !tinted && HasAnyFlags(pass.flags, RenderGraphPassFlags::AsyncCompute |
                                                      RenderGraphPassFlags::Compute |
                                                      RenderGraphPassFlags::Copy);

            if (tinted) {
                ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(52, 52, 56, 255));
                ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(72, 72, 78, 255));
                ImNodes::PushColorStyle(ImNodesCol_NodeOutline, IM_COL32(90, 90, 96, 255));
            } else if (HasAnyFlags(pass.flags, RenderGraphPassFlags::AsyncCompute)) {
                ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(156, 88, 24, 255));
            } else if (HasAnyFlags(pass.flags, RenderGraphPassFlags::Compute)) {
                ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(66, 96, 156, 255));
            } else if (HasAnyFlags(pass.flags, RenderGraphPassFlags::Copy)) {
                ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(96, 130, 96, 255));
            }

            ImNodes::BeginNode(node_id);
            ApplyInitialNodePosition(node_id);

            ImNodes::BeginNodeTitleBar();
            CompactLabel(pass.name);
            ImNodes::EndNodeTitleBar();

            ImNodes::BeginInputAttribute(kPassInputBase + static_cast<int>(index),
                                         ImNodesPinShape_CircleFilled);
            ImGui::TextDisabled(pass.culled ? "x" : "in");
            ImNodes::EndInputAttribute();

            if (s_show_pass_details) {
                ImGui::TextDisabled("%u b | %u a", pass.barrier_count,
                                    static_cast<UInt32>(pass.accesses.size()));
            }

            ImNodes::BeginOutputAttribute(kPassOutputBase + static_cast<int>(index),
                                          ImNodesPinShape_CircleFilled);
            ImGui::TextDisabled(pass.culled ? "x" : "out");
            ImNodes::EndOutputAttribute();

            ImNodes::EndNode();

            if (tinted) {
                ImNodes::PopColorStyle();
                ImNodes::PopColorStyle();
                ImNodes::PopColorStyle();
            } else if (compute_tint) {
                ImNodes::PopColorStyle();
            }
        }

        void RenderResourceNode(const RenderGraphDebugSnapshot& snap, const UInt32 index) {
            const auto& resource = snap.resources[index];
            const int node_id = kResourceNodeBase + static_cast<int>(index);
            const Bool tinted = resource.exported || resource.imported ||
                                resource.type == RenderGraphResourceType::Buffer;

            if (resource.exported) {
                ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(52, 118, 88, 255));
            } else if (resource.imported) {
                ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(96, 76, 128, 255));
            } else if (resource.type == RenderGraphResourceType::Buffer) {
                ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(128, 96, 40, 255));
            }

            ImNodes::BeginNode(node_id);
            ApplyInitialNodePosition(node_id);

            ImNodes::BeginNodeTitleBar();
            CompactLabel(resource.name);
            ImNodes::EndNodeTitleBar();

            ImNodes::BeginInputAttribute(kResourceInputBase + static_cast<int>(index),
                                         ImNodesPinShape_Circle);
            ImGui::TextDisabled("in");
            ImNodes::EndInputAttribute();

            ImGui::TextDisabled("%s%s%s",
                                resource.type == RenderGraphResourceType::Texture ? "tex" : "buf",
                                resource.imported ? " imp" : "",
                                resource.exported ? " exp" : "");

            ImNodes::BeginOutputAttribute(kResourceOutputBase + static_cast<int>(index),
                                          ImNodesPinShape_Circle);
            ImGui::TextDisabled("out");
            ImNodes::EndOutputAttribute();

            ImNodes::EndNode();

            if (tinted) {
                ImNodes::PopColorStyle();
            }
        }

        void RenderLinks(const RenderGraphDebugSnapshot& snap) {
            s_link_infos.clear();
            int link_id = kLinkIdBase;

            if (s_show_resources) {
                for (UInt32 r = 0; r < snap.resource_count; r++) {
                    const auto& resource = snap.resources[r];
                    for (const auto writer : resource.writer_passes) {
                        if (writer >= snap.pass_count) continue;
                        if (snap.passes[writer].culled) continue;
                        ImNodes::Link(link_id, kPassOutputBase + static_cast<int>(writer),
                                      kResourceInputBase + static_cast<int>(r));
                        s_link_infos.push_back({link_id, r});
                        link_id++;
                    }
                    for (const auto reader : resource.reader_passes) {
                        if (reader >= snap.pass_count) continue;
                        if (snap.passes[reader].culled) continue;
                        ImNodes::Link(link_id, kResourceOutputBase + static_cast<int>(r),
                                      kPassInputBase + static_cast<int>(reader));
                        s_link_infos.push_back({link_id, r});
                        link_id++;
                    }
                }
            } else {
                for (UInt32 r = 0; r < snap.resource_count; r++) {
                    const auto& resource = snap.resources[r];
                    for (const auto writer : resource.writer_passes) {
                        if (writer >= snap.pass_count) continue;
                        if (snap.passes[writer].culled) continue;
                        for (const auto reader : resource.reader_passes) {
                            if (reader >= snap.pass_count) continue;
                            if (snap.passes[reader].culled) continue;
                            ImNodes::Link(link_id, kPassOutputBase + static_cast<int>(writer),
                                          kPassInputBase + static_cast<int>(reader));
                            s_link_infos.push_back({link_id, r});
                            link_id++;
                        }
                    }
                }
            }
        }

        void RenderHoverTooltips(const RenderGraphDebugSnapshot& snap) {
            int hovered_node = -1;
            if (ImNodes::IsNodeHovered(&hovered_node)) {
                const int pass_index = hovered_node - kPassNodeIdBase;
                const int resource_index = hovered_node - kResourceNodeBase;
                if (pass_index >= 0 && static_cast<UInt32>(pass_index) < snap.pass_count) {
                    PassTooltip(snap, static_cast<UInt32>(pass_index));
                } else if (resource_index >= 0 &&
                           static_cast<UInt32>(resource_index) < snap.resource_count) {
                    const auto& resource = snap.resources[static_cast<UInt32>(resource_index)];
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(resource.name.c_str());
                    ImGui::TextDisabled("%u writers / %u readers",
                                        static_cast<UInt32>(resource.writer_passes.size()),
                                        static_cast<UInt32>(resource.reader_passes.size()));
                    ImGui::EndTooltip();
                }
            }

            int hovered_link = -1;
            if (ImNodes::IsLinkHovered(&hovered_link)) {
                for (const auto& link : s_link_infos) {
                    if (link.id != hovered_link) continue;
                    const auto& resource = snap.resources[link.resource_index];
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(resource.name.c_str());
                    ImGui::EndTooltip();
                    break;
                }
            }
        }

        void RenderResourceTable(const RenderGraphDebugSnapshot& snap) {
            if (!ImGui::CollapsingHeader("Resources", ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }
            if (!ImGui::BeginTable("render_graph_resources", 6,
                                   ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                       ImGuiTableFlags_Resizable)) {
                return;
            }
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Source");
            ImGui::TableSetupColumn("Exported");
            ImGui::TableSetupColumn("First/Last Pass");
            ImGui::TableSetupColumn("W/R");
            ImGui::TableHeadersRow();

            for (UInt32 r = 0; r < snap.resource_count; r++) {
                const auto& resource = snap.resources[r];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(resource.name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(resource.type == RenderGraphResourceType::Texture
                                           ? "texture"
                                           : "buffer");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(resource.imported ? "imported" : "transient");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(resource.exported ? "yes" : "-");
                ImGui::TableNextColumn();
                ImGui::Text("%d / %d", resource.first_pass_index, resource.last_pass_index);
                ImGui::TableNextColumn();
                ImGui::Text("%u / %u", static_cast<UInt32>(resource.writer_passes.size()),
                            static_cast<UInt32>(resource.reader_passes.size()));
            }
            ImGui::EndTable();
        }

    }

    void RenderGraphPanel::Register() {
        if (s_registered) return;
        ImNodes::CreateContext();
        auto& style = ImNodes::GetStyle();
        style.Colors[ImNodesCol_NodeBackground] = IM_COL32(51, 61, 77, 255);
        style.Colors[ImNodesCol_NodeBackgroundHovered] = IM_COL32(66, 79, 97, 255);
        style.Colors[ImNodesCol_NodeBackgroundSelected] = IM_COL32(79, 97, 120, 255);
        style.Colors[ImNodesCol_NodeOutline] = IM_COL32(152, 108, 213, 150);
        style.Colors[ImNodesCol_TitleBar] = IM_COL32(25, 26, 33, 255);
        style.Colors[ImNodesCol_TitleBarHovered] = IM_COL32(66, 79, 97, 255);
        style.Colors[ImNodesCol_TitleBarSelected] = IM_COL32(152, 108, 213, 255);
        style.Colors[ImNodesCol_Link] = IM_COL32(152, 108, 213, 190);
        style.Colors[ImNodesCol_LinkHovered] = IM_COL32(189, 147, 249, 255);
        style.Colors[ImNodesCol_LinkSelected] = IM_COL32(255, 121, 198, 255);
        style.Colors[ImNodesCol_Pin] = IM_COL32(152, 108, 213, 255);
        style.Colors[ImNodesCol_PinHovered] = IM_COL32(255, 121, 198, 255);
        style.Colors[ImNodesCol_GridBackground] = IM_COL32(33, 34, 44, 255);
        style.Colors[ImNodesCol_GridLine] = IM_COL32(51, 61, 77, 255);
        style.Colors[ImNodesCol_GridLinePrimary] = IM_COL32(66, 79, 97, 255);
        GetDebugger()->addImGuiRenderFunc("RenderGraphPanel", OnImGuiRender);
        s_registered = true;
    }

    void RenderGraphPanel::Unregister() {
        if (!s_registered) return;
        GetDebugger()->removeImGuiRenderFunc("RenderGraphPanel");
        ImNodes::DestroyContext();
        s_registered = false;
    }

    void RenderGraphPanel::OnImGuiRender() {
        const auto snap = RenderGraphDebug::snapshot();
        if (!snap) return;

        ImGui::Begin("Render Graph");

        RenderGraphDebug::setAutoRefresh(s_auto_refresh);
        RenderGraphDebug::setRefreshIntervalMs(static_cast<UInt32>(s_refresh_interval_ms));

        ImGui::Checkbox("Auto Refresh", &s_auto_refresh);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140.0f);
        ImGui::SliderInt("##interval", &s_refresh_interval_ms, 100, 2000, "%d ms");
        ImGui::SameLine();
        if (ImGui::Button("Refresh Now")) {
            RenderGraphDebug::requestRefresh();
        }
        ImGui::Checkbox("Show Resources", &s_show_resources);
        ImGui::SameLine();
        ImGui::Checkbox("Pass Details", &s_show_pass_details);
        ImGui::SameLine();
        if (ImGui::Button("Reset Layout")) {
            InvalidateLayout();
            ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
        }

        ImGui::TextDisabled("frame #%llu | %u passes (%u culled) | %u resources | %u levels",
                            static_cast<unsigned long long>(snap->sequence), snap->pass_count,
                            snap->culled_count, snap->resource_count, snap->level_count);

        ComputeLayout(*snap);

        ImNodes::PushStyleVar(ImNodesStyleVar_NodePadding, ImVec2(4.0f, 4.0f));
        ImNodes::BeginNodeEditor();

        for (UInt32 i = 0; i < snap->pass_count; i++) {
            RenderNode(*snap, i);
        }

        if (s_show_resources) {
            for (UInt32 r = 0; r < snap->resource_count; r++) {
                RenderResourceNode(*snap, r);
            }
        }

        RenderLinks(*snap);

        ImNodes::MiniMap(0.18f, ImNodesMiniMapLocation_BottomRight);

        ImNodes::EndNodeEditor();
        ImNodes::PopStyleVar();
        s_apply_layout = false;

        RenderHoverTooltips(*snap);

        ImGui::Separator();
        RenderResourceTable(*snap);

        ImGui::End();
    }

} // dodoe

#endif//DODOE_DEBUG_ENABLED
