GC 形态设计：RC + Cycle Detector（完整 GC，推迟实现）
本项目（dodoe）未来的 GC 形态定为 引用计数（RC）为主 + 周期性 cycle detector 兜底。本文档定义该形态的完整设计、与现状的契合、当前要预留的装备口子、以及未来分阶段填充的实现路径。
核心原则：现在只立接口与规范（零行为变化），未来按需填充实现。 GC 是"填充"不是"重构"。
整理日期：2026-07-13

---
一、为什么选 RC + Cycle Detector
1.1 纯 RC 的局限
RC（引用计数）用"计数归零即回收"替代可达性扫描。它无法处理循环引用：A 持 B（B 计数+1）、B 持 A（A 计数+1），外部都不再持有时，A/B 计数均为 1，永不归零，永久泄漏。
项目变大后循环引用几乎必然出现：行为树节点父子双向、UI 控件父子双向、观察者模式、脚本对象 ↔ C++ 对象双向。纯 RC 不是完整 GC，顶不住变大后的循环引用。
1.2 纯 mark-sweep 的不契合
mark-sweep（标记-清扫）自动处理循环引用，但：
- STW 停顿与本项目的多线程渲染（DrawThread/RenderThread）冲突。
- "批量延迟回收"与本项目"显式销毁"的对象心智冲突（对象死亡时机本已确定）。
- 需要反射标注引用字段 + 根集枚举 + 三色标记，基建量大。
1.3 RC + Cycle Detector 的选择理由
- RC 为主：日常对象确定性回收，无 STW，符合显式销毁心智。
- Cycle detector 兜底：周期性扫描专门找"计数>0 但实际不可达"的循环垃圾（Python gc 模块、CoreCLR 自身 GC 均用此路）。补全 RC 的循环引用盲区。
- 与现状高度契合：
  - 本项目已有 RC 雏形（Ref=shared_ptr、Scope=unique_ptr）。
  - 已有弱引用破环工具（PPtr，ID 间接弱句柄，等价于 weak_ptr）。
  - 对象所有权图当前是 DAG 无环（已验证：Material/RenderGraphPass/AnimClip2D/Texture 均无反向强引用），RC 当前可用，cycle detector 当前不需要。
- 本质：RC + cycle detector 里的 "cycle detector" 就是 mark 的一种。本方案没有真正避开 mark，只是把它从"现在实现"推迟到"未来作为 cycle detector 按需实现"。推迟是对的（现在无环、用不上、无法验证），但完整性最终仍依赖它。
1.4 与之前否决的"RC+mark+slab 大方案"的区别

大方案（已否决）
本方案
时机
现在实现整套 RC+mark+slab
现在只立接口，未来按需填
trace()
现在实现
现在空预留，未来填
ObjHandle
现在全量迁移
现在定义，新代码用，存量逐步过渡
验证
现在无环无法验证 trace 正确性
未来有环时填，可针对性测试
风险
高（用不上的机制 + 无法验证）
低（空虚函数 + 编码规范）

---
二、完整设计
2.1 对象模型
class Object {
    // —— 现有 ——
    InstanceID m_instance_id{0};
    static UnorderedMap<InstanceID, Object*> s_instance_map;      // GC 堆枚举基础（已有）
    static UnorderedMap<UInt64, InstanceID> s_id_to_instance;

    // —— RC 装备预留（当前空/不启用）——
    std::atomic<UInt32> m_strong_refs{1};     // 强引用计数
    std::atomic<UInt32> m_weak_refs{0};       // 弱引用计数（ WeakHandle 用）
    std::atomic<UInt8>  m_alive{1};           // 存活标志（fake-null + cycle detector 用）

    virtual void trace(TraceVisitor& v) const {}   // cycle detector 预留口子

    virtual void onDestroy() {}               // 子类钩子：释放 GPU handle 等
    virtual ~Object() {
        if (m_instance_id) { onDestroy(); notifyCSharpDestroyed(m_instance_id); ReleaseInstanceID(m_instance_id); }
    }

