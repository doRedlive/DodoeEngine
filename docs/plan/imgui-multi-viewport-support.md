ImGui 多视口独立窗口支持方案

范围与结论

目标是在 Debug ImGui 中支持将 Hierarchy、Inspector、Debugger 等 Dear ImGui 窗口拖出主程序窗口，成为可移动、缩放、关闭并重新停靠的操作系统窗口。

这不是只启用 ImGuiConfigFlags_ViewportsEnable 的改动。当前运行时只具备单 GLFW 窗口、单 swapchain、单 RenderGraph 输出的能力。多视口需要把每一个 ImGui secondary viewport 连接到独立的原生窗口、GPU 呈现表面、渲染目标、命令提交和 present 路径。

本方案不改变编辑器 Qt 窗口体系，也不改变普通游戏窗口的主渲染流程。第一目标是 DODOE_DEBUG_ENABLED 下的 Dear ImGui 独立窗口。

当前状态

ImGui 初始化位于 runtime/function/ui/imgui/imgui_builder.cpp：
- 已启用 ImGuiConfigFlags_DockingEnable。
- 未启用 ImGuiConfigFlags_ViewportsEnable。
- 已使用 imgui_impl_glfw 作为平台后端。
- RenderImGui 仅序列化 ImGui::GetDrawData() 的主 viewport draw data。

ImGui 渲染位于 runtime/function/render/render_pipeline/passes/render_imgui_pass.cpp：
- ImGuiPass 从 ImGuiBuilder::GetRenderPacket() 读取单一 packet。
- Pass 输出到主 RenderGraph 的 ImGuiColor，再由 PresentPass 合成到主 swapchain。
- 该路径没有接收 ImGuiViewport::DrawData，也没有独立 framebuffer 或 present。

图形上下文位于 runtime/function/graphics/gfx_context.h/.cpp：
- GfxContext 只维护一组 swapchain_textures_、swapchain_framebuffers_ 和主窗口 acquire/present 逻辑。
- VulkanBackend、OpenGLBackend、D3D12Backend 都以一个 GLFWwindow 初始化一个呈现目标。

结论：在多视口 backend 完成前，不得只打开 ViewportsEnable。否则 ImGui 会创建或请求 secondary viewport，但项目没有可用的 renderer viewport 回调，窗口可能不显示、黑屏或触发后端断言。

目标与非目标

目标：
- 支持 ImGui 窗口拖出主窗口、跨屏幕移动、缩放、最小化、关闭和重新停靠。
- 每个 secondary viewport 有独立的 GPU render target 与 present。
- 主 viewport 继续使用现有 RenderGraph 主路径。
- Secondary viewport 使用与主 ImGui 相同的字体纹理、shader、输入布局和裁剪规则。
- OpenGL、Vulkan、D3D12 最终拥有同一套 ImGui renderer 回调接口。

非目标：
- 不在第一期支持普通游戏 camera 输出到多个窗口。
- 不允许独立 ImGui 窗口承载场景 RenderGraph、游戏 UI 或其他 renderer feature。
- 不将 Qt 编辑器 panel 改为 Dear ImGui viewport。
- 不在本功能中重写 ImGui docking 布局或持久化格式。

总体架构

新增两层抽象：

1. GfxViewportSurface

每个 OS 窗口对应一个呈现表面。主窗口也应逐步迁移到这个抽象，避免 GfxContext 内部只存在“唯一 swapchain”的隐含假设。

建议职责：

    class GfxViewportSurface {
    public:
        Bool initialize(GfxDeviceHandle device, GLFWwindow* window, RenderBackendApiType api);
        void shutdown();
        Bool resize(UInt32 width, UInt32 height);
        Bool acquire(UInt32& image_index);
        Bool present(UInt32 image_index);
        Vector2i extent() const;
        GfxFramebufferHandle framebuffer(UInt32 image_index) const;
    };

内部资源按后端不同保存：
- Vulkan：VkSurfaceKHR、VkSwapchainKHR、image/image view、framebuffer、acquire/present semaphore、fence。
- D3D12：IDXGISwapChain4、backbuffer、RTV、fence/同步状态。
- OpenGL：共享 GLFW context、当前窗口 default framebuffer 的 wrapper、当前 framebuffer 尺寸。

GfxContext 继续拥有 device、共享 pipeline/cache 与主渲染命令提交；它管理主 surface 和 secondary surface 的创建入口，不再把所有 present 状态硬编码为一份成员数组。

2. ImGuiViewportRenderer

该对象负责把 ImGuiPlatformIO 的 renderer 回调转换为 GfxViewportSurface 操作。它不创建 GLFW 窗口；窗口生命周期由 ImGui_ImplGlfw 平台后端负责。

建议接口：

    class ImGuiViewportRenderer {
    public:
        static void Install(GfxContext& gfx_context, ImGuiRenderResources& resources);
        static void Uninstall();
        static void RenderDrawData(const ImDrawData& draw_data, GfxViewportSurface& surface);
    };

ImGuiViewport::RendererUserData 指向一个 ViewportRenderData：

    struct ViewportRenderData {
        Scope<GfxViewportSurface> surface;
        UInt32 image_index;
        Bool suspended;
    };

