// do@Redlive

#pragma once

#include "dopch.h"

#include "entity.h"
#include "registry.h"
#include "scene.h"
#include "runtime/core/container/command_list.h"

namespace dodoe {

    struct CommandContext {
        Scene* scene{ nullptr };
        Registry* registry{ nullptr };

        std::function<void(UInt64 entity_uuid, const String& type_name)> add_managed;
        std::function<void(UInt64 entity_uuid, const String& type_name)> remove_managed;

        [[nodiscard]] Entity resolveEntity(const UUID& uuid) const {
            if (!scene) return {};
            return scene->tryGetEntityByUUID(uuid);
        }

        void report(const char* op, UInt64 uuid) const {
            DO_DEBUG("CommandBuffer: {} dropped, entity {} not found", op, uuid);
        }
    };

    class WorldCommands : public CommandList<CommandContext> {
    public:
        void destroyEntity(const UUID& uuid);
        void destroyEntity(UInt64 uuid) { destroyEntity(UUID(uuid)); }

        template<typename T, typename... Args>
        void emplaceComponent(const UUID& uuid, Args&&... args);

        template<typename T>
        void removeComponent(const UUID& uuid);

        void addComponentByName(const UUID& uuid, std::function<void(Entity)> apply);
        void removeComponentByName(const UUID& uuid, std::function<void(Entity)> apply);

        void addManagedComponent(const UUID& uuid, const String& type_name);
        void removeManagedComponent(const UUID& uuid, const String& type_name);

        void apply(CommandContext context) {
            execute(context);
            replayPendingWrites();
        }

        bool bufferComponentWrite(std::function<void()> write) {
            if (!write || m_replaying) return false;
            std::lock_guard<std::mutex> lock(*m_writes_mutex);
            m_pending_writes.push_back(std::move(write));
            return true;
        }

    private:
        Scope<std::mutex> m_writes_mutex{ create_scope<std::mutex>() };
        DynamicArray<std::function<void()>> m_pending_writes;
        bool m_replaying{ false };

        void replayPendingWrites() {
            if (m_replaying) return;
            m_replaying = true;
            DynamicArray<std::function<void()>> writes;
            {
                std::lock_guard<std::mutex> lock(*m_writes_mutex);
                writes.swap(m_pending_writes);
            }
            for (auto& write : writes) write();
            m_replaying = false;
        }

        struct DestroyCmd : CommandImpl<DestroyCmd> {
            UUID uuid;
            explicit DestroyCmd(UUID u) : uuid(u) {}
            void execute(CommandContext& context) const {
                Entity entity = context.resolveEntity(uuid);
                if (!entity.valid()) {
                    context.report("destroy", static_cast<UInt64>(uuid));
                    return;
                }
                if (context.scene) context.scene->destroyEntity(entity);
            }
        };

        template<typename T>
        struct EmplaceCmd : CommandImpl<EmplaceCmd<T>> {
            UUID uuid;
            T component;

            template<typename... Args>
            EmplaceCmd(UUID u, Args&&... args)
                : uuid(u), component(std::forward<Args>(args)...) {}

            void execute(CommandContext& context) const {
                Entity entity = context.resolveEntity(uuid);
                if (!entity.valid() || !context.registry) return;
                if (context.registry->all_of<T>(entity)) return;
                context.registry->emplace<T>(entity, component);
            }
        };

        template<typename T>
        struct RemoveCmd : CommandImpl<RemoveCmd<T>> {
            UUID uuid;
            explicit RemoveCmd(UUID u) : uuid(u) {}
            void execute(CommandContext& context) const {
                Entity entity = context.resolveEntity(uuid);
                if (!entity.valid() || !context.registry) return;
                if (context.registry->all_of<T>(entity)) context.registry->remove<T>(entity);
            }
        };

        struct AddComponentByNameCmd : CommandImpl<AddComponentByNameCmd> {
            UUID uuid;
            std::function<void(Entity)> apply;
            AddComponentByNameCmd(UUID u, std::function<void(Entity)> fn)
                : uuid(u), apply(std::move(fn)) {}
            void execute(CommandContext& context) const {
                Entity entity = context.resolveEntity(uuid);
                if (!entity.valid() || !apply) return;
                apply(entity);
            }
        };

        struct RemoveComponentByNameCmd : CommandImpl<RemoveComponentByNameCmd> {
            UUID uuid;
            std::function<void(Entity)> apply;
            RemoveComponentByNameCmd(UUID u, std::function<void(Entity)> fn)
                : uuid(u), apply(std::move(fn)) {}
            void execute(CommandContext& context) const {
                Entity entity = context.resolveEntity(uuid);
                if (!entity.valid() || !apply) return;
                apply(entity);
            }
        };

        struct AddManagedCmd : CommandImpl<AddManagedCmd> {
            UUID uuid;
            String type_name;
            AddManagedCmd(UUID u, String tn) : uuid(u), type_name(std::move(tn)) {}
            void execute(CommandContext& context) const {
                Entity entity = context.resolveEntity(uuid);
                if (!entity.valid()) return;
                if (context.add_managed) context.add_managed(static_cast<UInt64>(uuid), type_name);
            }
        };

        struct RemoveManagedCmd : CommandImpl<RemoveManagedCmd> {
            UUID uuid;
            String type_name;
            RemoveManagedCmd(UUID u, String tn) : uuid(u), type_name(std::move(tn)) {}
            void execute(CommandContext& context) const {
                Entity entity = context.resolveEntity(uuid);
                if (!entity.valid()) return;
                if (context.remove_managed) context.remove_managed(static_cast<UInt64>(uuid), type_name);
            }
        };
    };

    inline void WorldCommands::destroyEntity(const UUID& uuid) {
        enqueue<DestroyCmd>(uuid);
    }

    template<typename T, typename... Args>
    void WorldCommands::emplaceComponent(const UUID& uuid, Args&&... args) {
        enqueue<EmplaceCmd<T>>(uuid, std::forward<Args>(args)...);
    }

    template<typename T>
    void WorldCommands::removeComponent(const UUID& uuid) {
        enqueue<RemoveCmd<T>>(uuid);
    }

    inline void WorldCommands::addComponentByName(const UUID& uuid, std::function<void(Entity)> apply) {
        enqueue<AddComponentByNameCmd>(uuid, std::move(apply));
    }

    inline void WorldCommands::removeComponentByName(const UUID& uuid, std::function<void(Entity)> apply) {
        enqueue<RemoveComponentByNameCmd>(uuid, std::move(apply));
    }

    inline void WorldCommands::addManagedComponent(const UUID& uuid, const String& type_name) {
        enqueue<AddManagedCmd>(uuid, type_name);
    }

    inline void WorldCommands::removeManagedComponent(const UUID& uuid, const String& type_name) {
        enqueue<RemoveManagedCmd>(uuid, type_name);
    }

} // namespace dodoe
