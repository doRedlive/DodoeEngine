// do@Redlive

#pragma once

#include "dopch.h"
#include "runtime/function/render/render_settings.h"

namespace dodoe {

    struct WindowProperty {
        Int width;
        Int height;
        const Char* title;
        RenderBackendApiType backend_api{ RenderBackendApiType::None };

        explicit WindowProperty(const Int w = 800, const Int h = 600, const Char* t = "dodoe", const bool custom_titlebar = false, const bool resizeable = true)
            : width(w), height(h), title(t) {}
    };

    struct WindowManagerCreateInfo {
        void* host_handle = nullptr;
        WindowProperty prop{};
    };

} // dodoe
