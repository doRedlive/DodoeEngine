# 03 UE 架构同源渲染核心：Proxy/SceneInfo + MeshDrawingPipeline

对应简历条目：UE 架构同源渲染核心。

## 一句话

> 参考 UE 渲染范式：游戏侧用轻量 Proxy，渲染侧展开为紧凑 SceneInfo 数组；再由 PSO 缓存驱动的 MeshDrawingPipeline 统一网格合批与绘制提交，减少逐帧构建渲染状态的 CPU 开销。

## 要解决的问题

1. 游戏对象与渲染数据耦合，每帧全量重建；
2. 逐帧重建 pipeline/binding/root signature 的 CPU 开销；
3. draw call 数量失控（无合批）。

## 架构设计

### Proxy 层（游戏语义）

代码位置：`render_scene/` + `render_object.h`

- `PrimitiveRenderObject`：mesh + section + override 材质 + mobility/可见/投影/透明/tint；`SpriteRenderObject`、`FoliageRenderObject`（按空间簇展开实例批次）。
- 每个对象实现 `diff()` 增量脏标记（`Mesh/Materials/State/ProxyData`），游戏线程与渲染线程之间只传 delta。

### SceneInfo 层（渲染消费的紧凑表示）

- `RenderScene` 持有 `primitive_scene_infos / sprite_scene_infos / light_scene_infos` 紧凑数组 + RenderId 索引表；
- `PrimitiveSceneInfo`：world transform、bounds、`MeshBatch` 数组、上一帧矩阵（供 motion vector）、`hasRelevantBatch(pass)`；
- 删除用 swap-and-pop 保持数组稠密；每帧同步有预算（见 05）。

### MeshDrawingPipeline（`mesh_draw/`）

数据链路：`MeshBatch`（per-section 剔除/编命令单元）→ `MeshDrawCommand`（pipeline + 固定 binding set 数组 + VB/IB + draw args）→ `MeshDrawList`（排序 + 合批 + 缓存/动态分流）→ `MeshDrawCommandCache` → `SubmitMeshDrawSources` 提交。

关键类：

| 类 | 职责 |
|---|---|
| `MeshPassProcessor`（mesh_processor_base.h:53） | 基类：视锥剔除 + `BuildDrawCommand` 填 pipeline/绑定/参数 |
| `LitMeshProcessor` | Opaque/Transparent 共用，切像素着色器与深度状态 |
| `ShadowMeshProcessor` | 仅 Global/View 两套 layout，支持级联 mask |
| `MeshPassRegistry` | 按 `MeshPassType{Opaque,Shadow,Transparent}` O(1) 查处理器 + 命令存储 |

### PSO 缓存（`pipeline_state/`）

- `GraphicsPipelineCacheKey`（pso_key.h:12）= pass_type + primitive_type + 五个 shader 阶段指针 + input layout + binding layouts 数组 + **完整 render state**（blend/depth-stencil/raster，含阴影 depth bias）+ VRS + framebuffer 格式签名；
- `PipelineStateCache::resolveGraphicsPipeline` 未命中才创建；`MeshPassType` 是 key 的一部分。

## 关键实现细节（显功力，挑两个深讲）

### 1. 排序键设计 `MeshDrawSortKey`

字典序：`pipeline → material → binding_set → VB → IB → index_count → start_index → base_vertex → depth_bucket`。排序后相同状态的 draw 天然相邻，为合批创造条件；透明额外按深度从远到近 + depth bucket 降序。

### 2. 不透明合批 `MergeOpaqueMeshDrawSources`（mesh_draw_list.h:30）

相邻 source 若 `canMergeWith` 且 primitive 常量一致，可合并：

- 实例范围连续（instance_offset 紧接）→ 累加 instance count；
- 索引范围连续且实例数相等 → 累加 vertex count。

混合生命周期的合并降级为 Frame。**这是 draw call 下降的直接来源**。

### 3. 缓存 vs 逐帧（命令复用）

- `Movable` 物体 → `CommandLifetime::Frame`，每帧重建；
- 静态物体 → `CommandLifetime::Cached`，`MeshDrawCommand` 常驻缓存，逐帧只重建 source/instance 数组；
- 缓存 key 含 `material_revision`；材质编辑时用"loose key（revision 归零）"命中后**原地替换**条目而不是新增（cached_mesh_draw_command.cpp:58-72）。

### 4. 并行编命令

