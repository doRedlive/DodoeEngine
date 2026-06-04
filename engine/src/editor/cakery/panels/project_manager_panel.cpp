// do@Redlive

#include "project_manager_panel.h"

#include "cakery/helper/lang_config.h"

#include "runtime/core/project/project.h"
#include "runtime/core/project/project_serializer.h"
#include "runtime/core/utils/json.h"
#include "runtime/function/world/world.h"
#include "runtime/resource/file/file_system.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/res_type/scene_res.h"
#include "runtime/platform/platform_tool.h"

#include "imgui/imgui.h"

#include <cctype>
#include <chrono>
#include <fstream>

namespace cakery {

    namespace {

        uint64_t NowUnixSeconds() {
            const auto now = std::chrono::system_clock::now();
            const auto secs = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
            return secs > 0 ? static_cast<uint64_t>(secs) : 0ull;
        }

        std::filesystem::path ResolveProjectFile(const std::string& path_str) {
            std::filesystem::path p(path_str);
            if (p.is_relative()) {
                p = std::filesystem::path(dodoe::FileSystem::GetCWD()) / p;
            }
            return p.lexically_normal();
        }

        bool IsEmptyOrWhitespace(const char* s) {
            if (!s) return true;
            for (const char* p = s; *p; ++p) {
                if (!std::isspace(static_cast<unsigned char>(*p))) {
                    return false;
                }
            }
            return true;
        }

        bool WriteDefaultSceneFile(const std::filesystem::path& assets_root, const std::string& scene_name) {
            const auto scenes_dir = assets_root / "Scenes";
            const auto scene_file = scenes_dir / (scene_name + ".doscn");

            std::error_code ec;
            std::filesystem::create_directories(scenes_dir, ec);

            auto bootstrap_world = dodoe::World::Create({"Bootstrap"});
            if (!bootstrap_world) {
                return false;
            }

            dodoe::Scene* scene = bootstrap_world->createScene(scene_name);
            if (!scene) {
                dodoe::World::Destroy(bootstrap_world);
                return false;
            }

            dodoe::SceneRes scene_res = scene->serialize();
            dodoe::World::Destroy(bootstrap_world);

            std::ofstream scene_out(scene_file);
            if (!scene_out.is_open()) {
                return false;
            }

            scene_out << dodoe::Serializer::write(scene_res).dump(4);
            return true;
        }


