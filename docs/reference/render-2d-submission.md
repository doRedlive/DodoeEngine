# 2D 渲染提交契约

适用于 Only2D / OnlyGUI 及其 Sprite、UI、PostProcess2D、Gizmo、ImGui、Present pass。

## 一帧的顺序

1. 收集资源请求、场景快照、可见实例与 UI 数据。
2. 构建图，登记逻辑资源、上传数据、附件和读取用途。
3. 从输出和 NeverCull pass 追溯依赖，排除无用 pass 和临时资源。
4. 录制 CPU 命令。一个依赖层只有一个 pass 时直接录制；多个独立 pass 可以并行录制。
5. 所有 pass 录制结束后，准备 Buffer/Texture，再准备 Framebuffer、BindingSet、PSO 和描述符写入。
6. 在同一次队列提交中先回放持久资源上传，再回放各 pass 的上传、状态设置和绘制。
7. 使用 GPU 完成查询回收命令数据；帧槽完成后才能复用临时资源。

Shader、sampler、输入布局和绑定布局属于设备级准备对象。CPU 映射上传区、swapchain 和回读资源有独立的设备访问契约。这里的延迟创建保证覆盖帧内 Buffer/Texture、Framebuffer、BindingSet、PSO 和描述符写入，不将设备初始化与帧提交混为一个阶段。

## Pass 的职责

- 使用 `uploadBuffer` 保存上传数据，不保留调用方临时内存的裸指针。
- 使用 `RenderGraphBufferUsage` 区分 Vertex、Index、Constant、ShaderResource 和 Indirect。
- 上传先于本 pass 的读取；图负责 CopyDest 到实际用途的切换。
- 清屏通过附件的 LoadOp 声明；pass 回调仅绑定逻辑状态并绘制。
- Atlas、ImGui draw texture 等实际读取的纹理必须进入图。相同物理包装对象的重复 import 返回同一图索引。
- 临时常量数据与临时纹理的绑定使用 `BindingSetFrequency::Pass` 或直接创建逻辑 BindingSet，由帧持有。稳定材质绑定才进入持久缓存。

SceneColorClear 为每个 2D view 提供背景，因此关闭绘制功能不会造成缺失 SceneColor。FXAA 仅在配置为 FXAA 时加入。Sprite 相同排序键保留原始顺序；UI 保留提交顺序，非 bindless 路径只合并连续纹理批次。

## 所有权与失败

`DrawCommandList` 移动、分离和拼接必须同时转移命令内存、资源准备计划及引用。准备计划按资源身份去重。Framebuffer/BindingSet 的依赖由执行器展开，而非依赖偶然的登记顺序。

持久资源的待上传批次只有在成功提交后才转入完成查询队列；准备失败保留批次重试。某个帧资源失败时可以继续提交已准备好的持久上传，但不回放该帧的绘制。回放状态设置失败后，相关 draw/dispatch 被阻止，直到成功设置新的状态。

ImGui 的纹理 ID 必须由 `ImGuiBuilder::RegisterTexture(handle)` 获取。注册表持有弱引用，序列化后的 packet 持有强引用。不要将任意裸指针强制转换成 `Ref<T>`。不再使用时调用 `UnregisterTexture`；已经产生的 packet 仍持有自己的引用。

临时资源池使用容量档位、预算和空闲年龄淘汰，淘汰时使关联 framebuffer/binding 缓存失效。窗口调整尺寸时，等待 GPU 后清理旧池和相关缓存。延迟删除只能使用已完成帧号，不能使用当前正在录制的帧号。

## 回归检查

`tests/unit/render/test_render_graph.cpp` 覆盖重复 import、无关消费者裁剪、NeverCull 依赖保留、上传数据所有权、buffer 用途和纯资源计划的移动/拼接。

运行时还应覆盖：不同纹理交替重叠的 UI；相同排序键的透明 Sprite；关闭 Sprite/UI/ImGui 的功能组合；FXAA 开关；透明 ImGui 边缘；连续调整窗口尺寸；新纹理首次使用；资源创建失败后重试；ImGui 子窗口关闭和缩放。

目前完成的是源码静态检查和用例接入，尚未取得这些场景的 GPU 运行结果。合成 shader 的源码与随项目分发的二进制资产需保持一致。
