# GpuScene 与 GPU Driven 剔除

本文覆盖 GPU driven 路径的核心:`GpuScene`(场景数据的 GPU 镜像)、`GpuCulling`(compute 剔除 + bucket/indirect 参数构建)、以及 GBuffer 对 indirect 绘制的消费方式。

## 文件地图

| 文件 | 职责 |
|---|---|
| `runtime/function/render/gpu_driven/gpu_scene.h/.cpp` | GpuScene:对象注册、CPU 镜像数组、脏区间、稀疏上传 |
| `runtime/function/render/gpu_driven/gpu_scene_buffers.h` | GPU 数据结构定义(`GpuObjectMeta`/`GpuBounds`/`PrimitiveGpuData`/`GpuBucketTemplate` 等)、bucket hash |
| `runtime/function/render/gpu_driven/gpu_object_id.h` | `GpuObjectHandle`(SlotMap 索引)、`GpuObjectType`、`kGpuObjectFlagValid` |
| `runtime/function/render/gpu_driven/gpu_dirty_flags.h` | 对象脏标志 |
| `runtime/function/render/gpu_driven/gpu_driven_renderer.h/.cpp` | `GpuCulling`:剔除 dispatch、bucket count/scan/fill、模板与快照、readback |
| `runtime/function/render/render_scene/render_scene.cpp` | `syncPrimitiveGpuScene`/`syncSpriteGpuScene`/`syncLightGpuScene`:CPU 场景 → GpuScene |
| `res/shaders/gpu_culling_pass.comp` | 视锥剔除 compute(可见列表) |
| `res/shaders/bucket_count_pass.comp` / `bucket_scan_pass.comp` / `bucket_fill_pass.comp` | bucket 计数 / 前缀和 / indirect args 填充 |
| `render_pipeline/deferred_renderer.cpp` | `executeGpuCulling`(模板上传 + 逐视调度)、`buildGpuDrivenDrawCommands` |

## 1. GpuScene 数据模型

所有类型(Primitive/Sprite/Light)共享同一组 buffer,以 `GpuObjectHandle`(SlotMap 槽位索引)为对象空间:

| Buffer | 元素 | 用途 |
|---|---|---|
| `object_meta` | `GpuObjectMeta`(32B) | type/flags(bit0 = valid)/material_id/texture_id |
| `transforms` | `GpuTransform`(128B) | `local_to_world` + `world_to_local`(VS 侧按 `transform_index` 读取) |
| `bounds` | `GpuBounds`(32B) | **世界空间** center/extent/sphere_radius,供剔除使用 |
| `primitive_instance` | `PrimitiveGpuData`(40B) | `transform_index`/`mesh_id`/`index_count`/`start_index`/`base_vertex`(每 section 一份) |
| `primitive_render_instance` | `GpuPrimitiveRenderInstance`(80B) | instance 流(mat4 model + tint + params,与 CPU 路径 `InstanceSceneData` 逐字节一致) |
| `sprite_instance` / `light_instance` | 同名数据 | 2D/光照路径 |

上传由脏区间驱动:`DirtyRange`(start/end + 64 位脏字)支持**稀疏上传**(`flushSparseRange` 合并连续 bit 为 region);buffer 容量增长时重建并标记全量脏。

### 对象粒度:每 section 一个对象

`syncPrimitiveGpuScene` 按 `subMeshes.size()` 为每个 primitive 注册 **N 个 GpuScene 对象**(`RenderScene::m_primitive_gpu_handles`):

- 每个 section 持有自己的 `material_id`(材质名 hash)、`mesh_id`(VB RHI 指针低 32 位)、`index_count`/`start_index`/`base_vertex`
- transform/bounds/render_instance 每个 section 各存一份(section 数量变化时整体注销重建并强制全量刷新)
- sprite/light 仍走 `m_cpu_to_gpu_map` 单句柄

## 2. CPU → GPU 同步时序

```text
游戏线程: markPrimitiveDirty(id, flags) → m_pending_primitive_updates
渲染线程每帧: rebuildPipelineSceneData(cmd_list)
  ├─ processPendingPrimitiveUpdates(delta)   (upsert 有每帧预算)
  ├─ applyPrimitiveUpdate                    (CPU 侧 PrimitiveSceneInfo 更新)
  ├─ syncPrimitiveGpuScene/Sprite/Light      (写 GpuScene CPU 镜像)
  └─ gpu_scene->flushUpdates(cmd_list)       (脏区间上传,先于本帧绘制)
```

