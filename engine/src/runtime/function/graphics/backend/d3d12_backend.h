// do@Redlive
#pragma once

#include "dopch.h"

#include "gfx_backend.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

struct GLFWwindow;

namespace dodoe {

    using Microsoft::WRL::ComPtr;

    class D3D12Backend : public GfxBackend, public Managed<D3D12Backend, GfxBackendCreateInfo> {
        friend class Managed<D3D12Backend, GfxBackendCreateInfo>;

        // DXGI
        ComPtr<IDXGIFactory7> m_factory{};

        // D3D12 device
        ComPtr<ID3D12Device9> m_device{};

        // Command queues
        ComPtr<ID3D12CommandQueue> m_graphics_queue{};
        ComPtr<ID3D12CommandQueue> m_compute_queue{};
        ComPtr<ID3D12CommandQueue> m_copy_queue{};

        // Validation
        ComPtr<ID3D12Debug6> m_debug_controller{};
        ComPtr<ID3D12InfoQueue1> m_info_queue{};
        DWORD m_message_callback_cookie{0};

    public:
        [[nodiscard]] ID3D12Device9* getDevice() const { return m_device.Get(); }
        [[nodiscard]] ID3D12CommandQueue* getGraphicsQueue() const { return m_graphics_queue.Get(); }
        [[nodiscard]] ID3D12CommandQueue* getComputeQueue() const { return m_compute_queue.Get(); }
        [[nodiscard]] ID3D12CommandQueue* getCopyQueue() const { return m_copy_queue.Get(); }
        [[nodiscard]] IDXGIFactory7* getFactory() const { return m_factory.Get(); }

    private:
        bool initialize(const GfxBackendCreateInfo& info);
        void shutdown() override;

        void createFactory();
        void enableDebugLayer();
        void setupInfoQueue();
        void createDevice();
        void createCommandQueues();

        void selectAdapter(ComPtr<IDXGIAdapter4>& out_adapter);
    };

} // dodoe
