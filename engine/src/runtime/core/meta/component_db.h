// do@Redlive

#pragma once

#include "entt/entt.hpp"

#include "dopch.h"

#include <cstddef>

namespace dodoe {

    class Entity;

    class ComponentDB {
    public:
        using HasFunc = bool (*)(Entity&);
        using GetPtrFunc = void* (*)(Entity&);
        using EditFunc = void (*)(Entity&);

        struct Entry {
            entt::id_type type{};
            std::string name{};
            bool addable{ true };

            HasFunc has{ nullptr };
            GetPtrFunc getPtr{ nullptr };
            EditFunc add{ nullptr };
            EditFunc remove{ nullptr };

            bool contains(Entity& entity) const;
            void* get(Entity& entity) const;
            bool canAdd() const { return addable && add != nullptr; }
        };

        static ComponentDB& self();

        const Entry* find(const std::string& name) const;
        const Entry* find(entt::id_type type) const;

        const std::vector<Entry>& entries() const;
        const std::vector<std::pair<entt::id_type, std::string>>& allComponents() const;

        bool hasComponent(Entity& entity, const std::string& name) const;
        void* getComponentPtr(Entity& entity, const std::string& name) const;
        bool addComponent(Entity& entity, const std::string& name) const;
        bool removeComponent(Entity& entity, const std::string& name) const;

    private:
        ComponentDB();

        template <typename T>
        void registerComponent(const std::string& name, bool addable = true);

        void registerBuiltinComponents();

        std::vector<Entry> m_entries{};
        std::vector<std::pair<entt::id_type, std::string>> m_addable_components{};
        std::unordered_map<std::string, std::size_t> m_name2index{};
        std::unordered_map<entt::id_type, std::size_t> m_typ2index{};
    };

} // dodoe