注意:`TransformChanged` 走 `applyPrimitiveTransform` 只更新 CPU 侧,GPU 侧由 `syncPrimitiveGpuScene` 消费同一条 delta 补齐。

## 3. GpuCulling 管线

每帧、每个 view 依次执行(`DeferredRenderer::executeGpuCulling`):

```text
uploadBucketTemplates ──► executeCulling ──► executeBucketBuild
(状态模板 + 快照)         (视锥剔除)         (count → scan → fill)
```

### 3.1 视锥平面提取(Gribb-Hartmann)

`Matrix4f = glm::mat4`(列主序,`operator[]` 取列),平面必须取**行**:

```cpp
const Matrix4f transposed = Math::Transpose(view_projection);
planes[i] = transposed[3] ± transposed[i];   // row3 ± row_i,内法线朝内
```

与 CPU 路径(`render_view.cpp` / `mesh_processor_base.cpp`)同构。投影矩阵自带 `FlipClipSpaceY`,镜像 Y 只会让上下平面互换(集合不变),不影响剔除结果。平面无需归一化(AABB 测试 `s > -r` 对平面长度缩放不变)。far/near 比值极大时远平面趋近 `(0,0,0,w)` 属正常退化,不影响结果。

### 3.2 剔除 compute(`gpu_culling_pass.comp`)

- 输入:6 平面 + `object_count`(std140 `CullingParams`,与 C++ `sizeof` 逐字节对齐)
- 每对象:flags bit0 有效 → 世界空间 AABB 对 6 平面做 `s > -r` 测试
- 输出:`visible_objects[]`(对象索引,atomicAdd 分配)+ `visible_count[0]`
- 另有 4 字节 readback(CPU 侧 1Hz 日志读上一帧可见数)

### 3.3 Bucket 构建(状态模板)

模板描述**绘制状态**(pipeline/binding/VB/IB),**不描述几何范围**:

- hash key = `type ^ material_id*16777619 ^ mesh_id*2166136261`(只含状态字段,**不含 ic/si**)
- `GpuBucketTemplate.key_check`(data[14])保存 32 位原始 hash;fill 端校验 `tpl.data[14] == raw_key`,碰撞桶(first-wins)中不匹配者跳过
- `template_map[hash_bucket] → template_index`,`uploadBucketTemplates` 每 generation 全量重写

count/scan/fill 三段:

```text
count: visible 列表 → 按状态 key 统计 bucket_counts[b].instance_count
scan : 单 group 前缀和 → bucket_bases[b](独占前缀)
fill : 每个可见对象 → 命中模板后
       args.index_count/start_index/base_vertex ← 物体自己的 PrimitiveGpuData(!!!非模板)
       args.start_instance_location ← object_idx
       写入 args[bases[b] + atomicAdd(current_offset)],ranges[b] 记 {first_arg, arg_count}
```

**几何范围来自物体自身数据**是设计关键:CPU 侧 draw cache 的合并(`MergeOpaqueMeshDrawSources`,相邻 section 合并 ic/si)与 GPU 路径完全解耦,多 section 模型(如多部件模型)每个 section 都能用自己的 index range 命中同一状态模板。

### 3.4 Generation(帧 vintage)体系

`kMaxGenerations = 3`,每帧每 view 一次 bucket build 占用一个 generation:

```text
帧 N:counter=C → 上传/写入 templates/map/args/readback[C%3] → counter=C+1
帧 N 绘制(gbuffer):消费 counter-2 槽 = 帧 N-1 的 args/ranges/map/snapshot(三者同 vintage)
```

- `getIndirectArgsBuffer(1)`、`acquireBucketDraws()`、`getBucketDrawSnapshots(1)` 三个接口的槽位公式一致,**必须成对使用**
- ranges 通过 readback 回读(CPU 枚举 draw 组),自带一帧延迟;可视作 GPU driven 路径的正常延迟
- `GpuBucketDrawSnapshot`(pipeline/binding sets/vertex bindings/index binding/shader_data)与模板同 vintage 存储——绘制状态必须取与 args 同帧的快照,否则桶列表重排时 template_index 会指向错误的桶

