//
// Created by GreenMuffin on 2026/2/20.
//

#include "project.h"

#include "project_serializer.h"

namespace dodoe {
	Ref<Project> Project::Create() {
		active_project_ = create_ref<Project>();
		return active_project_;
	}

	Ref<Project> Project::Load(const std::filesystem::path& path) {
		const Ref<Project> project = create_ref<Project>();

		if (ProjectSerializer serializer(project); serializer.deserialize(path)) {
			project->project_directory_ = path.parent_path();
			active_project_ = project;
			return active_project_;
		}

		return nullptr;
	}

	bool Project::Save(const std::filesystem::path& path) {
		if (ProjectSerializer serializer(active_project_); serializer.serialize(path)) {
			active_project_->project_directory_ = path.parent_path();
			return true;
		}
		return false;
	}	
}

