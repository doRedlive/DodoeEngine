//
// Created by GreenMuffin on 2026/2/22.
//

#ifndef DODOE_COMPONENT_DB_H
#define DODOE_COMPONENT_DB_H

#include "dopch.h"

#include "entt/entt.hpp"

namespace dodoe {

    struct ComponentInfo {
        entt::id_type type_id;
        std::string name;
    };

	class ComponentDB {
        using AddComponentFunc = std::function<void(entt::registry&, entt::entity)>;
	public:
		static ComponentDB& instance() {
			static ComponentDB db;
			return db;
		}

        template<typename T>
        void register_component(const std::string& name) {
            entt::id_type comp_type = entt::type_id<T>().hash();
            names_by_type_umap_[comp_type] = name;
            types_by_name_umap_[name] = comp_type;
            all_infos_.emplace_back(comp_type, name);
            add_functions_[name] = [](entt::registry& reg, entt::entity entt) {
                reg.emplace_or_replace<T>(entt);
            };
        }

        AddComponentFunc add_component_func(const std::string& name) {
            if (!add_functions_.contains(name)) {
                DoError("No this component {}", name);
                return nullptr;
            }
            return add_functions_.at(name);
        }

        std::optional<entt::id_type> type_from_name(const std::string& name) const {
            auto it = types_by_name_umap_.find(name);
            if (it != types_by_name_umap_.end())
                return it->second;
            return std::nullopt;
        }

        std::optional<std::string> name_from_type(entt::id_type id) const {
            auto it = names_by_type_umap_.find(id);
            if (it != names_by_type_umap_.end())
                return it->second;
            return std::nullopt;
        }

        const std::vector<ComponentInfo>& all_components() const {
            return all_infos_;
        }

    private:
        std::unordered_map<std::string, AddComponentFunc> add_functions_;
        std::unordered_map<entt::id_type, std::string> names_by_type_umap_;
        std::unordered_map<std::string, entt::id_type> types_by_name_umap_;
        std::vector<ComponentInfo> all_infos_;
	};
} // dodoe

#endif//DODOE_COMPONENT_DB_H
