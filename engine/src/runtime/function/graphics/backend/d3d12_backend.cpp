// do@Redlive

#include "d3d12_backend.h"

#include "runtime/function/render/render_settings.h"

#include "GLFW/glfw3.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3native.h"

namespace dodoe {

    namespace {
        void __stdcall OnD3D12Message(
            D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity,
            D3D12_MESSAGE_ID, LPCSTR p_description, void*) {
            const char* text = p_description ? p_description : "";
            switch (severity) {
            case D3D12_MESSAGE_SEVERITY_CORRUPTION:
            case D3D12_MESSAGE_SEVERITY_ERROR:
                DO_ERROR("[D3D12] {}", text);
                break;
            case D3D12_MESSAGE_SEVERITY_WARNING:
                DO_WARN("[D3D12] {}", text);
                break;
            default:
                DO_INFO("[D3D12] {}", text);
                break;
            }
        }
    }

    void D3D12Backend::enableDebugLayer() {
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debug_controller)))) {
            m_debug_controller->EnableDebugLayer();
            OutputDebugStringA("[D3D12] Debug layer enabled\n");
        }
    }

    void D3D12Backend::setupInfoQueue() {
        if (FAILED(m_device.As(&m_info_queue))) {
            OutputDebugStringA("[D3D12] WARNING: device does not expose ID3D12InfoQueue1\n");
            return;
        }

        m_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        m_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

        D3D12_MESSAGE_ID hide_ids[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
            D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
        };
        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = _countof(hide_ids);
        filter.DenyList.pIDList = hide_ids;
        m_info_queue->AddStorageFilterEntries(&filter);

        m_info_queue->RegisterMessageCallback(&OnD3D12Message, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &m_message_callback_cookie);

        OutputDebugStringA("[D3D12] Info queue configured (break on errors, message callback registered)\n");
    }

    bool D3D12Backend::initialize(const D3D12BackendCreateInfo& info) {
        OutputDebugStringA("[D3D12] initialize begin\n");

        m_enable_validation = info.enable_validation;
        m_host_handle = info.host_handle;

        if (info.host_handle != nullptr) {
            m_hwnd = static_cast<HWND>(info.host_handle);
        }

        OutputDebugStringA("[D3D12] creating factory...\n");
        createFactory();
        OutputDebugStringA("[D3D12] factory ok\n");

        if (m_enable_validation) {
            enableDebugLayer();
        }

        OutputDebugStringA("[D3D12] creating device...\n");
        createDevice();
        OutputDebugStringA("[D3D12] device ok\n");

        if (m_enable_validation) {
            setupInfoQueue();
        }

        OutputDebugStringA("[D3D12] creating command queues...\n");
        createCommandQueues();
        OutputDebugStringA("[D3D12] queues ok\n");

        OutputDebugStringA("[D3D12] creating swapchain...\n");
        createSwapchain(info.window_handle, info.width, info.height);
        OutputDebugStringA("[D3D12] swapchain ok\n");

        createRTVHeap();
        createBackbufferRTVs();
        OutputDebugStringA("[D3D12] RTV heap ok\n");

        createFence();

        OutputDebugStringA("[D3D12] fence ok, initialize done\n");

        return true;
    }

    void D3D12Backend::shutdown() {
        waitForGpu();

        releaseBackbufferResources();
        m_rtv_heap.Reset();

        if (m_fence_event != nullptr) {
            CloseHandle(m_fence_event);
            m_fence_event = nullptr;
        }
        m_fence.Reset();

        m_swapchain.Reset();

        m_copy_queue.Reset();
        m_compute_queue.Reset();
        m_graphics_queue.Reset();

        if (m_info_queue && m_message_callback_cookie != 0) {
            m_info_queue->UnregisterMessageCallback(m_message_callback_cookie);
            m_message_callback_cookie = 0;
        }
        m_info_queue.Reset();

        m_device.Reset();

        if (m_debug_controller) {
            m_debug_controller.Reset();
        }

        m_factory.Reset();
        m_hwnd = nullptr;
        m_host_handle = nullptr;
    }

    void D3D12Backend::createFactory() {
        UINT dxgi_factory_flags = 0;
        if (m_enable_validation) {
            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        }

        HRESULT hr = CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&m_factory));
        DO_ASSERT(SUCCEEDED(hr), "D3D12Backend::createFactory failed with HRESULT={:08X}", static_cast<UINT>(hr));
    }

    void D3D12Backend::selectAdapter(ComPtr<IDXGIAdapter4>& out_adapter) {
        ComPtr<IDXGIAdapter1> adapter;
        ComPtr<IDXGIAdapter1> fallback_adapter;
        SIZE_T max_vram = 0;

        for (UINT i = 0; m_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                adapter.Reset();
                continue;
            }

            ComPtr<ID3D12Device> test_device;
            if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&test_device)))) {
                if (!fallback_adapter) {
                    fallback_adapter = adapter;
                }
                adapter.Reset();
                continue;
            }

            if (desc.DedicatedVideoMemory > max_vram) {
                max_vram = desc.DedicatedVideoMemory;
                out_adapter.Reset();
                adapter.As(&out_adapter);
            }

            if (!fallback_adapter) {
                fallback_adapter = adapter;
            }
            adapter.Reset();
        }

        if (!out_adapter) {
            if (fallback_adapter) {
                fallback_adapter.As(&out_adapter);
            }
        }

        DO_ASSERT(out_adapter != nullptr, "D3D12Backend::selectAdapter: no suitable D3D12 adapter found.");
        if (out_adapter) {
            DXGI_ADAPTER_DESC1 desc;
            out_adapter->GetDesc1(&desc);
            { char buf[256]; snprintf(buf, sizeof(buf), "[D3D12] selected adapter: %ls, VRAM=%zu MB\n", desc.Description, max_vram / (1024 * 1024)); OutputDebugStringA(buf); }
        }
    }

    void D3D12Backend::createDevice() {
        ComPtr<IDXGIAdapter4> adapter;
        selectAdapter(adapter);

        HRESULT hr = D3D12CreateDevice(
            adapter.Get(),
            D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(&m_device));
        DO_ASSERT(SUCCEEDED(hr), "D3D12Backend::createDevice failed with HRESULT={:08X}", static_cast<UINT>(hr));

        if (m_enable_validation) {
            m_device->SetName(L"Dodoe D3D12 Device");
        }
    }

    void D3D12Backend::createCommandQueues() {
        D3D12_COMMAND_QUEUE_DESC queue_desc{};
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queue_desc.NodeMask = 0;

        HRESULT hr = m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_graphics_queue));
        DO_ASSERT(SUCCEEDED(hr), "D3D12Backend::createCommandQueues: graphics queue failed with HRESULT={:08X}", static_cast<UINT>(hr));
        if (m_enable_validation) m_graphics_queue->SetName(L"Graphics Queue");

        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        hr = m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_compute_queue));
        DO_ASSERT(SUCCEEDED(hr), "D3D12Backend::createCommandQueues: compute queue failed with HRESULT={:08X}", static_cast<UINT>(hr));
        if (m_enable_validation) m_compute_queue->SetName(L"Compute Queue");

        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
        hr = m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_copy_queue));
        DO_ASSERT(SUCCEEDED(hr), "D3D12Backend::createCommandQueues: copy queue failed with HRESULT={:08X}", static_cast<UINT>(hr));
        if (m_enable_validation) m_copy_queue->SetName(L"Copy Queue");
    }

    UINT D3D12Backend::GetSwapchainFlags() {
        return (RenderSettings::GetPresentMode() == PresentMode::Immediate) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    }

    void D3D12Backend::createSwapchain(GLFWwindow* window_handle, UInt32 width, UInt32 height) {
        HWND hwnd = m_hwnd;
        if (hwnd == nullptr && window_handle != nullptr) {
            hwnd = glfwGetWin32Window(window_handle);
        }
        DO_ASSERT(hwnd != nullptr, "D3D12Backend::createSwapchain: no window handle.");

        UInt32 w = width;
        UInt32 h = height;
        if (w == 0 || h == 0) {
            RECT rect;
            GetClientRect(hwnd, &rect);
            w = rect.right - rect.left;
            h = rect.bottom - rect.top;
        }
        m_swapchain_width = (w == 0) ? 1 : w;
        m_swapchain_height = (h == 0) ? 1 : h;

        DXGI_SWAP_CHAIN_DESC1 swapchain_desc{};
        swapchain_desc.Width = m_swapchain_width;
        swapchain_desc.Height = m_swapchain_height;
        swapchain_desc.Format = m_backbuffer_format;
        swapchain_desc.Stereo = FALSE;
        swapchain_desc.SampleDesc.Count = 1;
        swapchain_desc.SampleDesc.Quality = 0;
        swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapchain_desc.BufferCount = kBackbufferCount;
        swapchain_desc.Scaling = DXGI_SCALING_STRETCH;
        swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapchain_desc.Flags = GetSwapchainFlags();

        ComPtr<IDXGISwapChain1> swapchain1;
        HRESULT hr = m_factory->CreateSwapChainForHwnd(
            m_graphics_queue.Get(),
            hwnd,
            &swapchain_desc,
            nullptr,
            nullptr,
            &swapchain1);
        DO_ASSERT(SUCCEEDED(hr), "D3D12Backend::createSwapchain: CreateSwapChainForHwnd failed with HRESULT={:08X}", static_cast<UINT>(hr));

        hr = swapchain1.As(&m_swapchain);
        DO_ASSERT(SUCCEEDED(hr), "D3D12Backend::createSwapchain: QueryInterface IDXGISwapChain4 failed with HRESULT={:08X}", static_cast<UINT>(hr));

        m_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

        { char buf[128]; snprintf(buf, sizeof(buf), "[D3D12] swapchain created: %ux%u, format=%u\n", m_swapchain_width, m_swapchain_height, static_cast<UINT>(m_backbuffer_format)); OutputDebugStringA(buf); }
    }

    void D3D12Backend::releaseBackbufferResources() {
        for (auto& bb : m_backbuffers) {
            if (bb) {
                bb->Release();
            }
        }
        m_backbuffers.clear();
    }

    void D3D12Backend::createRTVHeap() {
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
        heap_desc.NumDescriptors = kBackbufferCount;
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        heap_desc.NodeMask = 0;

        HRESULT hr = m_device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_rtv_heap));
        DO_ASSERT(SUCCEEDED(hr), "D3D12Backend::createRTVHeap failed with HRESULT={:08X}", static_cast<UINT>(hr));

        m_rtv_descriptor_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    void D3D12Backend::createBackbufferRTVs() {
        releaseBackbufferResources();

        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = m_rtv_heap->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < kBackbufferCount; ++i) {
            ID3D12Resource* backbuffer = nullptr;
            HRESULT hr = m_swapchain->GetBuffer(i, IID_PPV_ARGS(&backbuffer));
            DO_ASSERT(SUCCEEDED(hr), "D3D12Backend::createBackbufferRTVs: GetBuffer({}) failed with HRESULT={:08X}", i, static_cast<UINT>(hr));

            m_device->CreateRenderTargetView(backbuffer, nullptr, rtv_handle);
            m_backbuffers.push_back(backbuffer);

            if (m_enable_validation) {
                wchar_t name[64];
                swprintf(name, 64, L"Backbuffer %u", i);
                backbuffer->SetName(name);
            }

            rtv_handle.ptr += m_rtv_descriptor_size;
        }
    }

    void D3D12Backend::createFence() {
        HRESULT hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
        DO_ASSERT(SUCCEEDED(hr), "D3D12Backend::createFence failed with HRESULT={:08X}", static_cast<UINT>(hr));

        m_fence_value = 0;
        m_fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        DO_ASSERT(m_fence_event != nullptr, "D3D12Backend::createFence: CreateEventW failed.");

        for (UINT i = 0; i < kBackbufferCount; ++i) {
            m_frame_fence_values[i] = 0;
        }
    }

    void D3D12Backend::waitForGpu() {
        if (m_graphics_queue && m_fence) {
            ++m_fence_value;
            HRESULT hr = m_graphics_queue->Signal(m_fence.Get(), m_fence_value);
            if (SUCCEEDED(hr)) {
                m_fence->SetEventOnCompletion(m_fence_value, m_fence_event);
                WaitForSingleObject(m_fence_event, INFINITE);
            }
        }
    }

    bool D3D12Backend::acquireNextImage(UINT& backbuffer_index) {
        if (!m_swapchain) {
            return false;
        }

        backbuffer_index = m_swapchain->GetCurrentBackBufferIndex();

        if (m_frame_fence_values[backbuffer_index] > 0 && m_fence) {
            UINT64 completed = m_fence->GetCompletedValue();
            if (completed < m_frame_fence_values[backbuffer_index]) {
                m_fence->SetEventOnCompletion(m_frame_fence_values[backbuffer_index], m_fence_event);
                WaitForSingleObject(m_fence_event, INFINITE);
            }
        }

        return true;
    }

    bool D3D12Backend::presentImage(UINT backbuffer_index) {
        if (!m_swapchain || !m_graphics_queue) {
            return false;
        }

        HRESULT hr = DXGI_ERROR_INVALID_CALL;
        switch (RenderSettings::GetPresentMode()) {
        case PresentMode::VSync:
            hr = m_swapchain->Present(1, 0);
            break;
        case PresentMode::Immediate:
            hr = m_swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
            break;
        case PresentMode::Mailbox:
        default:
            hr = m_swapchain->Present(0, 0);
            break;
        }
        if (hr == DXGI_ERROR_DEVICE_REMOVED) {
            HRESULT device_removed = m_device->GetDeviceRemovedReason();
            DO_ERROR("D3D12Backend::presentImage: Device removed! HRESULT={:08X}", static_cast<UINT>(device_removed));
            return false;
        }

        ++m_fence_value;
        m_frame_fence_values[backbuffer_index] = m_fence_value;
        HRESULT signal_hr = m_graphics_queue->Signal(m_fence.Get(), m_fence_value);
        if (FAILED(signal_hr)) {
            DO_ERROR("D3D12Backend::presentImage: Signal fence failed with HRESULT={:08X}", static_cast<UINT>(signal_hr));
            return false;
        }

        return SUCCEEDED(hr);
    }

    bool D3D12Backend::recreateSwapchain(GLFWwindow* window_handle, UInt32 width, UInt32 height) {
        (void)window_handle;
        if (width == 0 || height == 0) {
            return false;
        }

        waitForGpu();

        releaseBackbufferResources();

        m_swapchain_width = width;
        m_swapchain_height = height;

        HRESULT hr = m_swapchain->ResizeBuffers(
            kBackbufferCount,
            m_swapchain_width,
            m_swapchain_height,
            m_backbuffer_format,
            GetSwapchainFlags());
        if (FAILED(hr)) {
            DO_ERROR("D3D12Backend::recreateSwapchain: ResizeBuffers failed with HRESULT=0x{:08X}", static_cast<UINT>(hr));
        }
        DO_ASSERT(SUCCEEDED(hr), "D3D12Backend::recreateSwapchain: ResizeBuffers failed");

        createBackbufferRTVs();
        return true;
    }

} // dodoe
