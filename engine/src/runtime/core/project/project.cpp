//
// Created by GreenMuffin on 2026/2/20.
//

#include "project.h"

#include "project_serializer.h"

namespace dodoe {
	Ref<Project> dodoe::Project::create() {
		active_project_ = create_ref<Project>();
		return active_project_;
	}

	Ref<Project> dodoe::Project::load(const std::filesystem::path& path) {
		Ref<Project> project = create_ref<Project>();

		ProjectSerializer serializer(project);
		if (serializer.deserialize(path)) {
			project->project_directory_ = path.parent_path();
			active_project_ = project;
			return active_project_;
		}

		return nullptr;
	}

	bool dodoe::Project::save_active(const std::filesystem::path& path) {
		ProjectSerializer serializer(active_project_);
		if (serializer.serialize(path)) {
			active_project_->project_directory_ = path.parent_path();
			return true;
		}
		return false;
	}	
}