        std::string Trim(const std::string& str) {
            size_t start = 0;
            while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
                ++start;
            }
            size_t end = str.size();
            while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
                --end;
            }
            return str.substr(start, end - start);
        }

    } // namespace

    ProjectManagerPanel::ProjectManagerPanel(EditorPanelDescriptor descriptor)
        : EditorPanel(std::move(descriptor)) {
        readConfig();
    }

    ProjectManagerPanel::~ProjectManagerPanel() = default;

    void ProjectManagerPanel::onDraw(const EditorPanelContext& context) {
        (void)context;
        if (drawImpl()) {
            if (context.request_enter_editor) {
                context.request_enter_editor();
            }
        }
    }

    bool ProjectManagerPanel::drawImpl() {
        const ImGuiIO& io = ImGui::GetIO();
        const ImVec2 window_pos(0.0f, 0.0f);
        const ImVec2 window_size = (io.DisplaySize.x > 0.0f && io.DisplaySize.y > 0.0f)
            ? io.DisplaySize
            : ImVec2(1280.0f, 720.0f);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(window_size, ImGuiCond_Always);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar;
        ImGui::Begin(LangConfig::TR("PROJECT_MANAGER"), nullptr, flags);

        ImGui::TextUnformatted(LangConfig::TR("PROJECT_MANAGER_SUBTITLE"));
        ImGui::Separator();
        drawSearchBar();
        ImGui::Spacing();

        const ImGuiStyle& style = ImGui::GetStyle();
        const float footer_height = ImGui::GetFrameHeightWithSpacing() + style.ItemSpacing.y + style.WindowPadding.y * 2.0f;

        drawProjectList(footer_height);
        ImGui::SameLine();
        drawProjectDetails(footer_height);
        ImGui::Separator();
        drawActions();
        drawPopups();
        ImGui::End();

        const bool request = m_request_enter_editor;
        m_request_enter_editor = false;
        return request;
    }

    bool ProjectManagerPanel::newProject() {
        const std::string name = Trim(std::string(m_new_name_buffer.data()));
        std::string location = Trim(std::string(m_new_location_buffer.data()));
        if (location.empty()) {
            location = dodoe::PlatformTool::OpenDirectoryDialog(dodoe::FileSystem::GetDocumentsPathString());
            if (!location.empty()) {
                std::snprintf(m_new_location_buffer.data(), m_new_location_buffer.size(), "%s", location.c_str());
            }
        }
        if (name.empty() || location.empty()) {
            return false;
        }
        return createProjectOnDisk(name, location);
    }

    void ProjectManagerPanel::delProject() {
        if (m_selected_filtered < 0 || m_selected_filtered >= static_cast<int>(m_filtered_indices.size())) {
            return;
        }

        const size_t index = m_filtered_indices[static_cast<size_t>(m_selected_filtered)];
        if (index >= m_projects.size()) {
            return;
        }

        const auto project_file = ResolveProjectFile(m_projects[index].project_file);
        const auto project_dir = project_file.parent_path();

        m_projects.erase(m_projects.begin() + static_cast<std::ptrdiff_t>(index));
        m_selected_filtered = -1;

        if (m_last_opened == dodoe::FileSystem::NormalizePath(project_file)) {
            m_last_opened.clear();
        }

        if (m_delete_from_disk && dodoe::FileSystem::DirExists(project_dir)) {
            std::error_code ec;
            std::filesystem::remove_all(project_dir, ec);
        }

        writeConfig();
        rebuildFilteredIndices();
    }

    void ProjectManagerPanel::openProject() {
        if (m_selected_filtered < 0 || m_selected_filtered >= static_cast<int>(m_filtered_indices.size())) {
            return;
        }

        const auto& entry = m_projects[m_filtered_indices[static_cast<size_t>(m_selected_filtered)]];
        (void)loadProjectFromFile(entry.project_file);
    }

    bool ProjectManagerPanel::readConfig() {
        m_projects.clear();
        m_filtered_indices.clear();
        m_selected_filtered = -1;
        m_last_opened.clear();

        const auto path = ConfigPath;
        if (!dodoe::FileSystem::FileExists(path)) {
            syncDefaultNewProjectLocation();
            rebuildFilteredIndices();
            return false;
        }

        dodoe::Json root;
        try {
            std::ifstream fin(path);
            fin >> root;
        }
        catch (...) {
            rebuildFilteredIndices();
            return false;
        }

        if (root.contains("lastOpened") && root["lastOpened"].is_string()) {
            m_last_opened = root["lastOpened"].get<std::string>();
        }

        if (root.contains("projects") && root["projects"].is_array()) {
            for (const auto& node : root["projects"]) {
                if (!node.is_object()) continue;
                if (!node.contains("name") || !node.contains("projectFile")) continue;
                if (!node["name"].is_string() || !node["projectFile"].is_string()) continue;

                ProjectEntry e;
                e.name = node["name"].get<std::string>();
                e.project_file = node["projectFile"].get<std::string>();
                if (node.contains("pinned") && node["pinned"].is_boolean()) {
                    e.pinned = node["pinned"].get<bool>();
                }
                if (node.contains("lastOpened") && node["lastOpened"].is_number_unsigned()) {
                    e.last_opened_unix = node["lastOpened"].get<uint64_t>();
                }
                m_projects.push_back(std::move(e));
            }
        }

        syncDefaultNewProjectLocation();

        rebuildFilteredIndices();
        selectLastOpenedProject();

        return true;
    }

    void ProjectManagerPanel::writeConfig() const {
        dodoe::Json root;
        root["version"] = 1;
        root["lastOpened"] = m_last_opened;
        root["projects"] = dodoe::Json::array();

        for (const auto& e : m_projects) {
            dodoe::Json node;
            node["name"] = e.name;
            node["projectFile"] = e.project_file;
            node["pinned"] = e.pinned;
            node["lastOpened"] = e.last_opened_unix;
            root["projects"].push_back(std::move(node));
        }

        const auto path = ConfigPath;
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream fout(path);
        if (!fout.is_open()) {
            return;
        }
        fout << root.dump(4);
    }

    void ProjectManagerPanel::rebuildFilteredIndices() {
        m_filtered_indices.clear();

        const std::string filter = Trim(std::string(m_search_buffer.data()));
        for (size_t i = 0; i < m_projects.size(); ++i) {
            const auto& e = m_projects[i];
            if (filter.empty()) {
                m_filtered_indices.push_back(i);
                continue;
            }

            const std::string name = e.name;
            const std::string path = e.project_file;
            if (name.find(filter) != std::string::npos || path.find(filter) != std::string::npos) {
                m_filtered_indices.push_back(i);
            }
        }

        std::stable_sort(m_filtered_indices.begin(), m_filtered_indices.end(),
            [&](const size_t a, const size_t b) {
                const auto& ea = m_projects[a];
                const auto& eb = m_projects[b];
                if (ea.pinned != eb.pinned) {
                    return ea.pinned > eb.pinned;
                }
                return ea.last_opened_unix > eb.last_opened_unix;
            });

        if (m_selected_filtered >= static_cast<int>(m_filtered_indices.size())) {
            m_selected_filtered = -1;
        }
    }

    bool ProjectManagerPanel::loadProjectFromFile(const std::string& project_file) {
        const auto resolved = ResolveProjectFile(project_file);
        if (!dodoe::FileSystem::FileExists(resolved)) {
            return false;
        }

        const auto loaded = dodoe::Project::Load(resolved);
        if (!loaded) {
            return false;
        }

        ProjectEntry entry;
        entry.name = dodoe::Project::ActiveProject()->config().name;
        entry.project_file = dodoe::FileSystem::NormalizePath(resolved);
        upsertProjectEntry(std::move(entry));
        markProjectOpened(dodoe::FileSystem::NormalizePath(resolved));

        writeConfig();
        m_request_enter_editor = true;
        return true;
    }

    bool ProjectManagerPanel::createProjectOnDisk(const std::string& name, const std::string& location_dir) {
        const auto project_root = std::filesystem::path(location_dir) / name;
        const auto project_file = project_root / (name + ".doproj");
        const auto assets_root = project_root / "Assets";
        const auto scenes_root = assets_root / "Scenes";
        const auto binaries_dir = project_root / "Binaries";
        const auto configs_dir = project_root / "Configs";
        const std::string start_scene_name = "Main";

        if (dodoe::FileSystem::DirExists(project_root) || dodoe::FileSystem::FileExists(project_file)) {
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(assets_root, ec);
        std::filesystem::create_directories(scenes_root, ec);
        std::filesystem::create_directories(binaries_dir, ec);
        std::filesystem::create_directories(configs_dir, ec);

        {
            dodoe::Json db;
            db["version"] = 1;
            db["assets"] = dodoe::Json::object();
            std::ofstream db_file(configs_dir / "asset_database.json");
            if (db_file.is_open()) {
                db_file << db.dump(4);
            }
        }

        dodoe::Project::Create();
        auto proj = dodoe::Project::ActiveProject();

        auto& cfg = proj->config();
        cfg.name = name;
        cfg.project_path = project_file.lexically_normal();
        cfg.asset_directory = "Assets";
        cfg.start_scene_name = start_scene_name;

        if (!WriteDefaultSceneFile(assets_root, start_scene_name)) {
            return false;
        }

        dodoe::ProjectSerializer serializer(proj);
        if (!serializer.serialize(project_file)) {
            return false;
        }

        ProjectEntry entry;
        entry.name = name;
        entry.project_file = dodoe::FileSystem::NormalizePath(project_file);
        entry.last_opened_unix = NowUnixSeconds();
        m_projects.push_back(std::move(entry));
        m_last_opened = dodoe::FileSystem::NormalizePath(project_file);
        writeConfig();
        rebuildFilteredIndices();

        return loadProjectFromFile(m_last_opened);
    }

    bool ProjectManagerPanel::hasSelection() const {
        return m_selected_filtered >= 0 && m_selected_filtered < static_cast<int>(m_filtered_indices.size());
    }

    size_t ProjectManagerPanel::selectedProjectIndex() const {
        return hasSelection() ? m_filtered_indices[static_cast<size_t>(m_selected_filtered)] : m_projects.size();
    }

    ProjectManagerPanel::ProjectEntry* ProjectManagerPanel::selectedProject() {
        const size_t index = selectedProjectIndex();
        return index < m_projects.size() ? &m_projects[index] : nullptr;
    }

    const ProjectManagerPanel::ProjectEntry* ProjectManagerPanel::selectedProject() const {
        const size_t index = selectedProjectIndex();
        return index < m_projects.size() ? &m_projects[index] : nullptr;
    }

    void ProjectManagerPanel::selectLastOpenedProject() {
        if (m_last_opened.empty()) {
            return;
        }

        const auto normalized_last = dodoe::FileSystem::NormalizePath(ResolveProjectFile(m_last_opened));
        for (size_t i = 0; i < m_filtered_indices.size(); ++i) {
            const auto& entry = m_projects[m_filtered_indices[i]];
            if (dodoe::FileSystem::NormalizePath(ResolveProjectFile(entry.project_file)) == normalized_last) {
                m_selected_filtered = static_cast<int>(i);
                return;
            }
        }
    }

    void ProjectManagerPanel::markProjectOpened(const std::string& normalized_project_file) {
        m_last_opened = normalized_project_file;
        const uint64_t now = NowUnixSeconds();
        for (auto& entry : m_projects) {
            if (dodoe::FileSystem::NormalizePath(ResolveProjectFile(entry.project_file)) == normalized_project_file) {
                entry.last_opened_unix = now;
            }
        }
    }

    void ProjectManagerPanel::upsertProjectEntry(ProjectEntry entry) {
        const std::string normalized_path = dodoe::FileSystem::NormalizePath(ResolveProjectFile(entry.project_file));
        for (auto& existing : m_projects) {
            if (dodoe::FileSystem::NormalizePath(ResolveProjectFile(existing.project_file)) == normalized_path) {
                existing.name = std::move(entry.name);
                existing.project_file = normalized_path;
                return;
            }
        }

        entry.project_file = normalized_path;
        m_projects.push_back(std::move(entry));
    }

    void ProjectManagerPanel::syncDefaultNewProjectLocation() {
        if (IsEmptyOrWhitespace(m_new_location_buffer.data())) {
            std::snprintf(m_new_location_buffer.data(), m_new_location_buffer.size(), "%s",
                dodoe::FileSystem::GetDocumentsPathString().c_str());
        }
    }

    void ProjectManagerPanel::drawSearchBar() {
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##ProjectSearch", LangConfig::TR("PM_SEARCH_HINT"),
            m_search_buffer.data(), static_cast<int>(m_search_buffer.size()));
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            rebuildFilteredIndices();
            selectLastOpenedProject();
        }
    }

    void ProjectManagerPanel::drawProjectList(float footer_height) {
        constexpr float kLeftWidth = 360.0f;
        ImGui::BeginChild("##ProjectList", ImVec2(kLeftWidth, -footer_height), true);
        ImGui::TextUnformatted(LangConfig::TR("PM_RECENT"));
        ImGui::Separator();

        for (int i = 0; i < static_cast<int>(m_filtered_indices.size()); ++i) {
            const auto& entry = m_projects[m_filtered_indices[static_cast<size_t>(i)]];
            std::string label = entry.pinned ? "★ " + entry.name : entry.name;
            label += "##" + entry.project_file;

            if (ImGui::Selectable(label.c_str(), m_selected_filtered == i, ImGuiSelectableFlags_AllowDoubleClick)) {
                m_selected_filtered = i;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    openProject();
                }
            }
        }

        ImGui::EndChild();
    }

    void ProjectManagerPanel::drawProjectDetails(float footer_height) {
        ImGui::BeginChild("##ProjectDetail", ImVec2(0.0f, -footer_height), true);
        ImGui::TextUnformatted(LangConfig::TR("PM_DETAILS"));
        ImGui::Separator();

        if (auto* entry = selectedProject()) {
            ImGui::Text("%s: %s", LangConfig::TR("PM_NAME"), entry->name.c_str());
            ImGui::Text("%s:", LangConfig::TR("PM_PATH"));
            ImGui::TextWrapped("%s", entry->project_file.c_str());
            ImGui::Spacing();

            bool pinned = entry->pinned;
            if (ImGui::Checkbox(LangConfig::TR("PM_PINNED"), &pinned)) {
                entry->pinned = pinned;
                writeConfig();
                rebuildFilteredIndices();
                selectLastOpenedProject();
            }
        } else {
            ImGui::TextDisabled("%s", LangConfig::TR("PM_NO_SELECTION"));
        }

        ImGui::EndChild();
    }

    void ProjectManagerPanel::drawActions() {
        if (ImGui::Button(LangConfig::TR("PM_NEW"))) {
            m_popup_new = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(LangConfig::TR("PM_OPEN"))) {
            const std::string path = dodoe::PlatformTool::OpenProjectFileDialog();
            if (!path.empty()) {
                (void)loadProjectFromFile(path);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(LangConfig::TR("PM_DELETE"))) {
            m_popup_delete = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(LangConfig::TR("PM_REFRESH"))) {
            readConfig();
        }
    }

    void ProjectManagerPanel::drawPopups() {
        if (m_popup_new) {
            ImGui::OpenPopup("##NewProjectPopup");
            m_popup_new = false;
        }
        if (ImGui::BeginPopupModal("##NewProjectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted(LangConfig::TR("PM_NEW_HINT"));
            ImGui::SetNextItemWidth(360.0f);
            ImGui::InputText(LangConfig::TR("PM_NAME"), m_new_name_buffer.data(), static_cast<int>(m_new_name_buffer.size()));
            ImGui::SetNextItemWidth(480.0f);
            ImGui::InputText(LangConfig::TR("PM_LOCATION"), m_new_location_buffer.data(), static_cast<int>(m_new_location_buffer.size()));
            ImGui::SameLine();
            if (ImGui::Button("...")) {
                const std::string path = dodoe::PlatformTool::OpenDirectoryDialog(Trim(std::string(m_new_location_buffer.data())));
                if (!path.empty()) {
                    std::snprintf(m_new_location_buffer.data(), m_new_location_buffer.size(), "%s", path.c_str());
                }
            }
            if (ImGui::Button(LangConfig::TR("PM_CREATE"))) {
                if (newProject()) {
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(LangConfig::TR("PM_CANCEL"))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (m_popup_delete) {
            ImGui::OpenPopup("##DeleteProjectPopup");
            m_popup_delete = false;
            m_delete_from_disk = false;
        }
        if (ImGui::BeginPopupModal("##DeleteProjectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted(LangConfig::TR("PM_DELETE_CONFIRM"));
            ImGui::Checkbox(LangConfig::TR("PM_DELETE_FROM_DISK"), &m_delete_from_disk);
            if (ImGui::Button(LangConfig::TR("PM_DELETE"))) {
                delProject();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(LangConfig::TR("PM_CANCEL"))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

} // cakery
