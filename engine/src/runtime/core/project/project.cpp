//
// Created by GreenMuffin on 2026/2/20.
//

#include "project.h"

#include "project_serializer.h"
#include "runtime/platform/platform_tool.h"

namespace dodoe {
	Ref<Project> Project::Create() {
		m_active_project = create_ref<Project>();
		return m_active_project;
	}

	Ref<Project> Project::Load(const std::filesystem::path& path) {
		const Ref<Project> project = create_ref<Project>();

		if (ProjectSerializer serializer(project); serializer.deserialize(path)) {
			project->m_project_directory = path.parent_path();
			project->m_config.project_path = path.lexically_normal();
			m_active_project = project;
			return m_active_project;
		}

		return nullptr;
	}

	bool Project::Save(const std::filesystem::path& path) {
		m_active_project->m_config.project_path = path.lexically_normal();
		if (ProjectSerializer serializer(m_active_project); serializer.serialize(path)) {
			m_active_project->m_project_directory = path.parent_path();
			return true;
		}
		return false;
	}

} // dodoe
