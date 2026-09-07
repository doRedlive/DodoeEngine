# Memory 体系

本文覆盖 Dodoe 引擎的 CPU 内存管理体系:三层分配模型(Tier × Tag)、底层分配器(线性/池/mimalloc)、`Memory` 门面与统计、线程分配器与帧内存 epoch、STL 适配器、所有权指针的内存路径,以及延迟删除队列。

代码位置:`engine/src/runtime/core/memory/`;线程侧时序见 [../rendering/threading.md](../rendering/threading.md),RHI 侧 GPU 内存见 [../rendering/cutie-rhi.md](../rendering/cutie-rhi.md)。

## 1. 总览

```text
              ┌─ 分配入口 ─────────────────────────────┐
              │ Memory::Allocate(tier, size, align, tag)│
              └───────┬──────────────┬─────────────────┘
        Persistent    │              │ Frame / Scratch
              ▼                     ▼
   PoolAllocator(s_pools[tag])   thread_local ThreadAllocator
   未命中 → MallocAllocator         ├─ frame   (LinearAllocator, 64KB 块)
   (mimalloc)                       └─ scratch (LinearAllocator, 16KB 块)
                                        │ 每帧 ResetFrame + epoch 惰性重置
              ▼
   TierStats[tier][tag](atomic 统计)+ Tracy(Persistent)
```

模块文件:

| 文件 | 职责 |
|---|---|
| `allocator.h/.cpp` | `IAllocator` 接口 + `MallocAllocator` / `LinearAllocator` / `PoolAllocator` |
| `memory.h/.cpp` | `Memory` 门面:分层路由、统计、线程分配器注册、`DODOE_NEW/DODOE_DELETE` |
| `thread_allocator.h/.cpp` | `thread_local ThreadAllocator`、`ScratchArena` |
| `std_allocator.h` | STL allocator 适配器 + 容器别名 |
| `own_ptr.h` | `OwnPtr`(pimpl 友好的独占指针) |
| `managed.h` | `Managed<T, CreateInfo>` 生命周期模板 |

## 2. 分层模型:Tier × Tag

两个正交维度决定一次分配的路由与统计归属:

**AllocTier**(生命周期层,决定从哪分配):

| Tier | 语义 | 后端 |
|---|---|---|
| `Persistent` | 跨帧存活 | 按 Tag 注册的 PoolAllocator,未命中回退 MallocAllocator |
| `Frame` | 帧末整批回收 | `threadAllocator().frame` |
| `Scratch` | 临时/函数级 | `threadAllocator().scratch` |

**AllocTag**(用途标签,统计维度 + 池选择):`Object / RenderCmd / Texture / Resource / Misc`(memory.h:33)。旧接口 `AllocCategory`(Object/Texture/RenderCmd/Resource/String/Container/Misc)在 memory.cpp:224 被折叠为 Tag,并固定 Tier 映射:`RenderCmd → Frame`,其余 `→ Persistent`。

统计矩阵 `s_tier_stats[Tier][Tag]`:`TierStats{current_bytes, peak_bytes, alloc_count, dealloc_count}`,全 atomic(relaxed),`peak` 用 CAS 循环维护。

## 3. 底层分配器(allocator.h/.cpp)

三者都实现 `IAllocator{allocate/deallocate/owns/name}`:

### 3.1 MallocAllocator

全量转发 mimalloc:`mi_malloc / mi_free`,对齐超 `max_align_t` 时走 `mi_aligned_alloc`。`owns()` 恒 true(兜底语义)。`Memory` 的 `s_fallback` 即它。

### 3.2 LinearAllocator

Block 链式 bump 分配器,**线程安全**(每个实例一把 `std::recursive_mutex`):

- `allocate`:当前块尾部按对齐 bump;放不下则 `createBlock(max(default_block_size, size+align))` 新建块(mimalloc 分配块内存,Block 移动语义管理);
- `deallocate`:**空操作**——线性分配器按域整体回收;
- `reset()`:所有块 offset 归零(保留块内存复用);`resetTo(offset)`:回退到标记(栈式回收,`ScratchArena` 依赖它);
- `release()`:释放全部块;`reserve()` 预留;`transferFrom()` 转移/拼接块链(帧内存 epoch 重置外的块交接用);
- `owns`:遍历块地址范围判断。

