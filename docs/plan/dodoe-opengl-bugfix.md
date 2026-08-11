Only2D OpenGL 运行时稳定性整改计划
范围与结论
本文记录对当前 Sandbox 配置（OpenGL + Only2D + DualThread）的静态排查结果，并给出修复顺序与验收标准。
本次仅做代码阅读，没有编译或运行。问题按风险分级：
- P0：可直接导致当前启动/首帧崩溃或 OpenGL 未定义行为。
- P1：在常见操作下会导致崩溃、数据竞争、资源失效或无法显示。
- P2：功能或兼容性缺陷，应在稳定性修复后处理。
目标不是只让 Sandbox "不崩"，而是建立明确的 OpenGL 线程、资源、present 与关闭约束，使 Only2D 可作为可维护的后端组合。
当前链路
主线程
  WindowManager / GLFW 创建 OpenGL 窗口
  GfxContext::initializeOpenGL
    -> glfwMakeContextCurrent(window)
    -> gladLoadGL / cutie::opengl::createDevice
  SystemContext 启动 RenderThread（DualThread）

每帧主线程
  ImGui NewFrame / World 与 UI 更新
  RenderThread::submitAndWait

RenderThread
  RenderSystem::renderFrame
    -> 消费 RenderCommand
    -> RenderScene::flushUpdates
    -> Only2DRenderer
       Sprite -> UI -> PostProcess2D -> ImGui -> Present
    -> 执行 cutie command list
    -> glfwSwapBuffers
P0：OpenGL context 与实际渲染线程不一致
现状
- Sandbox 显式选择 DualThread。
- OpenGL context 仅在初始化阶段由主线程 glfwMakeContextCurrent。
- DualThread 模式会在 RenderThread 调用 RenderSystem::renderFrame。
- 该调用中会创建资源、执行 command list，并最终 glfwSwapBuffers。
- 代码没有在 RenderThread 调用 glfwMakeContextCurrent，也没有从主线程释放该 context。
风险
OpenGL context 同一时刻只能属于一个线程。RenderThread 上的 RHI/GL 调用没有 current context，属于未定义行为；首帧创建 transient texture、binding set、pipeline、执行 command list 或 present 时均可能访问违规。
证据
- engine/src/sandbox/sandbox/sandbox_app.cpp:30-32
- engine/src/runtime/function/graphics/backend/opengl_backend.cpp:15-18
- engine/src/runtime/core/context/system_context.cpp:119-124
- engine/src/runtime/function/render/render_system.cpp:105-163
- engine/src/runtime/function/graphics/gfx_context.cpp:341-346
解决方案
分两阶段实施：
1. 稳定性基线：OpenGL 强制 SingleThread。在 RenderSettings 初始化或上下文初始化时拒绝/降级 DualThread、TripleThread，保持所有 GL 与 GLFW context 操作都在主线程。
2. 多线程支持：引入 OpenGLContextOwner（或等价封装），将 context 的获取、释放、线程亲和性校验集中管理。
  - 启动 RenderThread 前，主线程执行 glfwMakeContextCurrent(nullptr)。
  - RenderThread 入口将窗口 context 设为 current，并在退出前释放。
  - 所有 GL 资源创建、RHI 提交、waitForIdle、垃圾回收、resize 重建、present、device 销毁均只能在该 owner thread 执行。
  - 主线程不得再通过 GDrawCommandList 或任意 device API 创建/上传 GL 资源；应提交资源任务给 render thread。
  - Debug 构建加入 glfwGetCurrentContext() == window 与 owner-thread-id 断言。
