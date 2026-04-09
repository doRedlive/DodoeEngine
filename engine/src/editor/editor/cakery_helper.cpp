//
// Created by GreenMuffin on 2025/12/12.
//

#include "cakery_helper.h"

#include "runtime/core/world/scene.h"
#include "runtime/core/world/game_object.h"

namespace cakery {
    CakeryHelper g_cakery_helper;

    CakeryHelper::CakeryHelper() = default;
    CakeryHelper::~CakeryHelper() = default;

    dodoe::GameObject* CakeryHelper::get_selected_game_object() const {
        return selected_game_object_;
    }

    void CakeryHelper::set_selected_game_object(dodoe::GameObject* game_object) {
        selected_game_object_ = game_object;
    }

    dodoe::Scene* CakeryHelper::get_selected_scene() const {
        return selected_scene_;
    }

    void CakeryHelper::set_selected_scene(dodoe::Scene* scene) {
        selected_scene_ = scene;
    }

    void CakeryHelper::initialize() {

    }

    void CakeryHelper::shutdown() {

    }

}