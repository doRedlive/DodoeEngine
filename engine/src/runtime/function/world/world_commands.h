// do@Redlive

#pragma once

#include "dopch.h"

#include "entity.h"
#include "registry.h"
#include "runtime/core/container/command_list.h"

namespace dodoe {

    class WorldCommands : public CommandList<Registry> {
    public:
        void destroyEntity(Entity entity);

        template<typename T, typename... Args>
        void emplaceComponent(Entity entity, Args&&... args);

        template<typename T>
        void removeComponent(Entity entity);

        void apply(Registry& registry) { execute(registry); }

    private:
        struct DestroyCmd : CommandImpl<DestroyCmd> {
            Entity entity;
            explicit DestroyCmd(Entity e) : entity(e) {}
            void execute(Registry& reg) const { reg.destroy(entity); }
        };

        template<typename T>
        struct EmplaceCmd : CommandImpl<EmplaceCmd<T>> {
            Entity entity;
            T component;

            template<typename... Args>
            EmplaceCmd(Entity e, Args&&... args)
                : entity(e), component(std::forward<Args>(args)...) {}

            void execute(Registry& reg) const {
                reg.emplace<T>(entity, component);
            }
        };

        template<typename T>
        struct RemoveCmd : CommandImpl<RemoveCmd<T>> {
            Entity entity;
            explicit RemoveCmd(Entity e) : entity(e) {}
            void execute(Registry& reg) const { reg.remove<T>(entity); }
        };
    };

    inline void WorldCommands::destroyEntity(Entity entity) {
        enqueue<DestroyCmd>(entity);
    }

    template<typename T, typename... Args>
    void WorldCommands::emplaceComponent(Entity entity, Args&&... args) {
        enqueue<EmplaceCmd<T>>(entity, std::forward<Args>(args)...);
    }

    template<typename T>
    void WorldCommands::removeComponent(Entity entity) {
        enqueue<RemoveCmd<T>>(entity);
    }

} // namespace dodoe
