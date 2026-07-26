//
// Created by GreenMuffin on 2026/2/20.
//

#include "project_serializer.h"

#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

using Json = nlohmann::json;

namespace dodoe {
	ProjectSerializer::ProjectSerializer(Ref<Project> proj) : project_(proj) { }

	bool ProjectSerializer::serialize(const FsPath& file_path) {
		const auto& config = project_->config();
		const FsPath configured_project_path = config.project_path.empty() ? file_path.filename() : config.project_path;
		const FsPath project_path =
			configured_project_path.is_absolute() ? configured_project_path.filename() : configured_project_path;

		Json root;
		root["Project"] = {
			{"Name", config.name},
			{"ProjectPath", project_path.generic_string()},
			{"AssetDirectory", config.asset_directory.string()},
			{"StartSceneName", config.start_scene_name}
		};

		std::ofstream fout(file_path);
		if (!fout.is_open()) {
			DO_ERROR("Failed to open project file for writing: {}", file_path.string());
			return false;
		}

		fout << root.dump(4);
		return true;
	}

	bool ProjectSerializer::deserialize(const FsPath& file_path) {
		auto& config = project_->config();

		Json data;
		try {
			std::ifstream fin(file_path);
			if (!fin.is_open()) {
				DO_ERROR("Failed to open project file for reading: {}", file_path.string());
				return false;
			}
			fin >> data;
		}
		catch (const Json::exception& e) {
			DO_ERROR("Failed to parse project file {}: {}", file_path.string(), e.what());
			return false;
		}

		if (!data.contains("Project") || !data["Project"].is_object()) {
			DO_ERROR("No project");
			return false;
		}

		const auto& project_node = data["Project"];

		if (!project_node.contains("Name") || !project_node.contains("AssetDirectory")) {
			DO_ERROR("Project file missing required fields: {}", file_path.string());
			return false;
		}

		config.name = project_node["Name"].get<String>();
		if (project_node.contains("ProjectPath") && project_node["ProjectPath"].is_string()) {
			config.project_path = project_node["ProjectPath"].get<String>();
		} else {
			config.project_path = file_path.lexically_normal();
		}
		config.asset_directory = project_node["AssetDirectory"].get<String>();
		if (project_node.contains("StartSceneName") && project_node["StartSceneName"].is_string()) {
			config.start_scene_name = project_node["StartSceneName"].get<String>();
		} else if (project_node.contains("StartScene") && project_node["StartScene"].is_string()) {
			config.start_scene_name = FsPath(project_node["StartScene"].get<String>()).stem().string();
		} else {
			config.start_scene_name.clear();
		}

		return true;
	}
}
