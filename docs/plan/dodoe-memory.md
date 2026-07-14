内存方案：统一分配器 + 分类统计（Godot 路线）
目的：高效的自定义内存分配方式 + 内存分类统计。Godot 路线——引擎分配走自定义入口、可统计、不重载全局 new、不自建容器/字符串（已有别名可后续接）。
与 GC 方案配套：GC 决定"何时回收"，本方案决定"分配到哪/回收回哪"。两者共享 ObjectHeap/Memory 入口。
整理日期：2026-07-13
一、现状与目标
1.1 现状
- 零自定义分配基础设施：无全局 new/delete 重载、无 IAllocator、无 Memory 入口、无统计。
- 容器/字符串全用 std：DynamicArray=vector、UnorderedMap=unordered_map、String=string（base.h:71-87）。
- Object 子类极少：真正注册到 Object 表的只有 Texture，系统对象走 Managed(unique_ptr)。对象池不是主角。
- 已有块分配器雏形：DrawCommandList 内嵌线性块分配器（MemoryBlock={data,size,offset}，allocate()=bump，块满新建，placement new 落命令，releaseBlocks() 整批释放，draw_command_list.cpp:1014-1039）。多线程（std::recursive_mutex），默认块 4096B。这是帧分配器雏形，但未泛化、未统计、底层 new UInt8[]。
1.2 目标（已确认）
- 高效分配 + 分类统计，两者并重：IAllocator 接口内含统计钩子，分配器优化与统计同步到位。
- 统计粒度：分类统计——按类型/分类（对象/纹理/命令/字符串等）统计占用 + 总量。分配时带类型标签。开销可接受，够定位"哪类吃内存"。
- Godot 路线：引擎分配走 Memory 入口（类 memnew/memdelete），不重载全局 new，不碰 std/第三方内部。自建容器/字符串不在本方案范围（String 已有别名可后续接）。
1.3 不做的事（明确边界）
- 不重载全局 operator new（不碰 std/第三方，语义最干净）。
- 不自建容器体系（用 std + pmr 按需，热点再换）。
- 不全局接管字符串（String 别名后续可换成自定义类，但非本方案目标）。
- 不预先池化所有 Object（Object 少，池化收益低；按热点类型针对性池化）。

---
二、架构设计
2.1 分层
┌─────────────────────────────────────────────┐
│  Memory 全局入口（统计 + 路由）               │  do_new/do_delete 宏、分类标签
│  - 按分类路由到 IAllocator                   │  分类统计表
│  - 记录每次分配（类型/大小/分类）             │
└───────────┬─────────────────────────────────┘
            │
┌───────────▼─────────────────────────────────┐
│  IAllocator 接口                              │
│  - allocate(size, align) → void*             │
│  - deallocate(void*, size)                   │
│  - owns(void*) → bool                        │
└──┬──────────┬──────────┬──────────┬─────────┘
   │          │          │          │
┌──▼──┐  ┌───▼────┐  ┌───▼───┐  ┌───▼────────┐
│Malloc│  │Linear  │  │Pool   │  │(未来)Slab  │
│(fallback)│ │(帧/块)│  │(定长) │  │(typed)     │
└─────┘  └────────┘  └───────┘  └────────────┘
2.2 IAllocator 接口
class IAllocator {
public:
    virtual ~IAllocator() = default;
    virtual void* allocate(Size_t size, Size_t align) = 0;
    virtual void deallocate(void* p, Size_t size) = 0;
    virtual Bool owns(const void* p) const = 0;
    [[nodiscard]] virtual const char* name() const = 0;
};
- 接口纯虚，后端可切换。统计在 Memory 层做（不污染每个后端）。
2.3 后端分配器
MallocAllocator（fallback）
- 封装 malloc/free，所有非热点分配的默认后端。
- 统一入口，进统计。
LinearAllocator（帧/块分配，泛化自 DrawCommandList）
- 把现有 DrawCommandList::MemoryBlock 泛化：{data, size, offset} + bump pointer + 块满新建。
- allocate O(1)，deallocate 空操作（整批 reset()/release() 时释放块）。
- 多线程：内置 std::recursive_mutex（沿用现有 DrawCommandList 模式）。后续可演化为 per-thread cache。
- 用途：draw command、每帧渲染临时数据、命令录制缓冲。
- 重构 DrawCommandList 用它——消除重复、接入统计。
PoolAllocator（定长块池，按需）
- 固定块大小自由链表，分配/释放 O(1)、零碎片。
- 按热点类型针对性建，不全局。候选：高频小对象（profile 后定）。
- 与 GC 方案的 ObjHandle/对象回收配套：对象 RC 归零回收到池而非 delete。
SlabAllocator（typed，未来，GC 配套）
- 每具体类型一桶（GC 文档 §3 提及）。当 RC 启用、对象需池化回收时引入。本方案先不实现，留 IAllocator 接口可平滑接入。
2.4 Memory 全局入口 + 分类统计
class Memory {
    static UnorderedMap<AllocCategory, CategoryStats> s_stats;  // 分类统计
    static MallocAllocator s_malloc;                            // fallback
    static LinearAllocator* s_frame;                            // 帧分配器（per-frame）
    // ... 各分类 → IAllocator* 路由表
public:
    // 带分类标签的分配入口
    static void* allocate(Size_t size, Size_t align, AllocCategory cat, const char* typeName);
    static void  deallocate(void* p, Size_t size, AllocCategory cat);

