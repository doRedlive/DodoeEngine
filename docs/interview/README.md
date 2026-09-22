# 面试讲解：自研 3D 游戏引擎（DodoeEngine）

本目录是简历"个人项目：自研3D游戏引擎"的完整讲解材料，全部内容以本仓库真实实现为依据（附代码位置）。按简历六大技术成果分文档组织，每篇包含：一句话定位、要解决的问题、架构设计、关键实现细节、高频追问。

## 文档导航

| 文档 | 对应简历条目 | 内容 |
|---|---|---|
| [01-project-overview.md](01-project-overview.md) | 项目概述 | 电梯陈述、技术栈、分层架构、设计主线、模块地图 |
| [02-render-graph.md](02-render-graph.md) | RenderGraph 声明式渲染架构 | 两段式契约、编译五阶段、并行执行、瞬态池、可视化面板、基准渲染器 |
| [03-mesh-drawing-pipeline.md](03-mesh-drawing-pipeline.md) | UE 架构同源渲染核心 | Proxy/SceneInfo、MeshDraw 链路、排序键、合批、PSO 缓存、GPU driven |
| [04-pbr-pipeline.md](04-pbr-pipeline.md) | 完整 PBR 物理渲染管线 | Cook-Torrance、Split-Sum IBL、PCSS 阴影、ACES、前向/延迟双路径 |
| [05-threading.md](05-threading.md) | 多线程架构 | 双线程模型、双命令通道、每帧预算、帧槽与帧内存、Tracy；含"GL 单线程 vs 引擎双线程"面试题 |
| [06-rhi-and-opengl.md](06-rhi-and-opengl.md) | 跨平台 RHI 硬件抽象层 | RHI 分层、Proxy 惰性实体化、Bindless、能力降级；含 OpenGL 后端难点 / GL 版本选择 / Vulkan vs OpenGL 三道面试题 |
| [07-csharp-scripting.md](07-csharp-scripting.md) | C# 脚本系统 | hostfxr 手工宿主、双向互调用、元解析器代码生成、热重载十步；含"反射架构"面试题 |
| [08-shader-system.md](08-shader-system.md) | Shader 体系（延伸题） | SPIRV-Cross 构建管线、三后端差异屏蔽、ShaderResourceGroup / MeshBatch / DrawList 概念 |
| [09-ecs-vs-actor-component.md](09-ecs-vs-actor-component.md) | ECS 架构（延伸题） | EnTT + 系统读写声明 + 命令缓冲；与 Actor-Component 对比；GameObject 门面混合设计 |
| [10-api-math-differences.md](10-api-math-differences.md) | 数学差异（延伸题） | Y 翻转 / 深度范围 / 绕序 / viewport / 矩阵布局 / std140 对齐及三层统一处理 |

> 实习工作经历（不鸣科技 MeshDrawingPipeline / MeshLODStreaming、红海无限 C++→C# 迁移）的讲解材料不在本目录，另行维护。

## 面试高频题索引

| 问题 | 位置 |
|---|---|
| 反射架构是什么样的 | 07 §三 |
| 完善 OpenGL 后端的难点与解决（framebuffer 等） | 06 §面试题 A |
| 使用的 OpenGL 版本及理由 | 06 §面试题 B |
| OpenGL 单线程 vs 引擎双线程，如何同步 | 05 §双线程同步机制（GL 特定部分见 06 §难点 3） |
| Vulkan 与 OpenGL 最大区别，缺失特性如何处理 | 06 §面试题 C |
| 为什么基于 NVRHI 而不是从零写 RHI | 06 §高频追问 |
| GPU driven 做了什么（加分项） | 03 §GPU Driven 路径 |
| 怎么定位 CPU/GPU 瓶颈 | 05 §Tracy；02 §调试与可视化 |
| 热重载的静态状态/协程怎么办 | 07 §诚实提醒 |
| 项目里的 Shader 体系（SPIRV-Cross 屏蔽后端差异） | 08 §一 |
| ShaderResourceGroup 是什么 | 08 §二 |
| MeshBatch / DrawList 概念 | 08 §三；深度见 03 |
| ECS 架构是什么样的，与 Actor-Component 区别 | 09 |
| OpenGL/Vulkan 数学计算差异如何处理 | 10 |
| LOD 怎么分级（个人项目现状） | 03 §GPU Driven 路径旁注；`MeshLODData{screen_size}` 已有、选择/流送未做 |
| 项目里最难的部分 | 01 §高频追问 |