## 4. GBuffer 消费(`render_gbuffer_pass.cpp`)

```text
use_gpu_draws = gpu_driven_active && culling 有效 && instance buffer 有效
              && CPU buckets 非空 && acquireBucketDraws(gpu_draws) 非空
每 draw 组:
  snapshot = getBucketDrawSnapshots(1)[draw.template_index]
  writeBuffer(primitiveConstantBuffer, snapshot.shader_data)
  setGraphicsState(fb, snapshot.pipeline, snapshot.binding_sets,
                   viewport, snapshot.vertex_bindings + instance槽1, snapshot.index_binding, args)
  drawIndexedIndirect(draw.first_arg * sizeof(DrawIndexedIndirectArgs), draw.arg_count)
```

instance 槽 1 绑 `primitive_render_instance`,`start_instance_location = object_idx` 直接索引对象自己的模型矩阵。`InstanceSceneData` 与 `GpuPrimitiveRenderInstance` 布局逐字节一致,与 CPU 路径的 pipeline 输入布局兼容。任一环节缺失即整体回退 CPU 路径(1Hz 告警 `gpu-driven fallback`)。

## 5. 约定与陷阱(血泪清单)

1. **Set/Binding 宏**:`DOE_PASS_BINDING_INPUTn = n+1`,但 **`SAMPLER = 9` 占槽**,`INPUT8=10、INPUT9=11、INPUT10=12`——跨过 9 之后不再连续。新增 buffer 绑定时以宏为准,勿按序号脑补。
2. **跨端结构体步长**:C++ 结构体与 GLSL std430 的数组步长必须一致。`alignas(16)` 会把非 16 倍数的 struct(如 10×u32 的 `PrimitiveGpuData` 40→48)撑大,GPU 侧按 std430 步长读取导致从 1 号元素起整体错位。**只有尺寸本就是 16 倍数的结构体才允许 `alignas(16)`**。
3. **cbuffer(std140)与 C++ struct 逐字段对齐**:`CullingParams`/`BucketParams` 共用同一常量 buffer,各 pass dispatch 前重写自己布局需要的字段(count/scan/fill 用 `{object_count, max_buckets, template_count, 0}` 覆盖前 16 字节)。
4. **剔除平面取行不取列**(glm 列主序),并与 CPU 路径保持同一实现。
5. **剔除 bounds 必须是世界空间**:上传前用 `local_to_world` 变换 center、用线性部分绝对值变换 extent。
6. **同 vintage 原则**:args/ranges/template_map/snapshot/绘制状态五者必须取同一 generation 槽,任何跨 vintage 组合都会在桶列表变化时产生错配(症状:快速移相机时模型乱掉/闪屏)。
7. **几何与状态分离**:状态模板不做几何匹配(合并桶无法与单个 section 匹配),几何范围永远取自物体数据。
8. **每 section 一对象**:多材质/多 index range 模型必须展开注册,否则 GPU 路径只画第一个 section。
9. **改 bucket shader 后必须重编 shader**(`res/shaders/bin/*.dxil`),只编 C++ 会因 DXIL 与 root signature 不匹配在 PSO 创建时报错。

## 6. 开关与诊断

- `RenderSettings`:GPU driven 仅 D3D12 + bindless + compute queue 满足时激活(`gpu_driven_active`);`culling_path ∈ {CpuOnly, GpuOnly, CpuThenGpuVerify}`,不满足时强制 `CpuOnly`
- 日志(1Hz):
  - `GpuCulling: fill pass (objects=N, templates=M, generation=G)` — fill 调度与模板数
  - `GpuCulling: visible=X / Y` — 上一帧 GPU 剔除可见数 / 总对象数
  - `GpuCulling hash dump objects:[...] templates:[...]` — CPU 镜像侧与模板侧的状态 key/桶号对照
  - `GpuCulling gpu buckets: total=... nonzero=... | draws:[...]` — GPU count 结果回读与 draw 组
  - `GBufferPass: gpu-driven fallback (...)` — 回退原因(culling/instance_buffer/buckets/draws)
- 排障路径:visible 异常 → 查平面/planes 日志;桶号对不上 → 查 hash dump 两侧字段;total 正常但 draws 空 → 查 template_map/ranges vintage 与 key_check