ImGuiPlatformIO 回调与职责：

| 回调 | 行为 |
|---|---|
| Renderer_CreateWindow | 从 viewport->PlatformHandle 取得 GLFWwindow，创建 GfxViewportSurface，写入 RendererUserData |
| Renderer_DestroyWindow | 等待该 surface 的在途 GPU 工作完成，释放 framebuffer、swapchain/surface 与 RendererUserData |
| Renderer_SetWindowSize | 尺寸为零时设 suspended；否则延迟或立即重建对应 surface |
| Renderer_RenderWindow | acquire surface，使用 viewport->DrawData 绘制 ImGui，记录 image index |
| Renderer_SwapBuffers | present 已记录 image index；suspended viewport 跳过 |

Dear ImGui 配置与帧生命周期

初始化完成 renderer 回调后，在 ImGuiBuilder::SetupImGui 中启用：

    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

帧尾顺序必须固定，并在 ImGui context 的唯一 owner thread 执行：

    ImGui::Render();
    Serialize main viewport draw data;
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();

其中：
- 主 viewport 继续序列化为 ImGuiRenderPacket 并由主 RenderGraph 的 ImGuiPass 消费。
- RenderPlatformWindowsDefault 触发 secondary viewport 的 renderer 回调。
- 不允许 RenderThread、RenderGraph worker 和主线程同时调用 ImGui::NewFrame、ImGui::Render、UpdatePlatformWindows 或读取同一个 ImGuiContext。
- 若现有渲染线程模型无法保证这一点，先将 ImGui frame 结束和 packet 生成固定到 UI owner thread，再将 immutable packet 交给 GPU owner thread。

ImGui 绘制路径重构

当前 ImGuiPass 中的顶点/索引上传、pipeline 解析、binding set 创建、clip rect 与 indexed draw 逻辑必须提取为可复用的 ImGuiDrawRenderer。输入改为 const ImDrawData 或等价的不可变 ImGuiRenderPacket，输出改为一个明确的 framebuffer 和 extent。

建议最小接口：

    void RenderImGuiDrawData(
        const ImGuiRenderPacket& packet,
        GfxFramebufferHandle framebuffer,
        Vector2i extent,
        ImGuiRenderResources& resources,
        GfxCommandListHandle command_list);

主窗口调用方：
- ImGuiPass 将主 packet 渲染到 RenderGraph 的 ImGuiColor，保持现有 PresentPass 链路。

secondary viewport 调用方：
- Renderer_RenderWindow 将 viewport->DrawData 深拷贝或序列化为 packet。
- 创建临时或每 viewport 复用的 command list 和 vertex/index buffer。
- 直接渲染到该 GfxViewportSurface 的 framebuffer。
- Renderer_SwapBuffers present 该 surface。

不建议 secondary viewport 复用主 RenderGraph 的 ImGuiColor：不同窗口有不同 extent、framebuffer、acquire/present 时序，强行共享会把单窗口假设扩散到 RenderGraph。

后端实施策略

OpenGL

OpenGL 表面看似最简单，但当前项目已有 context owner/thread affinity 风险。多视口前必须先定义每个 GLFW context 的归属线程和切换规则。

实现要点：
- secondary GLFW window 必须与主 GLFW context 共享资源，保证字体纹理和 ImGui texture ID 可用。
- RenderWindow 前调用 glfwMakeContextCurrent(secondary_window)。
- 使用该窗口的默认 framebuffer 创建或获取 cutie OpenGL framebuffer wrapper。
- 绘制后调用 glfwSwapBuffers(secondary_window)。
- 渲染结束后恢复主 context 或释放 current context，且不能与主/渲染线程并发使用同一 context。

OpenGL 多线程 context 约束未解决前，OpenGL 多视口只能在单一 GPU owner thread 上运行。

Vulkan

每个 secondary GLFW window 都需要独立 VkSurfaceKHR 和 VkSwapchainKHR，但可以共享 VkInstance、VkPhysicalDevice、VkDevice、graphics queue 和 ImGui GPU resource。

实现要点：
- 用 glfwCreateWindowSurface 创建 secondary surface。
- 为每个 surface 独立查询 present family、format、present mode 和 extent。
- 每个 surface 维护自己的 image views、semaphores、fences、acquire 和 present。
- command buffer/command list 可共享 device，但提交与 semaphore wait/signal 必须对应到正确 surface。
- 销毁 viewport 前 wait idle 或按 surface fence 确认该 viewport 不再在 flight。

D3D12

每个 secondary GLFW window 的 HWND 创建独立 IDXGISwapChain4，但可共享 ID3D12Device、graphics command queue、shader heap 与 ImGui 资源。

实现要点：
- 从 GLFWwindow 取得 HWND，调用 CreateSwapChainForHwnd。
- 为每个 swapchain 建立 backbuffer wrapper、RTV 和 present fence 状态。
- resize 前等待该 surface fence，释放 backbuffer，再 ResizeBuffers 并重建 RTV。
- 对每个 secondary viewport 独立执行 command list、signal fence 和 Present。

