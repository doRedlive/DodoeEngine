//
// Created by GreenMuffin on 2025/12/12.
//

#ifndef CAKERY_CAKERY_HELPER_H
#define CAKERY_CAKERY_HELPER_H

namespace dodoe {
    class Scene;
    class GameObject;
}

namespace cakery {

    class CakeryHelper {
    public:
        CakeryHelper();
        ~CakeryHelper();

        // FIXME: Isn't this approach inelegant ? What if many go are selected ?
        [[nodiscard]] dodoe::GameObject* get_selected_game_object() const;
        void set_selected_game_object(dodoe::GameObject* game_object);

        [[nodiscard]] dodoe::Scene* get_selected_scene() const;
        void set_selected_scene(dodoe::Scene* scene);

        void initialize();
        void shutdown();

    private:
        dodoe::GameObject* selected_game_object_ {nullptr};
        dodoe::Scene* selected_scene_ {nullptr};
    };

    extern CakeryHelper g_cakery_helper;
} // cakery


#endif //CAKERY_CAKERY_HELPER_H