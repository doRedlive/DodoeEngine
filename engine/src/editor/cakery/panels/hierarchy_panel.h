//
// Created by GreenMuffin on 2025/12/7.
//

#ifndef CAKERY_HIERARCHY_PANEL_H
#define CAKERY_HIERARCHY_PANEL_H

#include "dopch.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"

namespace cakery {
    class HierarchyPanel {
        dodoe::Scene* context_ {nullptr};
        dodoe::ui32 editing_handle_{0};
    public:
        void draw();
        void setContext(dodoe::Scene* context);
    private:
        void drawEntityNode(dodoe::Entity entity);
    };
} // cakery


#endif//CAKERY_HIERARCHY_PANEL_H