实施阶段

Phase 0：线程与生命周期基线
- 明确 ImGui context owner thread、GLFW event thread、GPU owner thread。
- 添加运行时断言或日志，阻止 ImGui context 被多个线程访问。
- 明确 secondary viewport 销毁、最小化、resize 时不得访问无效 framebuffer。
- 验收：不开启 ViewportsEnable 时现有主窗口流程行为不变。

Phase 1：抽取可复用 ImGuiDrawRenderer
- 将 RenderImGuiPass 的 packet 绘制逻辑抽出，不改变主窗口输出。
- ImGuiBuilder 新增“从 ImDrawData 创建 packet”的内部能力，保留主 packet API。
- 验收：主窗口 ImGui 的字体、裁剪、纹理、缩放、Docking 与当前一致。

Phase 2：GfxViewportSurface 与主 surface 迁移
- 将主 swapchain 的 framebuffer、extent、acquire、present 收敛到 GfxViewportSurface 接口。
- 保持原 GfxContext API 作为转发层，避免一次性改动全部 renderer。
- 验收：三后端主窗口 resize、最小化/恢复、present 与退出行为不回归。

Phase 3：单后端 vertical slice
- 仅为一个后端实现 secondary GfxViewportSurface、ImGuiPlatformIO renderer callbacks 和 ViewportsEnable。
- 首选后端由 Phase 0 的线程约束决定：若 OpenGL context ownership 已稳定，可先做 OpenGL；否则先做 D3D12。
- 验收：可拖出 Hierarchy 和 Inspector；两个独立窗口可同时操作、缩放、最小化、关闭、重新停靠，主窗口继续渲染。

Phase 4：其余后端
- 按同一 GfxViewportSurface 合约实现 Vulkan 和 D3D12/OpenGL。
- 任何后端未实现时，禁止在该后端设置 ViewportsEnable。
- 验收：每个已声明支持的后端均通过同一多窗口行为测试矩阵。

Phase 5：稳定性和体验完善
- 多 DPI 屏幕 font scaling。
- 多窗口焦点、Alt+Tab、关闭顺序、主窗口关闭时 secondary viewport 自动销毁。
- imgui.ini docking/viewport 布局持久化。
- GPU 资源回收、device lost、validation diagnostics。

测试矩阵

功能：
- 主窗口内 docking、拆分与 tab 合并。
- 单个和多个面板拖出主窗口。
- 拖回主窗口并重新 dock。
- secondary viewport 跨显示器移动、DPI 改变、缩放、最小化和恢复。
- 关闭 secondary viewport、关闭主窗口、脚本重载、场景切换。

渲染：
- 默认字体和 ImTextureID 自定义纹理均正确显示。
- clip rect、透明混合、窗口圆角/阴影没有错位或越界。
- 各 viewport 使用自己的 extent；resize 后无旧帧、黑屏或拉伸。
- Vulkan/D3D12 validation 无资源状态、fence、present 或 swapchain 错误。

压力：
- 连续拖出/拖回 100 次。
- 同时打开至少 8 个 secondary viewport。
- resize 循环、最小化/恢复循环、关闭窗口时持续刷新 Inspector。
- GPU capture 验证每个 secondary viewport 只有自身 framebuffer 和 draw data。

失败处理与回滚

- ViewportsEnable 必须由后端能力开关控制，而不是无条件启用。
- 创建 secondary surface 失败时保留该 ImGui 窗口在主 viewport，不得留下半初始化 RendererUserData。
- acquire、resize 或 present 失败时标记该 viewport suspended，并在下一个有效尺寸/重建时恢复。
- 一旦 device lost 或主窗口退出，先禁用/销毁 ImGui viewport renderer，再销毁 GfxContext 和 GLFW。
- 保留单 viewport 路径作为默认 fallback；任何后端的多视口问题不得影响主窗口调试 UI。

文件与职责建议

新增：
- runtime/function/graphics/gfx_viewport_surface.h/.cpp
- runtime/function/ui/imgui/imgui_viewport_renderer.h/.cpp
- runtime/function/ui/imgui/imgui_draw_renderer.h/.cpp

修改：
- runtime/function/ui/imgui/imgui_builder.h/.cpp
- runtime/function/graphics/gfx_context.h/.cpp
- runtime/function/graphics/backend/opengl_backend.h/.cpp
- runtime/function/graphics/backend/vulkan_backend.h/.cpp
- runtime/function/graphics/backend/d3d12_backend.h/.cpp
- runtime/function/render/render_pipeline/passes/render_imgui_pass.h/.cpp
- runtime/function/render/render_pipeline/render_feature/imgui_feature.h/.cpp

完成定义

该功能完成不以“窗口能被拖出”为准，必须同时满足：
- 至少一个已声明后端能稳定运行完整测试矩阵。
- 未实现的后端不会启用多视口。
- 主 viewport 的 RenderGraph 和 Present 路径未回归。
- ImGui context、GLFW window 和 GPU surface 的生命周期有唯一所有者。
- secondary viewport 销毁、resize、最小化、device shutdown 均无 validation error、资源泄漏或崩溃。
