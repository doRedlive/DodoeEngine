// do@Redlive

# MeshDrawCommand 缓存重构

## 核心结构

```cpp
// ===== 静态 GPU 状态，不管来源是 cache 还是 frame-local =====
struct CachedMeshDrawCommand {
    MeshPassType pass_type;
    GfxGraphicsPipelineHandle pipeline;
    DynamicArray<GfxBindingSetHandle> binding_sets;
    DynamicArray<GfxVertexBufferBinding> vertex_bindings;
    GfxIndexBufferBinding index_binding;
    GfxDrawArguments draw_args;
};

// ===== 每帧实例：指针 + per-instance 动态数据 =====
struct CachedDrawInstance {
    const CachedMeshDrawCommand* cached_cmd;  // 直接指针，来源：cache 或 frame-local
    UInt32 shader_data_index;
    UInt64 instance_offset;
};

// ===== 缓存容器：append-only，只增不删 =====
class MeshDrawCommandCache {
    UnorderedMap<MeshDrawCommandCacheKey, UInt32> m_key_to_index;
    DynamicArray<CachedMeshDrawCommand> m_commands;
public:
    const CachedMeshDrawCommand* findOrCreate(
        const MeshDrawCommandCacheKey& key, CachedMeshDrawCommand&& cmd);
    void invalidate();
};
```

## 两条路径

### Cached（静态场景物体）
```
每帧:
  processor.buildCommands(frame_commands, cache, out_instances, out_shader_data)
    ├─ 遍历 visible_primitives → MeshBatch → MeshBatchElement
    ├─ 计算 CacheKey（batch_hash + material_hash + pass_hash）
    ├─ 构建 CachedMeshDrawCommand
    ├─ cache.findOrCreate(key, cmd) → const CachedMeshDrawCommand*
    └─ out_instances.push({cached_cmd_ptr, shader_data_index, instance_offset})

shader 热重载时 → cache.invalidate() → 下一帧自动重建
```

### Dynamic（粒子、程序化几何、skeletal mesh 等每帧变化的 mesh）
```
每帧:
  processor.buildCommands(frame_commands, out_instances, out_shader_data)
    ├─ 遍历数据源 → 构建 CachedMeshDrawCommand（不查 cache）
    ├─ frame_commands.push_back(std::move(cmd))
    └─ out_instances.push({&frame_commands.back(), shader_data_index, instance_offset})

  frame_commands 是 DynamicArray<CachedMeshDrawCommand>，每帧 clear
```

### Dispatch（两条路共用）
```
DispatchCached(instances[], ...)
  ├─ BatchWritePassShaderData（GBuffer）
  └─ for instance in instances:
      cached_cmd = instance.cached_cmd                     // 不关心指针来源
      pipeline = cached_cmd->pipeline ?: pass_pipeline     // null → pass pipeline
      setGraphicsState(pipeline, bindings, VB, IB)
      addVertexBuffer(primitive_scene_buffer, instance.instance_offset)
      drawIndexed(cached_cmd->draw_args)
```

## GPU-Driven 路径

```
GPU culling compute shader → ExecuteIndirect
  ↑ 从 cached_draw_instances[0].cached_cmd 取 state 初始化
```

### 数据流总览
```
                    ┌─ Cached:   cache.findOrCreate() ─┐
Mesh/Material → processor                             ├→ CachedDrawInstance[] ─→ DispatchCached
                    └─ Dynamic:  frame_commands.push() ─┘
```

## 改动点

| 文件 | 改动 |
|------|------|
| `mesh_draw/cached_mesh_draw_command.h` | **新建** CachedMeshDrawCommand + CachedDrawInstance + MeshDrawCommandCache |
| `mesh_draw/gbuffer_mesh_processor.h/.cpp` | buildCommands 改签名：输出 CachedDrawInstance[] + cache 参数 |
| `mesh_draw/directional_shadow_mesh_processor.h/.cpp` | 同上 |
| `mesh_draw/mesh_draw_command_dispatcher.h/.cpp` | DispatchCached 替代 Dispatch（无 cache 参数，读指针） |
| `render_view/mesh_view_extension.h/.cpp` | mesh_pass_commands[] → cached_draw_instances[] + frame_commands |
| `render_pipeline/deferred_renderer.h/.cpp` | 持有 MeshDrawCommandCache，buildMeshDrawCommands 走新路径 |
| `render_pipeline/passes/render_base_pass.cpp` | Dispatch → DispatchCached |
| `render_pipeline/passes/render_shadow_pass.cpp` | 同上 |

## 删除

- `MeshDrawCommandDispatcher::Dispatch` — DispatchCached 替代
- `mesh_pass_commands[]` — cached_draw_instances[] 替代
- `assign_pipeline()` — DispatchCached 内部 fallback pass_pipeline

## 关键设计

1. **CachedDrawInstance 持有裸指针而非 index**：append-only storage 保证地址永有效，dispatch 零查表开销
2. **Cached/Dynamic 共用一个 dispatch**：区别只在上游指针来源，dispatch 层不区分
3. **pipeline 延迟绑定**：CachedMeshDrawCommand.pipeline = null 表示用 pass_pipeline，DispatchCached 处理 fallback
4. **frame_commands**：Dynamic 路径的 frame-local allocator，每帧 clear，processor 不持有 cache 时用它