    void addRef();
    void releaseRef();   // 归零 → onDestroy() → 析构 → 回收
};
2.2 引用分类（GC 语义）
类型
C++ 类型
GC 角色
当前状态
强引用（阻止回收）
ObjHandle<T>（未来）/ Ref<T>=shared_ptr（存量）
计入 m_strong_refs
Ref 已在用
弱引用（不阻止回收）
PPtr<T> / WeakHandle<T>（未来）
计入 m_weak_refs，不计 strong
PPtr 已在用
唯一所有权
Scope<T>=unique_ptr
RC 特例（计数恒 1）
已在用
关键：PPtr 已是天然弱引用，破环工具现成。比 Unity/UE 起点都好。
2.3 RC 回收路径（确定性）
外部持引用 +1（addRef） → ... → 外部释放 -1（releaseRef）
                              → m_strong_refs 归 0
                              → onDestroy()（子类释放资源：GPU handle 等）
                              → 通知 C# wrapper 失效（fake-null）
                              → ReleaseInstanceID（摘注册表）
                              → 析构 → 内存回收
弱引用在 strong 归零后仍可通过 m_weak_refs 知道对象已死，WeakHandle::get() 查 m_alive 返回 null。
2.4 Cycle Detector 回收路径（兜底，周期性）
周期触发（挂帧末 submitAndWait 之后，节流）
  → 快照根集（见 §3.2）
  → trial deletion：从根集 trace，标记可达
  → 对未标记但 m_strong_refs>0 的对象，rescan 其 trace() 子节点
  → 确认所有 strong 引用都来自其它未标记对象 = 真循环垃圾
  → shutdown + 回收该循环
不能单遍 mark：m_strong_refs>0 可能是"渲染线程栈上临时持 ObjHandle"，不是环。必须 trial deletion（快照 + rescan）区分"环"与"临时持有"。这是 Python gc 的核心算法。
2.5 跨世界协调（与 C# CoreCLR GC）
- C++ 对象：RC + cycle detector（本项目负责）。
- C# wrapper：CoreCLR 自动 GC（CoreCLR 负责，本项目不管内存）。
- 跨世界一致性：C++ 回收时 notifyCSharpDestroyed → C# fake-null（obj == null）。C# wrapper 何时被 CoreCLR GC 回收与本项目无关。
- C# 持有 C++ 对象：C# wrapper 创建时 native_handle_acquire（strong +1），finalizer 时 native_handle_release（排队到主线程 SPSC，帧始 drain，避免 finalizer 线程竞态）。
2.6 C# 在 GC 方案中的角色边界（重要，避免混淆）
本方案的 RC + cycle detector 是 C++ 对象的 GC，不是 C# 对象的 GC。系统里两套 GC 并存，各管各的堆：

C++ 对象 GC
C# 对象 GC
实现
RC + cycle detector（本项目实现）
CoreCLR 分代 GC（.NET 自带）
管什么
C++ 的 Object（Texture/Asset/系统对象）
C# 的 wrapper（Object.cs/GameObject）
本项目要写吗
要（未来 G1/G2 阶段填）
不要，CoreCLR 自带
本项目干预吗
主体
不干预
C# 在本方案中只在三个"协调点"出现，没有任何一处是本项目在实现或干预 C# 的 GC：
1. C++ 销毁 → 通知 C# 失效（notifyCSharpDestroyed → fake-null）。这是 C++→C# 单向通知，让 C# 知道某 ID 的 C++ 对象没了。C# wrapper 壳子本身何时被 CoreCLR 回收，本项目不管。
2. C# 持有 → 影响 C++ 引用计数（native_handle_acquire/release）。改的是 C++ 的 m_strong_refs，不是 C# 的 GC。CoreCLR GC 负责回收 wrapper 内存，本项目只是借 wrapper 的 finalizer 钩子"顺便"通知 C++ 减引用。
3. C# 持有的对象算 C++ cycle detector 的根。这个"集"是 C++ 侧维护的（acquire 时加入、release 时移除），不去查 C#。
finalizer 的边界：文档提到 C# ~Object() 调 native_handle_release，看起来像"管 C# GC"，实则不是——CoreCLR 决定何时跑 finalizer（本项目控制不了也不控制），本项目只是在 finalizer 这个回调钩子里通知 C++ 减引用。类比：CoreCLR GC 是收走 C# wrapper 包裹的快递员，本项目只在包裹上贴张纸条"收走时通知 C++ 一声"，快递怎么收、何时收，本项目不管。
唯一因 C# 而引入的 C++ 侧同步：CoreCLR finalizer 在独立线程跑，直接调 native_handle_release 会与主线程/渲染线程并发改 m_strong_refs → 竞态。解法是 finalizer 不直接 release，推 SPSC 队列、主线程帧始 drain。这是"因 C# finalizer 机制而加的 C++ 同步措施"，主体仍在 C++ 侧，不属于管 C# GC。
结论：本方案工作量全在 C++ 侧；C# 侧只加几个 finalizer 和 acquire/release 调用，不碰 .NET 的 GC 机制本身。C# 堆的 GC 全程由 CoreCLR 负责。

