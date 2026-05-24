// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/file/file_system.h"

namespace cakery {

    struct ProjectManagerPanelCreateInfo {

    };

    class ProjectManagerPanel : public dodoe::Managed<ProjectManagerPanel, ProjectManagerPanelCreateInfo> {
        friend class dodoe::Managed<ProjectManagerPanel, ProjectManagerPanelCreateInfo>;

        struct ProjectEntry {
            std::string name;
            std::string project_file;
            bool pinned{false};
            uint64_t last_opened_unix{0};
        };

        std::vector<ProjectEntry> m_projects{};
        std::vector<size_t> m_filtered_indices{};
        int m_selected_filtered{-1};
        std::string m_last_opened{};

        std::array<char, 128> m_search_buffer{};
        std::array<char, 64> m_new_name_buffer{};
        std::array<char, 260> m_new_location_buffer{};

        bool m_popup_new{false};
        bool m_popup_delete{false};
        bool m_delete_from_disk{false};
        bool m_request_enter_editor{false};

        inline static std::filesystem::path ConfigPath = 
            dodoe::FileSystem::EngineResPath / "configs" / "project_manager_config.json";

    public:
        [[nodiscard]] bool draw();

    private:
        bool initialize(const ProjectManagerPanelCreateInfo& info);
        void shutdown();

        bool readConfig();
        void writeConfig() const;

        bool newProject();
        void delProject();
        void openProject();

        void drawSearchBar();
        void drawProjectList();
        void drawProjectDetails();
        void drawActions();
        void drawPopups();

        [[nodiscard]] bool hasSelection() const;
        [[nodiscard]] size_t selectedProjectIndex() const;
        [[nodiscard]] ProjectEntry* selectedProject();
        [[nodiscard]] const ProjectEntry* selectedProject() const;
        [[nodiscard]] bool loadProjectFromFile(const std::string& project_file);
        [[nodiscard]] bool createProjectOnDisk(const std::string& name, const std::string& location_dir);

        void rebuildFilteredIndices();
        void selectLastOpenedProject();
        void markProjectOpened(const std::string& normalized_project_file);
        void upsertProjectEntry(ProjectEntry entry);
        void syncDefaultNewProjectLocation();
    };

} // cakery