### 3.3 PoolAllocator

定长块池,`FreeNode` 侵入式空闲链表:

- 构造时指定 `block_size/block_align`(不足 `sizeof(FreeNode)` 时抬升);chunk 大小 64KB,`refill()` 一次切一批节点串成链;
- `allocate`:size > block_size 直接返回 nullptr(调用方回退);空链时 refill;
- `deallocate`:头插回链;`owns`:遍历 chunk 地址范围;
- 线程安全(`std::mutex`)。

## 4. Memory 门面(memory.h/.cpp)

静态类,路由规则集中在 `Allocate(tier, size, align, tag)`(memory.cpp:85):

```text
Persistent → s_pools[tag] 存在 ? pool->allocate(失败再 fallback) : fallback
Frame      → threadAllocator().frame.allocate
Scratch    → threadAllocator().scratch.allocate
成功后 TierStats.recordAlloc;Persistent 层挂 TracyAllocS(调用栈 16 层)
```

`Deallocate` 对称:Persistent 先扫描所有池 `owns()` 命中则回池,否则 fallback;Frame/Scratch 空操作(回收发生在 reset)。

便捷入口:

| API | 等价调用 |
|---|---|
| `AllocatePersistent / DeallocatePersistent(size, align, tag=Object)` | Persistent 层 |
| `AllocateFrame(size, align, tag=RenderCmd)` | Frame 层 |
| `AllocateScratch(size, align)` | Scratch 层 |
| `Allocate(size, align, AllocCategory, type_name)` | 旧分类接口,Tier/Tag 折叠映射(§2) |
| `DODOE_NEW(T, cat, ...)` / `DODOE_DELETE(p, T, cat)` | placement new + 显式析构,带分类统计 |

池注册:`RegisterPool(tag, block_size, block_align)` 按 Tag 建池(重复注册告警);`Shutdown()` 销毁全部池。

## 5. 线程分配器与帧内存(thread_allocator.h/.cpp)

### 5.1 ThreadAllocator

```cpp
struct ThreadAllocator {
    LinearAllocator frame;    // 64KB 块,帧内存
    LinearAllocator scratch;  // 16KB 块,临时内存
    std::atomic<UInt64> last_reset_epoch{0};
};
```

- `threadAllocator()`:thread_local 指针惰性创建,并 `Memory::RegisterThreadAllocator` 注册到全局表(带锁);
- `Memory::InitThread()` / `ShutdownThread()`:线程入口/出口显式调用(渲染线程 loop、TaskScheduler worker、ThreadPool worker,见 threading.md §3);ShutdownThread 注销并销毁。

### 5.2 帧内存 epoch 惰性重置

帧内存的生命周期是"分配后活到帧末整批回收":

- `Memory::ResetFrame()`(每帧 renderFrame 调用):持锁遍历所有已注册 ThreadAllocator 强制 `frame.reset()`,然后 `AdvanceFrameEpoch()` 推进全局 epoch;
- 错过 ResetFrame 的线程(如 worker 正忙)在各自循环顶部比较 `Memory::CurrentFrameEpoch()` 与自己的 `last_reset_epoch`,`exchange` 成功者自行 `frame.reset()`——补齐语义,同时避免双重 reset。

### 5.3 ScratchArena

RAII 栈式临时分配:构造时记录 scratch 的 `usedByteSize()` 标记,析构 `resetTo(mark)`。适合函数内临时缓冲,不影响帧内存。

## 6. STL 适配器与容器别名(std_allocator.h)

三个无状态(或持有线程分配器指针的)allocator:

| 适配器 | 分配路径 | deallocate |
|---|---|---|
| `StdAllocator<T>` | `AllocatePersistent`,Tag=Misc | 正常回收 |
| `FrameAllocator<T>` | `AllocateFrame` | **空操作** |
| `ScratchAllocator<T>` | `threadAllocator().scratch`(拷贝共享同一底层 LinearAllocator) | 空操作 |

类型别名(引擎代码统一使用):

```cpp
PersistentArray<T> / PersistentMap<K,V> / PersistentSet<T> / PersistentString   // 跨帧容器
FrameArray<T> / FrameString                                                     // 帧内存容器
ScratchArray<T>                                                                 // scratch 容器
```

