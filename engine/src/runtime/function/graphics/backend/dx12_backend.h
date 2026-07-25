// do@Redlive
#pragma once

#include "dopch.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

struct GLFWwindow;

namespace dodoe {

    using Microsoft::WRL::ComPtr;

    struct Dx12BackendCreateInfo {
        GLFWwindow* window_handle{nullptr};
        void*       host_handle{nullptr};
        bool        enable_validation{true};
    };

    class Dx12Backend : public Managed<Dx12Backend, Dx12BackendCreateInfo> {
        friend class Managed<Dx12Backend, Dx12BackendCreateInfo>;

        // DXGI
        ComPtr<IDXGIFactory7> m_factory{};

        // D3D12 device
        ComPtr<ID3D12Device9> m_device{};

        // Command queues
        ComPtr<ID3D12CommandQueue> m_graphics_queue{};
        ComPtr<ID3D12CommandQueue> m_compute_queue{};
        ComPtr<ID3D12CommandQueue> m_copy_queue{};

        // Swapchain
        ComPtr<IDXGISwapChain4> m_swapchain{};
        std::vector<ID3D12Resource*> m_backbuffers{};
        ComPtr<ID3D12DescriptorHeap> m_rtv_heap{};
        UINT m_rtv_descriptor_size{0};
        DXGI_FORMAT m_backbuffer_format{DXGI_FORMAT_R8G8B8A8_UNORM};
        UINT m_swapchain_width{0};
        UINT m_swapchain_height{0};
        static constexpr UINT kBackbufferCount = 2;

        // Frame synchronization
        ComPtr<ID3D12Fence> m_fence{};
        UINT64 m_fence_value{0};
        HANDLE m_fence_event{nullptr};
        UINT64 m_frame_fence_values[kBackbufferCount]{};

        // Window handle
        void* m_host_handle{nullptr};
        HWND m_hwnd{nullptr};

        // Validation
        ComPtr<ID3D12Debug6> m_debug_controller{};
        bool m_enable_validation{true};

    public:
        [[nodiscard]] ID3D12Device9* getDevice() const { return m_device.Get(); }
        [[nodiscard]] ID3D12CommandQueue* getGraphicsQueue() const { return m_graphics_queue.Get(); }
        [[nodiscard]] ID3D12CommandQueue* getComputeQueue() const { return m_compute_queue.Get(); }
        [[nodiscard]] ID3D12CommandQueue* getCopyQueue() const { return m_copy_queue.Get(); }
        [[nodiscard]] IDXGIFactory7* getFactory() const { return m_factory.Get(); }

        [[nodiscard]] const std::vector<ID3D12Resource*>& getBackbuffers() const { return m_backbuffers; }
        [[nodiscard]] DXGI_FORMAT getBackbufferFormat() const { return m_backbuffer_format; }
        [[nodiscard]] Vector2i getSwapchainExtent2d() const { return Vector2i(static_cast<Int32>(m_swapchain_width), static_cast<Int32>(m_swapchain_height)); }
        [[nodiscard]] UINT getBackbufferCount() const { return kBackbufferCount; }

        [[nodiscard]] bool acquireNextImage(UINT& backbuffer_index);
        [[nodiscard]] bool presentImage(UINT backbuffer_index);
        [[nodiscard]] bool recreateSwapchain(GLFWwindow* window_handle);

    private:
        bool initialize(const Dx12BackendCreateInfo& info);
        void shutdown();

        void createFactory();
        void createDevice();
        void createCommandQueues();
        void createSwapchain(GLFWwindow* window_handle);
        void releaseBackbufferResources();
        void createRTVHeap();
        void createBackbufferRTVs();
        void createFence();

        void waitForGpu();
        void selectAdapter(ComPtr<IDXGIAdapter4>& out_adapter);
    };

} // dodoe
