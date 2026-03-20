//
// Created by GreenMuffin on 2025/12/9.
//

#ifndef CAKERY_SCENE_HIERARCHY_HANDLER_H
#define CAKERY_SCENE_HIERARCHY_HANDLER_H
#include "command.h"
#include "runtime/core/world/game_object.h"

namespace cakery {
    class HierarchyPanel;
}

namespace cakery {
    class SceneHierarchyHandler {
    public:
        explicit SceneHierarchyHandler(HierarchyPanel& context);
        ~SceneHierarchyHandler();

        class SceneHierarchyCmd : public Command {
        public:
            explicit SceneHierarchyCmd(SceneHierarchyHandler& context) : context_(context) {}
            ~SceneHierarchyCmd() override = default;
            void execute() override = 0;
            void undo() override = 0;
        protected:
            SceneHierarchyHandler& context_;
        };

        class CreateGameObjectCmd final : public SceneHierarchyCmd {
        public:
            explicit CreateGameObjectCmd(SceneHierarchyHandler &context)
                : SceneHierarchyCmd(context) {
            }

            void execute() override;
            void undo() override;
        private:
            std::string game_object_name_{};
        };

        class DeleteGameObjectCmd final : public SceneHierarchyCmd {
        public:
            explicit DeleteGameObjectCmd(SceneHierarchyHandler &context, dodoe::GameObject* game_object)
                : SceneHierarchyCmd(context), game_object_(game_object) {
            }

            void execute() override;
            void undo() override;
        private:
            dodoe::GameObject* game_object_;
            std::string game_object_name_{};
        };

        class CreateChildGameObjectCmd final : public SceneHierarchyCmd {
        public:
            explicit CreateChildGameObjectCmd(SceneHierarchyHandler &context, dodoe::GameObject* game_object)
                : SceneHierarchyCmd(context), game_object_(game_object) {
            }

            void execute() override;
            void undo() override;
        private:
            dodoe::GameObject* game_object_;
            std::string chile_game_object_name_{};
        };

    private:
        HierarchyPanel& hierarchy_panel_;

        void create_game_object_(const std::string& name) const;
        void delete_game_object_(dodoe::GameObject* game_object) const;
    };
} // cakery


#endif //CAKERY_SCENE_HIERARCHY_HANDLER_H