    // 统计查询（供编辑器面板）
    static const CategoryStats& getStats(AllocCategory cat);
    static void dumpAll();  // 日志输出全分类统计
};

enum class AllocCategory : UInt8 {
    Object,       // Object 子类
    Texture,      // 纹理（GPU 侧另计）
    RenderCmd,    // draw command（LinearAllocator）
    Resource,     // Asset 等
    String,       // 字符串（未来 String 接入后）
    Container,    // 容器内部（未来 pmr 接入后）
    Misc,         // 其它
};

struct CategoryStats {
    std::atomic<Size_t> current_bytes{0};   // 当前存活字节
    std::atomic<Size_t> peak_bytes{0};      // 峰值
    std::atomic<UInt64> alloc_count{0};     // 累计分配次数
    std::atomic<UInt64> dealloc_count{0};   // 累计释放次数
};
do_new/do_delete 宏（仿 Godot memnew，带分类）
#define do_new(T, cat, ...)        (new (Memory::allocate(sizeof(T), alignof(T), cat, #T)) T(__VA_ARGS__))
#define do_delete(p, cat)          do { (p)->~T(); Memory::deallocate(p, sizeof(T), cat); } while(0)
- 默认分类 Misc，热点调用点显式指定分类。
- placement new 落在 Memory::allocate 返回的内存上，析构后 deallocate。
- 不强制全引擎立即改用——存量 new/create_ref 渐进迁移，新代码用 do_new。

---
三、与 GC 方案的配套
GC 方案（RC + cycle detector）决定"何时回收"，本方案决定"分配到哪/回收回哪"。共享入口：
GC: ObjHandle 构造 → 对象创建
        ↓
本方案: ObjectHeap::construct<T>() → Memory::allocate(cat=Object) → IAllocator（Pool/Slab）
        ↓
GC: RC 归零 → 判定回收
        ↓
本方案: 对象析构 → Memory::deallocate → 回收到 Pool/Slab（非 delete）
- ObjectHeap 是 GC 与内存方案的接合点：GC 的对象创建/回收经它，它内部调 Memory::allocate/deallocate 路由到 Pool/Slab。
- GC 文档里的 m_strong_refs/trace() 是 GC 侧；本方案的 IAllocator/Memory 是分配侧。两者通过 ObjectHeap 解耦但协作。
- GC 阶段 G1（启用 RC）时，对象创建从 create_ref/new 切到 ObjectHeap::construct，本方案的 Pool/Slab 同步启用。

---
四、迁移计划
Phase M0 — 地基（无行为变化）
1. 实现 IAllocator 接口 + MallocAllocator。
2. 实现 Memory 全局入口 + AllocCategory + CategoryStats + do_new/do_delete 宏。
3. Memory 内部 MallocAllocator 作为 fallback，统计先只覆盖 do_new 路径（存量 new 暂不统计，渐进迁移）。
4. 编辑器内存统计面板雏形（显示各分类 current/peak/count）。
Phase M1 — 泛化块分配器 + 接入热点
5. 实现 LinearAllocator（泛化自 DrawCommandList::MemoryBlock）。
6. 重构 DrawCommandList 用 LinearAllocator，删除内嵌 MemoryBlock 重复逻辑。
7. draw command 分类接入统计（cat=RenderCmd）。
8. 验证：DrawCommandList 行为不变 + RenderCmd 分类统计可见。
Phase M2 — 对象池（按热点，非全局）
9. 实现 PoolAllocator（定长块）。
10. profile 确定高频小对象类型，针对性池化（候选：渲染对象、命令子结构）。
11. 与 GC ObjectHeap 接合点预留（GC G1 启用时填）。
Phase M3 — 渐进迁移存量
12. 存量 new/create_ref/create_scope 按子系统迁移到 do_new/ObjectHeap，补分类标签。
13. String 别名后续可换成自定义类（走 cat=String），非本方案强制。
14. 容器热点按需换 std::pmr（走 cat=Container），非全局。
Phase M4 — 调试增强（可选）
15. do_new 调试模式记调用点（文件:行），支持逐次查询（比分类更细，仅 Debug 开）。
16. 内存泄漏检测（分类统计的 alloc/dealloc count 不平衡 = 泄漏线索）。