验收
- OpenGL + SingleThread 可连续运行、resize、最小化/恢复、退出。
- 如启用 DualThread，GL 调用前均能通过线程亲和性断言；连续渲染、贴图首次加载、窗口 resize/退出无 context 错误。
P0：OpenGL present 目标不是窗口默认 framebuffer
现状
OpenGL 分支将 "GL Backbuffer" 创建为普通 device_->createTexture 的离屏纹理。PresentPass 以它为 color attachment；随后只调用 GLFW 的 glfwSwapBuffers。这不会把该离屏纹理复制到窗口默认 framebuffer。
风险
渲染结果无法出现在窗口，通常为黑屏或旧帧。若 cutie 的 framebuffer 或状态模型把它当作 swapchain image，某些 GL driver/RHI 实现可能在 FBO 或 present 阶段失败。
证据
- engine/src/runtime/function/graphics/gfx_context.cpp:226-245
- engine/src/runtime/function/render/render_pipeline/passes/render_present_pass.cpp:46-52
- engine/src/runtime/function/graphics/gfx_context.cpp:341-346
解决方案
明确 OpenGL 的两个资源角色，不再混用：
1. Scene/PresentIntermediate：普通离屏 texture，供 Sprite、PostProcess、ImGui 或最终 compose 使用。
2. WindowBackbuffer：代表默认 framebuffer（FBO 0），不能用普通 texture 伪造。
实现路径二选一：
- 若 cutie 支持 native/default framebuffer：PresentPass 以该 native target 为最终 RT。
- 若 cutie 只能渲染 texture：Present 后执行 OpenGL 专用 blit/fullscreen composite，将最终 texture 写入 FBO 0，再调用 glfwSwapBuffers。
同时将 OpenGL present 从通用 swapchain_textures_ 语义中拆出，避免 RDG 把离屏 texture 当作真正的 swapchain image。
验收
- 无 ImGui、仅 Sprite、仅 UI、全链路四种场景均能显示正确画面。
- resize 后默认 framebuffer 和中间 RT 尺寸匹配，画面无黑屏、拉伸或旧帧。
P0：Present shader 的资源布局与 C++ 绑定不匹配
现状
combine_pass.frag 将 viewport 数据声明为 push constant；C++ PresentPass 却创建并绑定 constant buffer，并且未调用 setPushConstants。HLSL 与 GLSL 的布局也因此不一致。
风险
shader reflection / binding layout 与实际 shader 不一致。结果可能是 pipeline 或 binding set 创建失败、GL validation error、驱动异常，或 viewport 参数未初始化导致最终画面错误。
证据
- engine/res/shaders/combine_pass.frag:6-9
- engine/res/shaders/combine_pass_ps.hlsl:1-5
- engine/src/runtime/function/render/render_pipeline/passes/render_present_pass.cpp:24-25
- engine/src/runtime/function/render/render_pipeline/passes/render_present_pass.cpp:68-85
解决方案
选择一种跨后端一致的 ABI，并让 HLSL、GLSL、manifest/反射、C++ 参数结构同步：
- 推荐：使用 push constants。将 C++ PresentPassShaderParams 声明为 PushConstants，并在 draw 前提交 PresentViewportCB；删除该 pass 的 persistent constant buffer/import。
- 或：使用 uniform/constant buffer。则 GLSL 改为匹配 binding/set 的 UBO，并保证 HLSL register、反射和 binding layout 相同。
不允许 shader 源码与 C++ shader parameter struct 由不同资源模型表达同一数据。
验收
- 三后端的 shader reflection 中 Present layout 相同或有明确且受测试保护的 backend-specific mapping。
- 开启 RHI/GL validation 时，Present pipeline 与 binding set 创建无报错。
P1：RenderGraph 并行执行会竞争 PSO cache 与 GPU 对象创建
现状
非 immediate 模式下，RenderGraph 对同一 dependency level 的每个 pass 创建一个任务，交给 ThreadPool。Only2D 的 Sprite、UI、ImGui 等在首帧可处于不同 level 或相同无依赖 level。pass 内会调用：
- PipelineStateCache::resolveGraphicsPipeline；
- DrawCommandList::createFramebuffer；
- DrawCommandList::createBindingSet；
- transient resource acquire/create。
PipelineStateCache 的 unordered_map 无锁；其他缓存及 RHI 对象创建也没有明确的并发合约。
风险
两个 worker 并发插入 unordered_map 是 C++ 数据竞争，可能造成内存损坏和随机崩溃。即使加锁，OpenGL resource/pipeline 创建也不应从没有 current GL context 的 worker 执行。
证据
- engine/src/runtime/function/render/render_graph/render_graph.cpp:477-510
- engine/src/runtime/function/render/pipeline/pipeline_state_cache.cpp:6-23
- engine/src/runtime/function/graphics/draw_command_list.cpp:257-289
解决方案
将 RenderGraph 执行拆为两个明确阶段：
1. 可并行的 CPU build/record：只能生成不触及 device、cache mutation、ImGui 的纯数据或可合并 command payload。
2. 串行的 render-thread materialization/submit：资源获取、FBO/PSO/binding set 创建、RHI command list 执行都在 GL owner thread 完成。
过渡方案：OpenGL 下强制 RenderGraph direct/serial mode；同时给通用 cache 加同步并定义其调用线程。不要仅为避免崩溃给 cache 粗暴加锁后继续允许 worker 调 GL。
验收
- 首帧反复创建 Sprite/UI/ImGui PSO 不出现重复创建、数据竞争或随机崩溃。
- ThreadSanitizer（可用平台）或专用并发压力测试无 cache race。
P1：ImGui 帧生命周期跨线程且与 RenderGraph worker 并发
现状
主线程调用 ImGui GLFW backend 的 NewFrame 和 UI 构建；ImGuiPass 在 RenderGraph worker 中调用 ImGui::Render() 与 ImGui::GetDrawData()。
风险
Dear ImGui 默认不支持该并发模式。主线程下一帧 Begin/NewFrame 或窗口输入回调可与 worker 读取同一 context 并发，导致 draw data 损坏、断言或崩溃。
证据
- engine/src/runtime/core/context/system_context.cpp:201-214
- engine/src/runtime/function/ui/imgui/imgui_builder.cpp:48-57
- engine/src/runtime/function/render/render_pipeline/passes/render_imgui_pass.cpp:94-100
- engine/src/runtime/function/render/render_graph/render_graph.cpp:489-507
解决方案
在主线程的 ImGui frame 结束处调用一次 ImGui::Render()，并把不可变的 ImDrawData 深拷贝/序列化为 render packet；RenderThread/RenderGraph 只消费该 packet，绝不触碰 ImGui context。ImGui pass 强制串行录制。
如暂不拆 packet，OpenGL/Only2D 基线必须让 UI 构建、ImGui::Render、draw-data 读取、GPU 提交处在同一线程和严格帧边界内。
验收
- 连续拖动窗口、输入文本、打开/关闭调试窗口、切换 play mode 不崩溃。
- RenderThread 不再调用 ImGui context 的 Begin/Render API。
P1：贴图首次加载会从主线程直接创建/上传 GL 资源
现状
SpriteRendererSystem 在 world update（主线程）中调用 Texture2D::Load。TextureManager 随后通过全局 GDrawCommandList 立即创建 texture、写 texture，并可能更新 bindless descriptor table。
风险
这与 GL context 归属策略冲突：若 context 迁到 RenderThread，主线程首次加载贴图即非法调用 GL；即便 context 留主线程，render thread 同时使用 device/纹理也会产生线程安全问题。Texture cache 也没有在访问路径上使用其声明的 mutex。
证据
- engine/src/runtime/function/world/systems/sprite_renderer_system.cpp:58-77
- engine/src/runtime/function/render/texture/texture_manager.cpp:54-60
- engine/src/runtime/function/render/texture/texture_manager.cpp:104-168
解决方案
采用两阶段异步加载：
1. 任意 worker/main thread 仅做文件读取和像素解码，生成 CPU TextureUploadRequest。
2. render owner thread 在帧开始或专用 resource queue 中创建 GPU texture、上传、更新 descriptor table，并原子发布可用 handle/slot。
渲染对象在资源未就绪时使用 fallback texture；TextureManager 的 cache、slot LUT、descriptor 分配必须有单 owner 或完整同步，不允许无锁跨线程修改。
验收
- 运行期间首次出现多个不同 Sprite 贴图无崩溃。
- 贴图加载线程与 GPU owner thread 的职责可由日志/断言确认。
P1：窗口最小化会产生零尺寸资源与不完整 FBO
现状
OpenGL acquire/recreate 读取 framebuffer size 后直接按该 extent 创建 texture，没有针对 width/height 为 0 的分支。窗口最小化时 GLFW framebuffer size 可为 0。
风险
创建 0×0 texture、FBO、viewport 或 post-process pass 可能失败，具体表现依赖 driver/RHI。
证据
- engine/src/runtime/function/graphics/backend/opengl_backend.cpp:31-34
- engine/src/runtime/function/graphics/gfx_context.cpp:226-245
- engine/src/runtime/function/graphics/gfx_context.cpp:382-388
- engine/src/runtime/function/render/render_system.cpp:76-91
解决方案
定义 Suspended swapchain/window 状态：extent 任一维为 0 时不创建 RT、不 acquire、不执行 RenderGraph、不 present；事件循环继续运行。恢复到非零尺寸时，在 GL owner thread wait idle 后统一重建资源和 viewport。
验收
- 最小化保持数分钟、恢复、连续 resize 都不产生 GL error、断言或黑屏。
P1：Feature 生命周期没有按接口执行 shutdown
现状
RenderPipeline::shutdown() 直接 clearFeatures()，只析构容器，不会调用 Feature 的 shutdown()。例如 ImGuiFeature 的 shutdown() 负责清除 ImGui TexID；Sprite/UI feature 也有显式释放路径。
风险
若 renderer 重建、退出顺序变化或 ImGui context 存活更久，可能留下悬空 TexID、延迟释放 GPU 资源，或出现关机时序问题。
证据
- engine/src/runtime/function/render/render_pipeline/render_pipeline.cpp:37-42
- engine/src/runtime/function/render/render_pipeline/renderer.h:67-71
- engine/src/runtime/function/render/render_pipeline/render_feature/imgui_feature.cpp:73-82
解决方案
在 BaseRenderer::clearFeatures() 中以逆注册顺序调用每个 feature 的 shutdown()，然后再清空 pass/feature 容器。RenderPipeline 应调用 active renderer 的实际 shutdown()，而不是绕过它。明确关闭顺序：停止 render thread -> GPU idle -> pipeline/features -> shared render services -> device/context -> window/GLFW。
验收
- 正常退出、初始化失败后的回滚、renderer 重建均无悬空 ImGui texture id、无 context 已销毁后的 GPU 析构调用。
P2：OpenGL 4.5 与 SPIR-V shader 加载能力需显式协商
现状
窗口请求 OpenGL 4.5 core；ShaderLibrary 对 OpenGL 也一律读取 .spv。SPIR-V 直接加载通常依赖 ARB_gl_spirv/对应驱动支持，而 OpenGL 4.5 core 并不保证它。当前工作区的 cutie-rhi 子模块内容不可读，无法确认其具体的 SPIR-V/GLSL 处理路径。
风险
在无 ARB_gl_spirv 的驱动上，shader 创建可能失败；如果 RHI 对失败处理不足，可能继续使用空 shader/pipeline。
证据
- engine/src/runtime/function/window/window.cpp:18-23
- engine/src/runtime/function/render/shader/shader_library.cpp:17-49
- engine/res/shaders/bin/*.spv
解决方案
初始化时查询并记录 GL version、ARB_gl_spirv、所需 texture/bindless 功能。根据 capability 选择一种受支持的加载路径：
- 要求 GL 4.6/ARB_gl_spirv，不满足则清晰报错并退出；或
- 提供 GLSL source/经 SPIRV-Cross 转译的 GLSL fallback；或
- 在 RHI 内部封装 capability-aware shader creation，并向引擎返回结构化失败。
ShaderLibrary 必须将任何必要的核心 shader（Fullscreen、Sprite、FXAA、Present）加载失败视为初始化失败，而非仅记录日志后继续运行。
验收
- 支持的 GL driver 上所有 Only2D shader 成功加载；不支持的驱动给出明确诊断，不会进入空 pipeline 渲染。
P2：Only2D pass 的 RenderGraph 合约与实际 blackboard 写入不完整
现状
SpritePass 与 UIPass 实际会创建或覆盖 SceneColorKey，但其 Produces/Consumes 类型列表为空；PostProcess2D 同时声明 consume 和 produce 同一个 key，通用 validateBlackboard 按 pass 类型元数据可能拒绝这种链路或无法验证 Sprite -> UI -> PostProcess -> Present 的实际数据流。
此外，UI bindless 分支构建 pipeline/binding 后没有发出 graphics state/draw，也没有将 color target 转为 ShaderResource；这会造成 UI 在 bindless active 时不显示，并可能让后续 pass 读取错误状态。
风险
当前 RenderGraph 内部资源依赖可部分工作，但 pipeline 级别验证与实现不一致，后续重构或调序很容易引入断言、错误 cull 或资源状态问题。UI bindless 路径则为明确的功能缺陷。
证据
- engine/src/runtime/function/render/render_pipeline/passes/render_sprite_pass.h:19-20
- engine/src/runtime/function/render/render_pipeline/passes/render_ui_pass.h:19-20
- engine/src/runtime/function/render/render_pipeline/passes/render_post_process_2d_pass.h:14-24
- engine/src/runtime/function/render/render_pipeline/renderer.cpp:18-69
- engine/src/runtime/function/render/render_pipeline/passes/render_ui_pass.cpp:156-186
- engine/src/runtime/function/render/render_pipeline/passes/render_ui_pass.cpp:287-293
解决方案
1. 让 pass-level Produces/Consumes 与真实 blackboard 行为一致，或删除这套重复验证，仅以 RDG resource access 为唯一依赖真相；两者不能长期并存且语义不一致。
2. 将 SceneColorKey 定义为可覆盖的链式资源，验证逻辑应支持 producer replacement，不应把所有重复 producer 直接视为错误。
3. 完成 UI bindless 路径：创建/解析 pipeline 后必须设置 framebuffer、vertex/index buffer、binding sets、viewport 并 draw；结束时转回 ShaderResource。
4. 为 Only2D 固定链路添加 RDG dump/断言测试：Sprite -> UI -> PostProcess2D -> ImGui -> Present。
验收
- 开启/关闭 bindless 时，Sprite/UI 都正确渲染。
- RDG dump 显示每个资源有明确 producer/consumer 与正确状态转换。
实施顺序
1. P0 context 归属：先强制 OpenGL SingleThread，获得可重复的稳定基线。
2. P0 present：修复 default framebuffer 与 Present ABI，确保能看见正确画面。
3. P1 并发：停止 worker 触及 GL；固定 ImGui packet 与 render-thread 资源队列。
4. P1 生命周期与 resize：关闭顺序、0 尺寸窗口、renderer 重建。
5. P2 capability 与 RDG 合约：补齐 GLSL/SPIR-V 兼容策略、Only2D bindless/UI 和验证覆盖。
每完成一项都应回归：启动、空场景、Sprite、UI、ImGui、首次贴图加载、多贴图加载、resize、最小化恢复、正常退出。
非结论项
- 当前 workspace 中 engine/external/cutie-rhi 目录没有可读实现内容，因此未断言其 OpenGL SPIR-V 加载、FBO 0 支持或错误处理的具体实现；相关条目已按 capability 风险记录。
- 本文不替代运行时崩溃栈。拿到 call stack、GL debug callback 和 RHI validation 日志后，应将 P0/P1 项与实际故障地址交叉确认。