`lit_scene_feature.cpp:151` / `shadow_scene_feature.cpp:124`：primitive 索引按 64 分块，`thread_pool->parallelFor`，每线程 `MeshPassThreadLocalCommandStorage`，最后 `mergeThreadLocal` 合并（排序单线程做，保证确定性）。

## GPU Driven 路径（简历没写，是被问"还能聊什么"的加分项）

代码位置：`gpu_driven/`，激活条件 bindless + compute queue（`ResolveFeatures` 决定），`CpuOnly / GpuOnly / CpuThenGpuVerify` 三档切换：

- `GpuScene`：全场景对象 GPU 镜像（object_meta/transforms/bounds/instance 等 8 个持久 buffer），`DirtyRange`（区间 + 64 位 bitmap）稀疏上传；
- compute 视锥剔除（Gribb-Hartmann 平面提取，与 CPU 路径同构）→ 可见列表；
- bucket count/scan/fill 三段 compute 构建 indirect args → `drawIndexedIndirect` 一次画完全部；
- generation 三元槽（帧 vintage）：args/ranges/template/snapshot 必须同 vintage 取，防快速移相机时桶列表重排错配；
- 任一环节缺失整体回退 CPU 路径（1Hz 告警 `gpu-driven fallback`）。

## 与 UE 的差异对比（个人项目口径）

被问"你的 MeshDrawingPipeline 跟 UE 的相比有什么区别"时用这张表：

| 维度 | UE | 我的实现 |
|---|---|---|
| 顶点抽象 | VertexFactory：顶点流布局与 shader permutation 动态绑定，类型几十种 | 没有 VertexFactory：固定交错顶点布局（位置/UV/Snorm4x8 打包法线）+ 固定 input layout，思路接近 RenderPrimitive——用数据结构约定代替抽象基类 |
| Pass 数量 | 几十种 MeshPass（深度/命中/速度/光照各档） | 3 个 MeshPassType：Opaque / Shadow / Transparent |
| 命令缓存 | FMeshBatch→FMeshDrawCommand，静态网格缓存 draw command + DDI | `MeshDrawCommandCache`：key=batch/material/revision/pass/pipeline 哈希，静态物体命令常驻，材质 revision 变化用 loose key 原地刷新 |
| 合批 | 静态/动态 instancing + 排序 | 排序键（pipeline→material→binding_set→VB→IB→depth_bucket）字典序 + 相邻实例/索引范围合并（MergeOpaqueMeshDrawSources） |
| Permutation | 完整 shader permutation 体系 | 无，只有 bindless / no-bindless 两档变体（manifest 层切换） |
| PSO | 预热 + 管线文件缓存（磁盘） | 内存 PSO 缓存（key 含完整 render state + framebuffer 签名）；磁盘缓存留了格式未接线 |
| GPU driven | GPUScene + DDI + Nanite | GpuScene 镜像 + compute 视锥剔除 + bucket/indirect，无 Nanite |
| 线程 | RHI 线程 + per-context 并行命令录制 | 渲染线程单提交 + RenderGraph 分层并行录制 + 网格命令分块并行构建 |

**收尾表述**："范式同源（独立绘制指令 + pass processor + 命令缓存），UE 的规模和通用性我没有复刻；我在小规模里保留了它最值钱的两点——**命令可缓存**和**状态排序合批**，并加了一条 UE 没这么做的完整 RenderGraph 前置链路。"

**LOD 现状（诚实口径）**：`Mesh` 已有 LOD 数据基础设施（`MeshLODData{ buffers, sub_meshes, screen_size }`，mesh_data.h:43，导入期生成），但当前只生成 LOD0，LOD 选择与流送逻辑未实现——可作为下一步计划讲。

## 高频追问

- **PSO 缓存 key 为什么包含 framebuffer 签名？** → PSO 与 RT 格式/采样数强相关，格式变化必须新建；签名由 attachments 格式数组 + depth + sample count 哈希。
- **磁盘 PSO 缓存有吗？** → **诚实回答**：设计了 `pso_disk_cache.*`（magic/version/shader_hash + entry blob 格式），但目前未接线，实际生效的是内存缓存；可以说"预留了磁盘格式待启用"。
- **合批的边界条件？** → 只合 Opaque；透明排序优先级高于合并；primitive 常量必须逐字节一致。
- **材质修改后缓存怎么办？** → revision 进缓存 key，loose key 命中后原地替换，缓存不膨胀。
- **和 UE 的 MeshDrawCommand 差异？** → 思想同源（独立绘制指令 + pass processor + 缓存），实现简化：无 render target 层的 pass mask 全量管线，Pass 类型固定三个。
