# 资源体系与游戏侧链路

本文覆盖"场景组件如何变成 GPU 绘制"的完整资源链路:加载、引用、渲染侧表示、缓存复用与每帧同步协议。

## 1. 引用机制:PPtr(runtime/core/object/pptr.h)

场景组件对资产的引用统一用 `PPtr<T>`,同时持有:

- `ObjectID m_id`:持久资产 ID(asset_id + local_id);
- `InstanceID m_instance_id`:运行时实例 ID(加载后回填);
- `m_legacy_path`:遗留路径(兼容)。

`get()` 解析顺序:InstanceID 直查 → ObjectID 查找(`Object::FindInstanceID`)→ 失败返回 null。这支持"组件先存 ID、首次使用时惰性加载并回填"的模式。

## 2. 加载入口:ResourceManager(runtime/resource/)

单例。`loadObject<T>(asset_id, local_id)` 流程:

1. `findLoaded<T>`:按 InstanceID 查缓存(校验类型名);
2. 未命中按 `if constexpr` 分发到私有加载方法:

| 类型 | 加载路径 |
|---|---|
| `Texture2D` | `LoadTexture2D` → **TextureManager::createTexture**(见 §3) |
| `Sprite` | `LoadSprite` → `SpriteManager::Create`(见 §4) |
| `Material` | `Material::Create`(读 `.domat`,JSON 参数 + 纹理路径) |
| `Mesh` | `Mesh::Create`(Assimp 导入,见 §5) |
| 其他 | Anim2DClip / Skeleton / AnimClip / AnimatorController / Tileset / AudioClip |

`loadObjectByPath<T>(FileID)`:Sprite 走子对象引用(切片),其余 `resolvePathToRef` 后转 `loadObject`。

**关键约束**:任何线程都可以调用加载(如 World 系统 worker 中);GPU 资源创建经由 `GDrawCommandList`(延迟实体化,见 threading.md),stb 解码等 CPU 工作发生在调用线程。

## 3. 纹理:TextureManager(render/texture/)

`createTexture(path, ref, cmd_list, staging)`(texture_manager.cpp:98)流程:

```text
绝对路径 → TextureBlob(stb 解码,强制 RGBA,flip Y;HDR 用 stbi_loadf)
  → 格式:HDR ? RGBA32_FLOAT : RGBA8_UNORM
  → GfxTextureDesc(dimension=2D, mip=1, automaticStateTracking=ShaderResource)
  → cmd_list.createTexture(desc)            [Proxy,可能延迟实体化]
  → cmd_list.writeTexture(handle, 0, 0, pixels, row_pitch)   [延迟上传]
  → texture->setGpuHandle(handle)
  → bindless:m_slot_lut 分配 slot + m_descriptor_table->allocateSlot()
             → GfxBindingSetItem::Texture_SRV → device->writeDescriptorTable
             → texture->setSlot/setDescriptorIndex
  → 存 m_texture2d_cache(按 InstanceID)
```

- **fallback**:1×1 白 Texture2D 与 1×1×6 黑 TextureCube 常驻(实体化失败/槽解析失败时兜底)。
- **cubemap**:6 面方图,±Y 面旋转校正,RGBA32_FLOAT,缓存于 `m_cubemap_cache / m_cubemap_by_path`;`SkyLightSystem` 使用。
- **槽解析**:`resolveSlot(slot)` 经 `m_slot_lut` 还原 `Texture2D*`;`SharedRenderService::resolveTextureBySlot` 返回 `GfxTextureHandle`(SpritePass traditional 路径用)。

## 4. Sprite:pixel2d/

- **Sprite**(sprite.h):`Object` 子类,`PPtr<Texture2D> m_texture` + UV rect + `pixels_per_unit`(默认 10)。`getAtlasIndex()`:bindless 描述符索引优先,否则 slot——渲染侧据此索引图集。
- **SpriteManager::Create**(sprite_manager.cpp:35):加载图集 `Texture2D`(local_id 0)→ 读 `ImportSettings`(切片元数据)→ 按 local_id 取 SpriteMeta 换算 UV rect → 存静态管理表。
- Sprite 实例数据结构 `SpriteInstance`(64 字节对齐)+ 共享四边形 `kQuadVertices/kQuadIndices`(与 GpuScene 的 quad_vb/quad_ib 同源)。

## 5. Mesh 与 Material

**Mesh::Create**(mesh_draw/mesh.cpp:175):

```text
Assimp 导入(Triangulate | Normals | Tangents | JoinIdenticalVertices)
  → BuildMeshData:逐 submesh 填 MeshUploadData(位置/UV/打包法线 Snorm4x8/索引)
  → 交错顶点缓冲 → GDrawCommandList.createBuffer 建 VB/IB
  → 逐 section 生成 MaterialProperties → 落盘 materials/<mesh>_<i>.domat → PPtr<Material>
  → MeshLODData{ buffers, sub_meshes, screen_size } 存缓存
```

**Material**(material.h):纯资产(color/emissive/metallic/roughness + 4 个 PPtr 纹理),**不持有 GPU 资源**。GPU 侧表达由 **MaterialSystem** 负责:

- `MaterialTemplate`:shader + 光栅状态 + binding layout(经 `BindingLayoutCache`);
- `MaterialInstance`:纹理解析(bindless 时只记 descriptor index;否则经 `BindingSetCache` 建纹理 binding set)+ revision;
- `getResolvedMaterial(name, overrides, out)` 输出 `ResolvedMaterial`;纹理变化经 `invalidateForTexture` 失效。