## 开场陈述

### 30 秒版

> 这是我独立开发的一个跨后端现代化 3D 游戏引擎，从零搭底层架构。核心是四件事：一套跨平台 RHI 硬件抽象（D3D12/Vulkan/OpenGL 三后端 + Bindless），一套基于拓扑排序的 RenderGraph 帧图（自动依赖分析、Pass 裁剪、屏障推导、分层并行录制），一套参考 UE 的 Proxy/SceneInfo + PSO 缓存驱动的网格绘制管线，以及一套基于 CoreCLR 的 C# 脚本系统，支持热重载。全程用 C++20 和 Vulkan/D3D12 实现，Debug 下接 Tracy 做性能分析。

### 分层架构图（白板用）

```text
游戏侧   World ECS 系统（TaskScheduler 按读写依赖分层并行）
         → ResourceManager 惰性加载 → RenderCommandQueue
渲染侧   RenderSystem → RenderScene(脏标记 diff) → GpuScene
         → RenderViewFamily(视锥剔除) → RenderGraph(资源/屏障/裁剪)
         → DrawCommandList 命令录制 → 提交
抽象层   GfxContext / GfxBackend / Gfx* Proxy（gfx.h）
         → cutie-rhi（NVRHI fork）
后端     D3D12(默认) / Vulkan 1.3 / OpenGL 4.5(自研后端)
```

### 三条设计主线

1. **Proxy + 惰性实体化**：上层资源是持 desc 的轻量代理，GPU 对象渲染线程统一实体化，支持任意线程创建资源。
2. **命令录制优先**：所有操作先录进命令流（线性分配器 + 侵入式链表，零虚调用），再回放到 RHI。
3. **后端无关**：上层只面对 `Gfx*` 抽象与能力探测结果，后端差异封装在 Backend/cutie 内。

## 简历表述校准（防现场戳穿）

| 简历原话 | 实际实现 | 建议说法 |
|---|---|---|
| "支持运行时切换对比" | baseline 是启动时配置布尔开关，无运行时热切换 | "通过配置在 RenderGraph 主路径与固定管线基准渲染器之间切换，做正确性/性能对比验证" |
| "通过双命令队列控制任务预算" | 双通道是 resource/scene 两条命令流；预算是每帧 upsert 预算 16/64；图形+计算队列未真正并行 | "设计资源/场景双命令通道 + 每帧同步预算，平滑大场景 GPU 结构变更峰值" |
| "PSO 缓存驱动" | 内存 PSO 缓存真实；磁盘缓存（pso_disk_cache）已实现但未接线 | 只说内存 PSO 缓存按 key 复用；被问磁盘缓存就坦白"设计了格式但未启用" |
| "C# 热重载与运行时字段恢复" | 链路完整，但字段恢复的句柄映射有已知缺陷 | 强调机制与卸载/GC 流程，字段恢复留有余地（见 07 §诚实提醒） |
| "集成 PCSS 软阴影" | blocker search + 自适应 PCF 半径（shadow_csm.glsl），PCSS 简化版 | "基于 blocker search 的 PCSS 风格软阴影" |

其他注意：

- 别把工作经历的数据（顶点缓冲 700MB→300MB、Cocos→Unity 迁移）混进个人项目，面试官会分开问。
- dumpToJSON/dumpToDOT 是离线导出工具，ImGui 面板走的是 RenderGraphDebug 快照，两者没有接线关系。
- AsyncCompute 标志与 GPU compute queue 能力位存在，但当前没有 Pass 使用异步计算——"留了接口未启用"。

## 使用建议

1. 先背 01 的 30 秒陈述与三条设计主线；
2. 每篇主题文档按"一句话 → 问题 → 架构 → 细节 → 追问"顺序演练，细节部分挑两三个自己最熟的深讲（推荐：03 的排序键与合批、06 的 GLSL 重写器、02 的编译五阶段）；
3. 面试前一天过一遍"简历表述校准"表，把容易翻车的表述替换掉；
4. 高频追问答不上来时的话术："这个我当时是这么处理的……更深入的我没有继续做，但我的思路是……"——坦诚 + 给思路永远好于编造。