---
三、当前要做的装备预留（零行为变化）
3.1 代码预留（最小）
1. Object 加空虚函数 virtual void trace(TraceVisitor&) const {}（cycle detector 口子，当前空实现）。
2. Object 加 std::atomic<UInt32> m_strong_refs{1} / m_weak_refs{0} / m_alive{1} 成员（当前不被驱动，存量继续用 shared_ptr）。
3. 定义 ObjHandle<T> 强引用句柄类型（RC 计数 + InstanceID 间接），当前不强制迁移。
4. TraceVisitor 前向声明（不实现），trace() 参数用它。
5. Object 析构自动 ReleaseInstanceID + onDestroy() + notifyCSharpDestroyed（这条是修现状 bug，非纯预留，见 §5）。
3.2 文档定义（编码规范，零代码）
1. 根集清单（cycle detector 未来枚举用）：
  - SystemContext 持的系统单例
  - Scene 持的实体
  - TextureManager::m_texture_cache
  - AssetManager::m_assets
  - C# 侧 acquired 句柄集
2. 引用规范：
  - 长期持有 Object 子类一律用 ObjHandle（强）或 PPtr（弱），禁止裸 Object* 长期持有。
  - 双向引用必须一强一弱（ObjHandle + PPtr），父持子强、子持父弱。
  - 新增对象类的引用字段遵守上述；存量 Ref/Scope 逐步过渡，不强制一次性迁移。
3. 未来接入步骤（见 §4）。
3.3 不做的事（明确边界）
- 不现在实现 trace() 任何子类（无环、无法验证，写了也是错的）。
- 不现在实现 cycle detector。
- 不现在强制迁移存量到 ObjHandle。
- 不现在实现 slab 内存池（与 GC 无关，另议）。

---
四、未来分阶段填充路径
阶段 G1：启用 RC（当确定性回收 + 弱引用预防不够时）
- 启用 m_strong_refs，ObjHandle 的构造/析构驱动 addRef/releaseRef。
- 存量 Ref/Scope 逐步迁移到 ObjHandle（按子系统，非一次性）。
- WeakHandle<T> 基于 m_weak_refs + m_alive 实现，替代/并存于 PPtr。
- 验证：每个子系统迁移后，确认无 UAF、无双重计数（Ref 与 ObjHandle 不能同时拥有同一对象）。
阶段 G2：cycle detector（当出现循环泄漏时）
- 实现 TraceVisitor（遍历对象声明的强引用子节点）。
- 逐子类填 trace()：声明该对象持有哪些 ObjHandle 强引用子对象。用单测验证每个类 trace 完整（漏声明 = 误回收活对象）。
- 实现根集枚举（按 §3.2 清单）。
- 实现 trial deletion 算法（快照 + rescan，非单遍）。
- 挂载点：SystemContext::tickOneFrame 的 renderTick() 末尾、submitAndWait() 返回后（确保 RenderThread/DrawThread 空闲，避免与渲染遍历竞态）。节流（如每 60 帧或按内存阈值）。
- 验证：构造已知循环（A↔B），确认被 detector 识别并回收；构造"仅栈临时持有"对象，确认不被误回收（false positive 测试）。
阶段 G3：跨世界 RC 联动（当 C# 持有 C++ 对象变复杂时）
- native_handle_acquire/release 接入 m_strong_refs。
- C# finalizer → SPSC 队列 → 主线程帧始 drain → releaseRef。
- C# acquired 句柄集纳入 cycle detector 根集。

---
五、与 Phase 0 现状修复的关系
本 GC 装备预留不替代 Phase 0（堵泄漏 + 跨边界通知 + fake-null）。两者关系：
Phase 0（现在做，修 bug）
GC 装备预留（现在做，立接口）
~Object 自动 ReleaseInstanceID
trace() 空虚函数
onDestroy() 释放 GPU handle
m_strong_refs/m_weak_refs/m_alive 成员
Scene::destroyEntity 加通知
ObjHandle<T> 类型定义
删 Asset 死计数
TraceVisitor 前向声明
C# fake-null
编码规范（根集/引用分类）
Phase 0 是 GC 预留的前置：注册表修复后才是可用的堆枚举基础；onDestroy() 是 RC releaseRef 归零路径的子步骤；fake-null 是跨世界失效机制，RC 回收时复用。两者一起做，Phase 0 顺手把 GC 口子立好，零额外成本。