`container/containers.h` 的全局别名 `String / DynamicArray / UnorderedMap` 均基于 `StdAllocator`——引擎所有标准容器分配默认进 Persistent 统计。

## 7. 所有权工具的内存路径

所有智能指针统一走 `Memory::AllocatePersistent(..., AllocTag::Object)`:

- **Scope\<T\>**(base.h:40):独占,析构显式 `~T()` + `DeallocatePersistent`;`create_scope<T>(args...)` placement new;
- **Ref\<T\>**(base.h:105):自研引用计数,`ControlBlock{strong, weak, obj}` 一体分配;strong 归零析构对象,weak 归零释放 ControlBlock;
- **OwnPtr\<T\>**(own_ptr.h):独占但用 `delete` 表达式销毁——支持不完整类型(pimpl);持有方需定义 sized `operator delete` 把释放路由回 `DeallocatePersistent`,否则逃过统计(文件头注释详述);
- **Managed\<T, CreateInfo\>**(managed.h):`Create(info)` = `create_scope` + `initialize(info)`,失败返回 nullptr;`Destroy` = `shutdown()` + reset。引擎系统(ShaderLibrary、MaterialSystem、RenderPipeline 等)统一生命周期入口。

## 8. 延迟删除:DeferredDeletionQueue(container/deferred_deletion.h)

GPU 资源生命周期与帧 in-flight 相关,不能在提交时立刻销毁:

```cpp
DeferredDeletionQueue queue;
queue.enqueue(std::move(texture_scope), current_frame);  // Scope<T> 或任意闭包
queue.enqueueFunc([]{ ... }, current_frame);
queue.processCompleted(last_completed_frame);            // frame_number <= completed 的条目执行删除
```

Entry 为 `{std::function<void()> deleter, frame_number}`,按帧号排队。使用方:`render_frame_scheduler`(帧槽回收)、`shared_render_service`、`render_target_handle`(渲染目标延迟释放)。与 cutie-rhi 内部的 CommandListLifetimeTracker(RHI 侧)是两层独立的延迟释放。

## 9. 统计与调试

- `GetStats(tier, tag)`:读 TierStats 矩阵;
- `FrameUsedBytesTotal()`:所有线程 frame 分配器已用字节总和;
- `DumpAll()`:按 Tier/Tag 打 current/peak/allocs/frees 到日志;
- `ResetAllStats()`:清零;
- Tracy:Persistent 层分配/释放挂 `TracyAllocS/TracyFreeS`(16 层调用栈),`DODOE_TRACY_ENABLED` 控制。

## 10. 使用规则

1. **选层**:跨帧对象(系统、资产、容器)→ Persistent(默认容器别名即是);单帧数据(命令、可见列表、临时上传描述)→ Frame;函数内临时 → ScratchArena / ScratchArray;
2. **Frame 内存三不要**:不跨帧持有、不放大数据结构后长期引用、不在主循环外线程分配后假设"当帧还有效"(以该线程的 epoch 重置为准);
3. **Persistent 池是可选优化**:未 RegisterPool 的 Tag 自动落 mimalloc,不注册完全合法;
4. **新线程必须调用** `Memory::InitThread()` / `ShutdownThread()`,否则 frame/scratch 分配会惰性创建泄漏的 ThreadAllocator(thread_local 析构时未注销);
5. **OwnPtr 持有的 pimpl 类型**记得 sized `operator delete` 路由(§7);
6. 帧内存重置语义依赖 epoch 协议,新增工作线程循环顶部必须做惰性重置检查(threading.md §3)。

## 11. 相关文档

- [threading.md](../rendering/threading.md):RenderThread/TaskScheduler/ThreadPool 如何接入 InitThread/ResetFrame epoch 协议。
- [frame-flow.md](../rendering/frame-flow.md):每帧 ResetFrame 的时序位置、FrameStagingAllocator(staging 上传)。
- [cutie-rhi.md](../rendering/cutie-rhi.md):GPU 侧内存堆与上传机制。
- [core.md](core.md):Runtime Core 其他基础设施(Scope/Ref 详见其 §生命周期所有权)。
