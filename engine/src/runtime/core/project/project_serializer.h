//
// Created by GreenMuffin on 2026/2/20.
//

#ifndef DODOE_PROJECT_SERIALIZER_H
#define DODOE_PROJECT_SERIALIZER_H

#include "dopch.h"

#include "project.h"

namespace dodoe {
	class ProjectSerializer {
	public:
		explicit ProjectSerializer(Ref<Project> proj);

		bool serialize(const std::filesystem::path& file_path);
		bool deserialize(const std::filesystem::path& file_path);
		
	private:
		Ref<Project> project_;
	};
}

#endif//DODOE_PROJECT_SERIALIZER_H
