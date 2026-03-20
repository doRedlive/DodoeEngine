//
// Created by GreenMuffin on 2026/2/20.
//

#include "project_serializer.h"

#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace dodoe {
	ProjectSerializer::ProjectSerializer(Ref<Project> proj) : project_(proj) { }

	bool ProjectSerializer::serialize(const std::filesystem::path& file_path) {
		const auto& config = project_->config();

		json root;
		root["Project"] = {
			{"Name", config.name},
			{"StartScene", config.start_scene_path.string()},
			{"AssetDirectory", config.asset_directory.string()},
			{"ScriptModulePath", config.script_module_path.string()}
		};

		std::ofstream fout(file_path);
		if (!fout.is_open()) {
			DoError("Failed to open project file for writing: {}", file_path.string());
			return false;
		}

		fout << root.dump(4);
		return true;
	}

	bool ProjectSerializer::deserialize(const std::filesystem::path& file_path) {
		auto& config = project_->config();

		json data;
		try {
			std::ifstream fin(file_path);
			if (!fin.is_open()) {
				DoError("Failed to open project file for reading: {}", file_path.string());
				return false;
			}
			fin >> data;
		}
		catch (const json::exception& e) {
			DoError("Failed to parse project file {}: {}", file_path.string(), e.what());
			return false;
		}

		if (!data.contains("Project") || !data["Project"].is_object()) {
			DoError("No project");
			return false;
		}

		const auto& project_node = data["Project"];

		if (!project_node.contains("Name") || !project_node.contains("StartScene") ||
			!project_node.contains("AssetDirectory") || !project_node.contains("ScriptModulePath")) {
			DoError("Project file missing required fields: {}", file_path.string());
			return false;
		}

		config.name = project_node["Name"].get<std::string>();
		config.start_scene_path = project_node["StartScene"].get<std::string>();
		config.asset_directory = project_node["AssetDirectory"].get<std::string>();
		config.script_module_path = project_node["ScriptModulePath"].get<std::string>();

		return true;
	}
}