## 6. 渲染侧服务与缓存(render_service/)

`SharedRenderService`(单例)聚合并按序初始化:

| 服务 | 职责 |
|---|---|
| `DescriptorTableManager` | bindless 全局描述符表:`createDescriptor` 去重、`allocateSlot/releaseDescriptor` 空闲槽回收 |
| `TextureManager` | 见 §3 |
| `ShaderLibrary` | 读 `shaders/shader_manifest.json`,按后端加载(.dxil/.spv/GLSL 源码),反射 + 按名取 shader |
| `PipelineStateCache` | PSO 缓存:key = MeshPassType + PrimitiveType + VS/PS 指针 + binding layouts + RenderState + FramebufferInfo(pipeline_state/pso_key.h);未命中才创建;另有磁盘缓存 |
| `GlobalSamplers` | point/bilinear/screen 三个静态采样器 |
| `RenderTargetSystem` | 渲染目标管理 |
| `FramebufferCache` | key = 附件纹理指针 + revision + mip/layer + sampleCount;**命中但 `!isRHIReady()` 时重建**(时序自愈);`invalidateTexture` 在渲染目标重建时失效旧条目 |
| `BindingLayoutCache` / `BindingSetCache` | layout 按 desc 缓存 + generation 号;set 缓存 key 含 layout generation 与全部 item 字段;未就绪条目重建 |
| `InputLayoutCache` | 顶点布局按 attributes + VS 缓存 |
| `MaterialSystem` | 见 §5 |

缓存层的共同契约:**key 稳定、命中即复用、未就绪(Proxy 尚未实体化)不缓存死条目而是重建**——这使渲染线程在资源晚一帧就绪时自动恢复。

## 7. 场景表示:RenderScene(render_scene/)

游戏侧对象与渲染侧数据的桥梁,双结构:

- **RenderObject**(UUID 索引):`PrimitiveRenderObject`(Mesh + section + override materials + mobility/shadow/transparent)、`SpriteRenderObject`(PPtr\<Sprite\>、UV、color、sorting、batch instances)——游戏语义,支持 diff;
- **SceneInfo 紧凑数组**:`primitive_scene_infos / sprite_scene_infos / light_scene_infos / ui_scene_infos` + 索引表——渲染消费的高效表示。

**增量同步协议**:

1. `addSprite/addPrimitive` 时与旧对象 `diff`(TransformChanged/TextureChanged/...)生成 pending 更新;
2. 每帧 `flushUpdates(cmd_list)`(render_system.cpp 第⑨步调用):按每帧预算(primitive 16 / sprite 64)处理 pending → `RenderSceneDelta` → 同步到 **GpuScene**(`registerObject/updateTransform/update*Instance`)→ 按脏区间上传 GPU 缓冲。
3. light 用 swap-remove;UI 每帧整批替换(`submitUIInstances`)。

## 8. World 系统同步协议(world/systems/)

以 `SpriteRendererSystem` 为例(sprite_renderer_system.cpp),所有渲染系统同构:

```cpp
getAccess()   // 声明 readsComponents<ID, Transform, SpriteRenderer>
              // → World TaskGraph 依据此并行调度(见 threading.md)

update(reg, dt):
    for entity in view<ID, Transform, SpriteRenderer>:
        active_sprites.insert(id)
        if (needsSync(entity))          // 未提交过 || 任一组件 dirty
            resolved = 解析 PPtr<Sprite>(已加载指针 → loadObject → loadObjectByPath)
            构造 SpriteRenderObject{UUID, sprite, world matrix(含自然尺寸), color, flags}
            RenderCommandQueue::AddSprite(std::move(obj))    // → MPMC 队列
            清 dirty
    pruneRemovedSprites(active_sprites)  // diff 出已删除实体 → RemoveSprite
```

各系统差异:

| 系统 | 输出 |
|---|---|
| MeshRendererSystem | `AddPrimitive(StaticMeshRenderObject)`;同步 AnimationPose → skinning matrices |
| LightSystem | `AddLight`(Point/Spot 数据) |
| SkyLightSystem | `AddLight`(Sky + cubemap,经 TextureManager 加载) |
| CameraSystem | **不走命令队列**,直接写 `GetCameraRegistry()` 全局通道 |

**首帧语义**:组件 `dirty` 初始为 true(保证首帧必提交);删除靠每帧 active 集合 diff,不依赖析构回调。

## 9. 端到端链路总结

```text
场景资产(.spr/.mesh/.domat) + 组件 PPtr
  → World 系统惰性加载(ResourceManager)
      ├─ Texture2D:stb 解码(worker)→ cmd_list.createTexture(延迟)→ bindless slot
      ├─ Sprite:图集 + 切片 UV
      ├─ Mesh:Assimp → VB/IB → Material 落盘
      └─ Material:参数 + 纹理引用
  → RenderCommandQueue(MPMC)→ 渲染线程 renderFrame 消费
  → RenderScene(diff + 预算)→ GpuScene(脏区间上传)
  → RenderViewFamily(视锥剔除)
  → RenderGraph pass 命令录制(缓存:PSO/Framebuffer/BindingSet/InputLayout)
  → DrawCommandList 命令流 → cutie ICommandList → GPU
```
