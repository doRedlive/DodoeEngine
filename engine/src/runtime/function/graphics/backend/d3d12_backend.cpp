// do@Redlive

#include "d3d12_backend.h"

namespace dodoe {

    namespace {
        void __stdcall OnD3D12Message(
            D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity,
            D3D12_MESSAGE_ID, LPCSTR p_description, void* p_user_data) {
            auto* backend = static_cast<D3D12Backend*>(p_user_data);
            if (!backend) return;

            GfxNativeMessageSeverity mapped = GfxNativeMessageSeverity::Info;
            switch (severity) {
            case D3D12_MESSAGE_SEVERITY_CORRUPTION:
                mapped = GfxNativeMessageSeverity::Fatal;
                break;
            case D3D12_MESSAGE_SEVERITY_ERROR:
                mapped = GfxNativeMessageSeverity::Error;
                break;
            case D3D12_MESSAGE_SEVERITY_WARNING:
                mapped = GfxNativeMessageSeverity::Warning;
                break;
            default:
                break;
            }

            backend->reportNativeMessage(mapped, p_description ? p_description : "");
        }
    }

    void D3D12Backend::enableDebugLayer() {
        DO_PROFILE_SCOPE_CATEGORY("D3D12Backend::enableDebugLayer", "startup");
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debug_controller)))) {
            m_debug_controller->EnableDebugLayer();
            DO_INFO("D3D12Backend: debug layer enabled");
        } else {
            DO_WARN("D3D12Backend: debug layer requested but unavailable");
        }
    }

    void D3D12Backend::setupInfoQueue() {
        DO_PROFILE_SCOPE_CATEGORY("D3D12Backend::setupInfoQueue", "startup");
        if (FAILED(m_device.As(&m_info_queue))) {
            DO_WARN("D3D12Backend: device does not expose ID3D12InfoQueue1");
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

        m_info_queue->RegisterMessageCallback(&OnD3D12Message, D3D12_MESSAGE_CALLBACK_FLAG_NONE, this, &m_message_callback_cookie);
    }

    bool D3D12Backend::initialize(const GfxBackendCreateInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("D3D12Backend::initialize", "startup");

        initCommonState(info);

        createFactory();

        if (enable_validation_) {
            enableDebugLayer();
        }

        createDevice();

        if (enable_validation_) {
            setupInfoQueue();
        }

        createCommandQueues();

        DO_INFO("D3D12Backend: initialized");
        return true;
    }

    void D3D12Backend::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("D3D12Backend::shutdown", "shutdown");
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
    }

    void D3D12Backend::createFactory() {
        DO_PROFILE_SCOPE_CATEGORY("D3D12Backend::createFactory", "startup");
        UINT dxgi_factory_flags = 0;
        if (enable_validation_) {
            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        }

        HRESULT hr = CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&m_factory));
        DO_ASSERT(SUCCEEDED(hr), "D3D12Backend::createFactory failed with HRESULT={:08X}", static_cast<UINT>(hr));
    }

    void D3D12Backend::selectAdapter(ComPtr<IDXGIAdapter4>& out_adapter) {
        DO_PROFILE_SCOPE_CATEGORY("D3D12Backend::selectAdapter", "startup");
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
    }

    void D3D12Backend::createDevice() {
        DO_PROFILE_SCOPE_CATEGORY("D3D12Backend::createDevice", "startup");
        ComPtr<IDXGIAdapter4> adapter;
        selectAdapter(adapter);

        HRESULT hr = D3D12CreateDevice(
            adapter.Get(),
            D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(&m_device));
        DO_ASSERT(SUCCEEDED(hr), "D3D12Backend::createDevice failed with HRESULT={:08X}", static_cast<UINT>(hr));

        if (enable_validation_) {
            m_device->SetName(L"Dodoe D3D12 Device");
        }
    }

    void D3D12Backend::createCommandQueues() {
        DO_PROFILE_SCOPE_CATEGORY("D3D12Backend::createCommandQueues", "startup");
        D3D12_COMMAND_QUEUE_DESC queue_desc{};
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queue_desc.NodeMask = 0;

        HRESULT hr = m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_graphics_queue));
        DO_ASSERT(SUCCEEDED(hr), "D3D12Backend::createCommandQueues: graphics queue failed with HRESULT={:08X}", static_cast<UINT>(hr));
        if (enable_validation_) m_graphics_queue->SetName(L"Graphics Queue");

        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        hr = m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_compute_queue));
        DO_ASSERT(SUCCEEDED(hr), "D3D12Backend::createCommandQueues: compute queue failed with HRESULT={:08X}", static_cast<UINT>(hr));
        if (enable_validation_) m_compute_queue->SetName(L"Compute Queue");

        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
        hr = m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_copy_queue));
        DO_ASSERT(SUCCEEDED(hr), "D3D12Backend::createCommandQueues: copy queue failed with HRESULT={:08X}", static_cast<UINT>(hr));
        if (enable_validation_) m_copy_queue->SetName(L"Copy Queue");
    }

} // dodoe
