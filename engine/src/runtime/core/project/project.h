// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

	struct ProjectConfig {
		std::string name{ "Untitled" };

		std::filesystem::path project_path;
		std::filesystem::path asset_directory;
		std::string start_scene_name;
	};

	class Project {
	public:
		static const std::filesystem::path& ProjectDirectory() {
			return m_active_project->m_project_directory;
		}

		static std::filesystem::path AssetDirectory() {
			return ProjectDirectory() / m_active_project->m_config.asset_directory;
		}

		static std::filesystem::path BinariesDirectory() {
			return ProjectDirectory() / "Binaries";
		}

		static std::filesystem::path ScriptAssemblyPath() {
			return BinariesDirectory() / (m_active_project->m_config.name + ".dll");
		}

		ProjectConfig& config() { return m_config; }

		static Ref<Project> ActiveProject() { return m_active_project; }

		static Ref<Project> Create();
		static Ref<Project> Load(const std::filesystem::path& path);
		static bool 		Save(const std::filesystem::path& path);

	private:
		ProjectConfig m_config{};
		std::filesystem::path m_project_directory;

		inline static Ref<Project> m_active_project;
	};

} // dodoe