---
六、关键风险与对策
风险
对策
trace() 漏声明强引用 → cycle detector 误回收活对象
阶段 G2 每类填 trace 时配单测；override 强制；考虑静态分析扫描 ObjHandle 成员
Ref 与 ObjHandle 同时拥有 → 双重计数
阶段 G1 按子系统迁移，迁移期不混用；同一对象同一时刻只一种拥有方式
跨线程 RC：渲染线程持 ObjHandle 时主线程 releaseRef 归零
m_strong_refs 原子；归零不立即析构，延迟到帧同步点（submitAndWait 后）统一处理；或 cycle detector 挂帧末避免竞态
C# finalizer 线程调 C++ releaseRef 竞态
finalizer 不同步 release，排队 SPSC，主线程帧始 drain
cycle detector 单遍 mark 误杀栈临时持有
必须 trial deletion（快照 + rescan），非单遍
InstanceID 复用导致 WeakHandle 指错对象
InstanceID 单调递增不复用（现状已如此）；slab 槽位复用前必须 ReleaseInstanceID 先摘注册表

---
七、判断"何时该进入 G1/G2"的信号
不预先排期，按信号触发：
- 进入 G1（启用 RC）信号：
  - 出现需要"多所有者共享 + 自动回收"的对象（当前 Texture/Asset 是管理器独占，不需要）。
  - shared_ptr 的控制块开销在 profile 中可见。
  - 想要对象回收比"管理器显式 erase"更自动化。
- 进入 G2（cycle detector）信号：
  - 内存监控发现"计数>0 但逻辑上应回收"的对象（循环泄漏）。
  - 引入了行为树/UI 父子/观察者等易成环系统。
  - 弱引用预防纪律出现漏网（code review 已挡不住）。
在此之前，确定性回收（Phase 0 修复后）+ 弱引用预防完全够用。GC 装备只是"保险"，不是"现在就要用的工具"。

---
八、与三引擎的 GC 对照

Unity C++
Godot
本项目（本方案）
C++ 对象 GC
mark-sweep（反射扫描 UPROPERTY）
无自动 GC（显式 QueueFree）
RC + cycle detector（未来补）
循环引用
自动处理
靠树形结构避免
RC 不行，cycle detector 兜底
回收时机
批量（收集周期）
确定性（QueueFree 即销毁）
RC 确定性 + cycle 周期兜底
STW
有（增量缓解）
无
RC 无；cycle detector 有（帧末节流）
弱引用破环
TWeakObjectPtr
树形单向
PPtr/WeakHandle
反射依赖
强（UPROPERTY 标注）
强（_bind_methods）
弱（trace() 手动声明，未来填）
本项目用 RC + cycle detector 是介于 Unity（全自动 mark-sweep）与 Godot（纯显式无 GC）之间的中间路线：日常确定性回收像 Godot，循环兜底像 Unity 但更轻（手动 trace 而非反射）。

---
附录：现状关键事实（GC 装备预留的基准）
- Object 注册表：s_instance_map(InstanceID→Object*) + s_id_to_instance(FileID→InstanceID)，AllocateInstanceID 唯一调用方是 TextureManager（texture_manager.cpp:110,145）。ReleaseInstanceID 零调用点（死代码）——注册表单调泄漏，析构后持悬空指针。Phase 0 修。
- Object 当前无 m_strong_refs/trace()/onDestroy()，析构 = default（object.h:26）。本方案预留加。
- Asset 有死计数 m_ref_count/addRef/releaseRef（asset.h:53,77-79 零调用点）。Phase 0 删，本方案的 m_strong_refs 是配合 ObjHandle 的成对设计，与此不同。
- PPtr（pptr.h:12）：FileID + InstanceID 间接弱句柄，get() 查 s_instance_map，不计引用——天然 GC 弱引用。
- Ref=shared_ptr / Scope=unique_ptr / Weak=weak_ptr（base.h:24-38）——RC 雏形已有。
- 对象所有权图当前 DAG 无环（已验证：Material/RenderGraphPass/AnimClip2D/Texture 均无反向强引用）——RC 当前可用，cycle detector 当前不需要。
- 跨边界全 ID-based，C# 不持指针；marshalling 是 NativeBindings 函数指针结构体；codegen 单一瓶颈。
- 多线程：tickOneFrame = updateTick（主线程）→ renderTick（submitAndWait 阻塞，system_context.cpp:170-199）。cycle detector 挂 submitAndWait 返回后。