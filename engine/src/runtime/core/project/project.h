// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

	struct ProjectConfig {
		std::string name{ "Untitled" };

		FsPath project_path;
		FsPath asset_directory;
		std::string start_scene_name;
	};

	class DODOE_API Project {
	public:
		static const FsPath& ProjectDirectory() {
			return m_active_project->m_project_directory;
		}

		static FsPath AssetDirectory() {
			return ProjectDirectory() / m_active_project->m_config.asset_directory;
		}

		static FsPath BinariesDirectory() {
			return ProjectDirectory() / "Binaries";
		}

		static FsPath ScriptAssemblyPath() {
			return BinariesDirectory() / (m_active_project->m_config.name + ".dll");
		}

		ProjectConfig& config() { return m_config; }

		static Ref<Project> ActiveProject() { return m_active_project; }

		static Ref<Project> Create();
		static Ref<Project> Load(const FsPath& path);
		static bool 		Save(const FsPath& path);

	private:
		ProjectConfig m_config{};
		FsPath m_project_directory;

		inline static Ref<Project> m_active_project;
	};

} // dodoe