---
五、关键设计决策与理由
决策
理由
不重载全局 new
Godot 路线，不碰 std/第三方，语义干净。统计靠 do_new 宏 + 渐进迁移。
分类统计而非逐次调用点
用户确认分类粒度。逐次开销大仅 Debug，分类开销可接受、Release 常开、够定位"哪类吃内存"。
LinearAllocator 泛化自 DrawCommandList
已有块分配器雏形，不重复造轮子。重构消除重复 + 接统计。
Pool 按热点类型非全局
Object 少，全局池化收益低。profile 驱动针对性池化。
do_new/do_delete 而非重载 new
编码规范接管引擎代码，std/第三方内部不接管。与 Godot memnew 同思路。
String/容器不全局接管
收益不抵成本。String 已有别名可后续接，容器热点按需 pmr。
与 GC 共享 ObjectHeap
分配侧与回收侧解耦但协作，避免两套对象创建路径。

---
六、与三引擎对照

UE
Godot
Unity
本方案
全局 new 重载
✓
✗
部分
✗
引擎分配入口
FMalloc
memnew/Memory
内部
Memory + do_new
分类统计
✓
✓
弱
✓（分类级）
自建容器
✓
✓
内部
✗（按需 pmr）
帧分配器
✓ FMemStack
局部
✓
✓ LinearAllocator（泛化自 DrawCommandList）
对象池
✓
✗（RC）
部分
✓ 按热点（Pool）
路线
全局接管
引擎入口+统计
分层
Godot 路线
本方案是 Godot 路线的本地化：复用已有块分配器、分类统计、按热点池化、不全局接管。

---
七、风险与对策
风险
对策
do_new 宏用错分类 → 统计失真
编码规范 + code review；分类标签在调用点显式，易审
LinearAllocator 多线程开销
沿用 std::recursive_mutex（现有模式）；后续 per-thread cache 优化
Pool 类型选错 → 池化收益低
profile 驱动，不预先全池；先测后池
存量 new 不进统计 → 统计不全
渐进迁移，按子系统；统计面板标注"已覆盖分类"
与 GC 接合点 ObjectHeap 设计偏差
Phase M0 先留接口，GC G1 启用时对齐
do_new placement new 析构遗漏 → 泄漏
do_delete 宏强制调析构；RAII 句柄（ObjHandle）封装避免裸 do_new

---
八、验证
- Phase M0：do_new/do_delete 单测（分配/释放/统计计数正确）；编辑器面板显示 Misc 分类。
- Phase M1：DrawCommandList 重构后行为不变（渲染输出一致）；RenderCmd 分类统计随帧增长/释放可见。
- Phase M2：池化对象分配/释放 O(1)、零碎片；与 new 对比 benchmark。
- Phase M3：迁移后各分类统计覆盖度提升；无 UAF/双释放（ASAN）。
- 集成：/run 启动编辑器，加载/卸载场景、热重载脚本、退出——内存统计面板各分类 current_bytes 应回落（无泄漏）。
- 用 /verify 确认改动行为。

---
附录：现状关键事实
- DrawCommandList 块分配器：MemoryBlock={data,size,offset}（draw_command_list.h:23），allocate() bump + 块满新建（draw_command_list.cpp:1014-1039），releaseBlocks() 整批释放，多线程 std::recursive_mutex（draw_command_list.h:54），默认块 4096B。
- Object 子类极少：注册到 s_instance_map 的只有 Texture（texture_manager.cpp:110,145）。系统对象走 Managed(unique_ptr)。
- 容器/字符串全 std：base.h:71-87（String/DynamicArray/UnorderedMap 都是 std 别名）。
- 零自定义分配基础设施：无 IAllocator、无 Memory、无全局 new 重载、无统计。
- 多线程渲染：DrawThread/RenderThread（system_context.cpp:170-199）。
- GC 方案配套点：ObjectHeap（GC 文档 §3）—— GC 创建/回收经它，路由到本方案 Pool/Slab。