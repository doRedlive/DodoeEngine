// do@Redlive
#pragma once

#include "dopch.h"

struct GLFWwindow;

namespace dodoe {

    enum class RenderBackendApiType;

    enum class GfxNativeMessageSeverity {
        Info,
        Warning,
        Error,
        Fatal,
    };

    struct GfxBackendCreateInfo {
        GLFWwindow* window_handle{nullptr};
        void*       host_handle{nullptr};
        RenderBackendApiType api_type{};
        Bool enable_validation{true};
        UInt32 width{0};
        UInt32 height{0};
    };

    class GfxBackend {
    public:
        virtual ~GfxBackend() = default;

        static Scope<GfxBackend> Create(const GfxBackendCreateInfo& info);
        static void Destroy(Scope<GfxBackend>& obj);

        [[nodiscard]] RenderBackendApiType getApiType() const { return api_type_; }
        [[nodiscard]] virtual Bool isValidationEnabled() const { return enable_validation_; }
        [[nodiscard]] virtual Vector2i getSwapchainExtent2D() const {
            return Vector2i(static_cast<Int32>(width_), static_cast<Int32>(height_));
        }
        [[nodiscard]] GLFWwindow* getNativeWindow() const { return window_handle_; }
        [[nodiscard]] void* getHostHandle() const { return host_handle_; }

        void reportNativeMessage(GfxNativeMessageSeverity severity, const char* message_text) const;

        virtual void shutdown() {}

    protected:
        void initCommonState(const GfxBackendCreateInfo& info);

        GLFWwindow* window_handle_{nullptr};
        void*       host_handle_{nullptr};
        RenderBackendApiType api_type_{};
        Bool enable_validation_{true};
        UInt32 width_{0};
        UInt32 height_{0};
    };

} // dodoe
