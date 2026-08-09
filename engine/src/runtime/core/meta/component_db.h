// do@Redlive

#pragma once

#include "entt/entt.hpp"

#include "dopch.h"
#include "runtime/core/utils/json.h"

#include <cstddef>

namespace dodoe {

    class Entity;
    class Registry;

    class DODOE_API ComponentDB {
    public:
        using HasFunc = bool (*)(Entity&);
        using GetPtrFunc = void* (*)(Entity&);
        using EditFunc = void (*)(Entity&);
        using WriteJsonFunc = Json (*)(void*);
        using ReadJsonFunc = bool (*)(void*, const Json&);
        using WarmupPoolFunc = void (*)(Registry&);

        struct DODOE_API Entry {
            entt::id_type type{};
            String name{};
            bool addable{ true };

            HasFunc has{ nullptr };
            GetPtrFunc getPtr{ nullptr };
            EditFunc add{ nullptr };
            EditFunc remove{ nullptr };
            EditFunc markDirty{ nullptr };
            WriteJsonFunc writeJson{ nullptr };
            ReadJsonFunc readJson{ nullptr };
            WarmupPoolFunc warmupPool{ nullptr };

            bool contains(Entity& entity) const;
            void* get(Entity& entity) const;
            bool canAdd() const { return addable && add != nullptr; }
        };

        static ComponentDB& self();

        const Entry* find(const String& name) const;
        const Entry* find(entt::id_type type) const;

        const std::vector<Entry>& entries() const;
        const std::vector<std::pair<entt::id_type, String>>& allComponents() const;

        bool hasComponent(Entity& entity, const String& name) const;
        void* getComponentPtr(Entity& entity, const String& name) const;
        bool addComponent(Entity& entity, const String& name) const;
        bool removeComponent(Entity& entity, const String& name) const;
        bool markComponentDirty(Entity& entity, const String& name) const;

    private:
        ComponentDB();

        template <typename T>
        void registerComponent(const String& name, bool addable = true);

        void registerBuiltinComponents();

        std::vector<Entry> m_entries{};
        std::vector<std::pair<entt::id_type, String>> m_addable_components{};
        std::unordered_map<String, std::size_t> m_name2index{};
        std::unordered_map<entt::id_type, std::size_t> m_typ2index{};
    };

} // dodoe

