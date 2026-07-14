Cakery 编辑器重构方案 —— 类 Unity/UE 最终架构
目标：把当前 Qt 直连运行时的 Cakery 编辑器，重构为分层的、以 命令(Command) + 选择(Selection) + 视口交互(Gizmo/Picking) 为核心的专业级编辑器架构，对齐 Unity(EditorCore/GUI 分离) 与 UE(UnrealEd 模块) 的设计范式。
本文档是最终目标架构，不分阶段。所有模块一次性设计到位，给出可直接落地的接口定义、数据流、目录结构与迁移映射。
命名空间约定：编辑器就是 Cakery。所有编辑器侧代码（含框架层、面板、属性抽屉）统一置于 namespace cakery，不引入任何 dodoe::editor 之类的子命名空间。只有下沉到运行时的新增能力才属于 namespace dodoe。
适用引擎：dodoe runtime（DX12 / Deferred / DualThread）
UI 框架：Qt6 Widgets + qt-advanced-docking-system
文档版本：v1.1 · 2026-07-10

---
目录
1. 现状评估摘要
2. 设计原则
3. 目标分层架构
4. Framework 层详细设计
5. Property 抽屉体系
6. View 层重设计
7. Runtime 侧新增能力
8. 核心数据流
9. 目录与模块结构
10. 文件迁移映射表
11. CMake / 构建改动
12. 风险与测试策略
13. 编辑器布局系统
14. Scene View 与 Game View 双视口
15. 2D 瓦片地图编辑器 (Tilemap Editor)
16. 命令控制台 / 终端面板 + AI CLI + 类 Git 编辑历史

---
1. 现状评估摘要
当前 Cakery 约 4900 行、33 个文件。运行时(dodoe)已提供可复用基础：
能力
位置
状态
反射 TypeMeta/FieldAccessor
runtime/core/meta/reflection/reflection.h
✅ 可用，但无属性元数据
组件数据库 ComponentDB（逐组件 writeJson/readJson/add/remove/markDirty）
runtime/core/meta/component_db.h
✅ 可用
场景序列化 Scene::serialize()/deserialize()/save() → SceneRes
runtime/function/world/scene.h
✅ 可用
世界状态 WorldState{Simulation,Runtime,Pause} + 双系统集
runtime/function/world/world.h
✅ 可用
实体 Entity（带 Uuid/IDComponent）
runtime/function/world/entity.h
✅ 可用
事件系统 EventSystem
runtime/core/event/
✅ 可用
渲染视口 RenderViewport + host_handle 窗口嵌入
runtime/function/render/render_view/
✅ 可用
资源引用 PPtr = FileID + UUID + InstanceID
runtime/core/object/pptr.h
✅ 可用
结构性缺陷（本方案要彻底解决的）：
#
问题
影响
1
无命令层，widget 直改 ECS
无法 Undo/Redo（菜单项未接线）
2
无集中选择模型，选择隐式存于 Hierarchy tree
视口无法反映/发起选择，无多选
3
视口无拾取、无 Gizmo，工具栏按钮是装饰
与 Unity/UE 差距最大处
4
Play 模式不快照场景
Play 永久污染编辑场景
5
EngineManager 全局单例上帝对象
UI 与运行时深耦合，难测试
6
Inspector 是巨型 type-name 字符串 switch
不可扩展，反射无特性元数据
7
Inspector 全量重建、无双向绑定
运行时值变化不回流 UI
8
帧循环是 paintEvent 忙重绘
跑在 GUI 线程，无固定步长
9
New/Open/Save 菜单未接线
场景管理缺失
10
资源系统裸文件路径，未用 GUID
无资产数据库/缩略图
11
大量重复代码（Vector2/3/4 编辑器、拖拽解析三处复制）
维护成本

---
2. 设计原则
本架构的五条铁律，所有代码必须遵守：
1. View 永不直改 ECS。 任何对场景/实体/组件的修改，一律构造 ICommand 经 CommandStack::execute() 执行。这是 Undo/Redo、脏标记、协作编辑的前提。
2. 状态单向流动。 Framework 层持有权威状态（选择、文档、命令栈）；View 层订阅其变更信号并渲染，输入产生命令回流 Framework。禁止 View 之间横向直连。
3. 命令用 Uuid 引用实体，不用 entt::entity。 entt 句柄在实体销毁后会被复用，Undo(删除) 会拿到错乱的句柄。Entity 已带 IDComponent::id (Uuid)，命令必须存 Uuid，执行时解析回 Entity。
4. Framework 层零 Qt 依赖，但仍属 cakery 命名空间。 editor/framework/ 里的类不 #include <Q*>（只依赖 runtime 与标准库，可单元测试、可换 UI 框架），但命名空间就是 cakery —— 编辑器是一个整体。UI↔Framework 通过轻量观察者原语（cakery::Signal，非 Qt signal）通信。
5. 编辑器专属 runtime 代码用 DODOE_EDITOR 宏隔离，命名空间为 dodoe。 Picking / DebugDraw / 反射元数据等只在编辑器构建中编入，属于运行时能力，故在 namespace dodoe；发布 runtime 不含编辑器负担。

---
3. 目标分层架构
┌──────────────────────────────────────────────────────────────┐
│  View 层  (editor/Cakery/, namespace cakery) —— 只显示 + 采集输入 │
│                                                                │
│  EditorWindow (原 MainWindow, 停靠布局/菜单/工具栏)            │
│  ├─ Panel 基类                                                 │
│  ├─ ScenePanel         场景视图: 编辑相机 + 拾取 + Gizmo        │
│  ├─ GamePanel          游戏视图: 游戏相机 + 宽高比 + Play 主屏  │
│  ├─ HierarchyPanel     层级树: 读 Selection, 发 Command        │
│  ├─ InspectorPanel     检视器: PropertyDrawer 驱动 + 双向绑定   │
│  ├─ ProjectPanel       资源浏览器: AssetDatabase 驱动           │
│  └─ ConsolePanel       日志                                    │
└───────────────▲────────────────────────────┬──────────────────┘
   订阅变更(cakery::Signal) │                  │ 发命令 / 读状态
┌───────────────┴────────────────────────────▼──────────────────┐
│  Framework 层  (editor/framework/, namespace cakery, 零 Qt)     │
│                                                                │
│  EditorContext  ── 组合根, 持有并暴露下列服务:                  │
│   ├─ CommandStack       Undo/Redo 栈, 命令合并                  │
│   ├─ SelectionManager   选择集 + 变更广播                       │
│   ├─ SceneDocument      当前场景包装: 脏标记 / 存 / 取 / 新建    │
│   ├─ EditorCamera       3D 编辑相机(轨道/飞行), 实体化           │
│   ├─ GizmoService       变换 Gizmo 状态机 + 命令产出            │
│   ├─ PickingService     屏幕射线 → 实体                         │
│   ├─ PlayModeController  Play 前快照 / Stop 后还原              │
│   └─ EventBridge        runtime EventSystem ↔ cakery::Signal    │
└───────────────▲────────────────────────────┬──────────────────┘
        调用 runtime │                         │
┌───────────────┴────────────────────────────▼──────────────────┐
│  Runtime (dodoe, namespace dodoe)                             │
│  复用: Reflection/ComponentDB · Scene serialize · WorldState   │
│        EventSystem · RenderViewport · PPtr/FileID              │
│  新增(DODOE_EDITOR): 反射属性元数据 · PickingBackend            │
│        · DebugDraw 即时图元 · EditorCameraChannel              │
└────────────────────────────────────────────────────────────────┘
依赖方向严格自上而下： View → Framework → Runtime。Framework 绝不 include View 头；Runtime 绝不 include 任何 editor 头。View 与 Framework 同为 cakery 命名空间，但分属不同静态库以在构建期强制"Framework 不链 Qt"。

---
4. Framework 层详细设计
所有类置于 namespace cakery。头文件不含任何 <Q*>。
4.0 观察者原语（替代 Qt signal 做跨层通知）
Framework 层需要向 View 广播"选择变了/文档脏了"，但不能依赖 Qt。用极简的信号原语：
// editor/framework/core/Signal.h
#pragma once
#include <functional>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace cakery {

// 轻量多播信号。View 层在构造时 connect，析构时 disconnect。
template <typename... Args>
class Signal {
public:
    using Slot   = std::function<void(Args...)>;
    using Handle = std::uint64_t;

    Handle connect(Slot slot) {
        Handle h = ++m_next;
        m_slots.push_back({h, std::move(slot)});
        return h;
    }
    void disconnect(Handle h) {
        std::erase_if(m_slots, [h](const auto& e){ return e.handle == h; });
    }
    void emit(Args... args) const {
        auto snapshot = m_slots;          // 拷贝一份, 允许回调内 disconnect
        for (auto& e : snapshot) e.slot(args...);
    }
private:
    struct Entry { Handle handle; Slot slot; };
    std::vector<Entry> m_slots;
    Handle m_next{0};
};

// RAII 连接句柄: View widget 成员持有, 析构自动反订阅, 避免悬空回调
class ScopedConnection {
public:
    ScopedConnection() = default;
    template <typename S>
    ScopedConnection(S& sig, typename S::Handle h)
        : m_disconnect([&sig, h]{ sig.disconnect(h); }) {}
    ~ScopedConnection() { if (m_disconnect) m_disconnect(); }
    ScopedConnection(ScopedConnection&&) noexcept = default;
    ScopedConnection& operator=(ScopedConnection&&) noexcept = default;
    ScopedConnection(const ScopedConnection&) = delete;
private:
    std::function<void()> m_disconnect;
};

} // namespace cakery
4.1 EditorContext（组合根）
替代旧 EngineManager 单例。非单例，由 EditorApplication 构造一次并注入各面板。它持有引擎生命周期 + 所有编辑器服务。
// editor/framework/EditorContext.h
#pragma once
#include <memory>
#include <string>
#include "runtime/core/application.h"

namespace dodoe { class SystemContext; class World; class Scene; class RenderViewport; }

namespace cakery {

class CommandStack;
class SelectionManager;
class SceneDocument;
class EditorCamera;
class GizmoService;
class PickingService;
class PlayModeController;
class EventBridge;

struct EditorBootConfig {
    std::string projectPath;
    void*       hostWindowHandle = nullptr; // 视口宿主窗口 (ScenePanel 提供)
    int         width  = 1280;
    int         height = 720;
    float       devicePixelRatio = 1.0f;
};

class EditorContext {
public:
    EditorContext();
    ~EditorContext();
    EditorContext(const EditorContext&) = delete;
    EditorContext& operator=(const EditorContext&) = delete;

    // 生命周期
    bool boot(const EditorBootConfig& cfg);   // 初始化引擎 + 加载工程 + 建服务
    void shutdown();
    void tick(float deltaSeconds);            // 每帧: 引擎 tick + 相机/gizmo 更新
    void onViewportResized(int w, int h, float dpr);

    bool isBooted() const { return m_booted; }

    // runtime 访问器
    dodoe::SystemContext*  systemContext()  const;
    dodoe::World*          world()          const;
    dodoe::Scene*          activeScene()    const;
    dodoe::RenderViewport* renderViewport() const;

    // 编辑器服务 (引用语义, 生命周期同 context)
    CommandStack&        commands()   { return *m_commands; }
    SelectionManager&    selection()  { return *m_selection; }
    SceneDocument&       document()   { return *m_document; }
    EditorCamera&        camera()     { return *m_camera; }
    GizmoService&        gizmos()     { return *m_gizmos; }
    PickingService&      picking()    { return *m_picking; }
    PlayModeController&   playMode()   { return *m_playMode; }
    EventBridge&         events()     { return *m_events; }

private:
    bool m_booted = false;
    dodoe::ApplicationSpecification m_spec{};
    std::unique_ptr<dodoe::Application> m_app;
    dodoe::SystemContext* m_ctx = nullptr;

    std::unique_ptr<CommandStack>       m_commands;
    std::unique_ptr<SelectionManager>   m_selection;
    std::unique_ptr<SceneDocument>      m_document;
    std::unique_ptr<EditorCamera>       m_camera;
    std::unique_ptr<GizmoService>       m_gizmos;
    std::unique_ptr<PickingService>     m_picking;
    std::unique_ptr<PlayModeController> m_playMode;
    std::unique_ptr<EventBridge>        m_events;
};

} // namespace cakery
boot() 内部复刻旧 EngineManager::initialize 的时序（initializeModules → Project::Load → startRuntime → LayerStack::attach），随后构造各服务并互相接线（如 GizmoService 依赖 Selection + Commands）。
4.2 命令系统（CommandStack + ICommand）
这是整个重构的地基。 一切修改都是命令。
// editor/framework/command/ICommand.h
#pragma once
#include <string>

namespace cakery {

class EditorContext;

class ICommand {
public:
    virtual ~ICommand() = default;

    // 首次执行 (也可做初始化/捕获现场)。返回 false 表示无效, 不入栈。
    virtual bool execute(EditorContext& ctx) = 0;
    virtual void undo(EditorContext& ctx) = 0;
    virtual void redo(EditorContext& ctx) { execute(ctx); }

    // 用于 UI 显示 "Undo 移动实体"
    virtual std::string label() const = 0;

    // 命令合并: 连续拖拽 spinbox 应合并为一条。返回 true 表示已把 next 吸收进自己。
    virtual bool mergeWith(const ICommand& /*next*/) { return false; }
};

} // namespace cakery
// editor/framework/command/CommandStack.h
#pragma once
#include <memory>
#include <vector>
#include "ICommand.h"
#include "../core/Signal.h"

namespace cakery {

class EditorContext;

class CommandStack {
public:
    explicit CommandStack(EditorContext& ctx) : m_ctx(ctx) {}

    // 执行并入栈 (若 execute 返回 false 则丢弃)。会清空 redo 栈。
    void execute(std::unique_ptr<ICommand> cmd);

    bool canUndo() const { return !m_undo.empty(); }
    bool canRedo() const { return !m_redo.empty(); }
    void undo();
    void redo();
    void clear();

    // 合并窗口: 同一交互(如一次连续拖拽)期间 push 的同类命令自动合并。
    // beginMerge()/endMerge() 由 UI 在拖拽开始/结束时调用。
    void beginMerge() { m_merging = true; }
    void endMerge()   { m_merging = false; m_lastMergeable = nullptr; }

    std::string undoLabel() const;
    std::string redoLabel() const;

    Signal<> changed; // 栈变化 → UI 刷新 Undo/Redo 菜单可用态 + 标题脏标记

private:
    EditorContext& m_ctx;
    std::vector<std::unique_ptr<ICommand>> m_undo;
    std::vector<std::unique_ptr<ICommand>> m_redo;
    bool m_merging = false;
    ICommand* m_lastMergeable = nullptr;
};

} // namespace cakery
具体命令清单（editor/framework/command/commands/）——全部以 Uuid 引用实体：
命令
撤销策略
CreateEntityCommand
undo = 记录新实体 Uuid 并销毁；redo = 用同 Uuid 重建（Scene::createEntity(uuid, name)）
DeleteEntityCommand
execute 前先 序列化整棵子树到 JSON（利用 ComponentDB::writeJson + 层级）；undo = 反序列化重建
ReparentEntityCommand
存 旧父/新父 Uuid，undo 还原 HierarchyComponent
AddComponentCommand
undo = removeComponent
RemoveComponentCommand
execute 前 ComponentDB::writeJson 存组件 JSON；undo = addComponent + readJson
SetFieldValueCommand
存 组件名 + 字段名 + 旧值 + 新值(JSON)；支持 mergeWith（同实体同字段连续修改合并）
RenameEntityCommand
存旧名/新名
CompositeCommand
打包多条子命令（多选 Gizmo 拖拽一次撤销）
命令示例（字段修改，Inspector 与 Gizmo 都产出它）：
// editor/framework/command/commands/SetFieldValueCommand.h
#pragma once
#include "../ICommand.h"
#include "runtime/core/uuid.h"
#include "runtime/core/utils/json.h"
#include <string>

namespace cakery {

// 通用字段修改命令。值以 JSON 承载, 借用 ComponentDB 的读写能力。
class SetFieldValueCommand : public ICommand {
public:
    SetFieldValueCommand(dodoe::Uuid entity, std::string component,
                         std::string field, dodoe::Json oldVal, dodoe::Json newVal)
        : m_entity(entity), m_component(std::move(component)),
          m_field(std::move(field)), m_old(std::move(oldVal)), m_new(std::move(newVal)) {}

    bool execute(EditorContext& ctx) override; // 解析 entity → 写 field = m_new → markDirty
    void undo(EditorContext& ctx) override;     // 写 field = m_old → markDirty
    std::string label() const override { return "Modify " + m_component + "." + m_field; }

    bool mergeWith(const ICommand& next) override {
        auto* n = dynamic_cast<const SetFieldValueCommand*>(&next);
        if (!n || n->m_entity != m_entity || n->m_component != m_component
            || n->m_field != m_field) return false;
        m_new = n->m_new; // 只更新目标值, 保留最初 m_old
        return true;
    }
private:
    dodoe::Uuid m_entity;
    std::string m_component, m_field;
    dodoe::Json m_old, m_new;
};

} // namespace cakery
实体 Uuid 解析工具（editor/framework/core/UuidResolve.h）：dodoe::Entity resolve(dodoe::Scene*, dodoe::Uuid)（用 Scene::tryGetEntityByUUID）。所有命令 execute/undo 首先解析，实体不存在则安全跳过并记日志。
4.3 SelectionManager
// editor/framework/selection/SelectionManager.h
#pragma once
#include <vector>
#include "runtime/core/uuid.h"
#include "../core/Signal.h"

namespace cakery {

class SelectionManager {
public:
    // 以 Uuid 存储, 对场景重载/undo 稳健
    const std::vector<dodoe::Uuid>& selected() const { return m_selected; }
    bool isSelected(dodoe::Uuid id) const;
    bool empty() const { return m_selected.empty(); }
    dodoe::Uuid primary() const { return m_selected.empty() ? dodoe::Uuid{} : m_selected.front(); }

    void select(dodoe::Uuid id);                 // 单选(清空后选)
    void selectMany(std::vector<dodoe::Uuid> ids);
    void add(dodoe::Uuid id);                     // Ctrl+点击 累加
    void toggle(dodoe::Uuid id);
    void remove(dodoe::Uuid id);
    void clear();

    // 选择变化 → Hierarchy 高亮 / Inspector 重建 / 视口高亮 / Gizmo 更新
    Signal<const std::vector<dodoe::Uuid>&> changed;

private:
    std::vector<dodoe::Uuid> m_selected;
    void notify() { changed.emit(m_selected); }
};

} // namespace cakery
所有面板（Hierarchy 树、Inspector、SceneView 高亮、Gizmo）统一订阅 SelectionManager::changed，实现真正的多面板联动。
4.4 SceneDocument（文档模型 + 存取）
包装"当前正在编辑的场景"，管理脏标记与磁盘 IO，接线 New/Open/Save/SaveAs 菜单（解决问题 #9）。
// editor/framework/document/SceneDocument.h
#pragma once
#include <string>
#include <filesystem>
#include "../core/Signal.h"

namespace dodoe { class World; class Scene; }

namespace cakery {

class EditorContext;

class SceneDocument {
public:
    explicit SceneDocument(EditorContext& ctx) : m_ctx(ctx) {}

    void newScene(const std::string& name = "Untitled");
    bool openScene(const std::filesystem::path& file);   // deserialize → 设为 active
    bool save();                                          // 无路径则转 saveAs 语义
    bool saveAs(const std::filesystem::path& file);

    dodoe::Scene* scene() const;

    bool isDirty() const { return m_dirty; }
    void markDirty();      // 命令栈变化 / 值修改时置脏 → 标题栏 "*"
    void clearDirty();

    const std::filesystem::path& path() const { return m_path; }
    std::string displayTitle() const;        // "MyScene* — Cakery"

    Signal<>              dirtyChanged;
    Signal<dodoe::Scene*> sceneChanged;       // 场景切换 → 所有面板 refresh
private:
    EditorContext& m_ctx;
    std::filesystem::path m_path;
    bool m_dirty = false;
};

} // namespace cakery
落地时 save() 调 Scene::serialize() → 写盘（或直接 Scene::save()），openScene 用 Scene::deserialize(SceneRes)。CommandStack::changed 连到 markDirty。
4.5 EditorCamera（3D 编辑相机）
替换直写 MainCameraData channel 的旧 CameraController（那是纯 2D 平移缩放）。编辑相机应是完整 3D：轨道(Alt+LMB)、平移(MMB)、推拉(滚轮)、飞行(RMB+WASD)、聚焦选中(F)。
// editor/framework/camera/EditorCamera.h
#pragma once
#include "runtime/core/math/math.h"

namespace cakery {

class EditorCamera {
public:
    enum class Mode { Orbit, Fly };

    void setViewportSize(float w, float h);
    void update(float dt);              // 应用惯性/飞行位移
    void commitToRenderChannel();       // 写 view/proj 到编辑相机 channel

    // 输入 (由 ScenePanel 转发, 已归一到框架无 Qt 类型)
    void onMouseDown(float x, float y, int button, bool alt);
    void onMouseUp(int button);
    void onMouseMove(float x, float y);
    void onScroll(float delta);
    void onKey(int key, bool down);     // 飞行模式 WASDQE

    void focusOn(const dodoe::Vector3f& target, float radius); // F 键聚焦

    dodoe::Matrix4f view() const;
    dodoe::Matrix4f projection() const;
    // 供 Picking 用: 屏幕坐标 → 世界射线
    void screenToRay(float sx, float sy,
                     dodoe::Vector3f& outOrigin, dodoe::Vector3f& outDir) const;

private:
    Mode m_mode = Mode::Orbit;
    dodoe::Vector3f m_pivot{0,0,0};
    float m_distance = 10.f, m_yaw = 0.f, m_pitch = 0.f, m_fov = 60.f;
    float m_vpW = 1280, m_vpH = 720;
    // ...
};

} // namespace cakery
关键：编辑相机不应污染游戏相机。建议 runtime 提供独立的 EditorCameraChannel（DODOE_EDITOR 下），渲染系统在编辑视口用它、Play 时切回游戏相机。见 §7.4。
4.6 PickingService（视口拾取）
// editor/framework/picking/PickingService.h
#pragma once
#include "runtime/core/uuid.h"
#include "runtime/core/math/math.h"
#include <optional>
#include <vector>

namespace cakery {

class EditorContext;

class PickingService {
public:
    explicit PickingService(EditorContext& ctx) : m_ctx(ctx) {}

    // 点击拾取: 屏幕像素 → 命中实体 Uuid
    std::optional<dodoe::Uuid> pick(float screenX, float screenY);

    // 框选: 屏幕矩形 → 命中实体集合
    std::vector<dodoe::Uuid> pickRect(float x0, float y0, float x1, float y1);

private:
    EditorContext& m_ctx;
};

} // namespace cakery
后端两选一（见 §7.2）：
- CPU 射线 vs 包围盒（先落地，2D 场景用 AABB，3D 用 mesh bounds）——简单、无渲染改动。
- GPU Object-ID Buffer（终极方案）——渲染一遍 entity-id 到离屏 RT，读回像素。精确、支持像素级拾取，但需渲染管线加一个 pass。
方案推荐：先 CPU 包围盒，接口稳定后切 GPU id-buffer，PickingService 接口不变。
4.7 GizmoService（变换 Gizmo 状态机）
接管工具栏 Q(Pan)/W(Move)/E(Rotate)/R(Scale)/T(Rect)（解决问题 #3）。Gizmo 拖拽结束时产出 一条合并后的 SetFieldValueCommand（作用于 TransformComponent），天然可撤销。
// editor/framework/gizmo/GizmoService.h
#pragma once
#include "runtime/core/math/math.h"
#include "runtime/core/uuid.h"

namespace cakery {

class EditorContext;

enum class GizmoMode  { None, Translate, Rotate, Scale };
enum class GizmoSpace { World, Local };

class GizmoService {
public:
    explicit GizmoService(EditorContext& ctx) : m_ctx(ctx) {}

    void setMode(GizmoMode m)   { m_mode = m; }
    GizmoMode mode() const      { return m_mode; }
    void setSpace(GizmoSpace s) { m_space = s; }

    // 每帧: 依据当前选择计算 gizmo 位姿, 提交 DebugDraw 绘制 gizmo 图元
    void update();

    // 输入 (ScenePanel 转发)。返回 true 表示 gizmo 消费了此次输入(命中手柄)。
    bool onMouseDown(float x, float y);   // 命中手柄 → 进入拖拽, beginMerge()
    bool onMouseMove(float x, float y);   // 拖拽中 → 每帧 push SetFieldValueCommand(合并)
    void onMouseUp();                     // endMerge()

    bool isDragging() const { return m_dragging; }

private:
    EditorContext& m_ctx;
    GizmoMode  m_mode  = GizmoMode::Translate;
    GizmoSpace m_space = GizmoSpace::World;
    bool m_dragging = false;
    // 拖拽现场: 命中轴、起始位姿、拖拽平面...
};

} // namespace cakery
Gizmo 渲染依赖 runtime 的 DebugDraw（§7.3）。多选时 gizmo 作用于选择中心，命令对每个选中实体各产一条（打包进一个 CompositeCommand）。
4.8 PlayModeController（Play 前快照 / Stop 后还原）
解决问题 #4——Play 不再污染编辑场景。
// editor/framework/playmode/PlayModeController.h
#pragma once
#include <optional>
#include "runtime/function/world/scene.h" // SceneRes
#include "../core/Signal.h"

namespace cakery {

class EditorContext;

enum class PlayState { Edit, Playing, Paused };

class PlayModeController {
public:
    explicit PlayModeController(EditorContext& ctx) : m_ctx(ctx) {}

    void play();    // 快照 active scene → setState(Runtime)
    void pause();   // setState(Pause)
    void resume();  // setState(Runtime)
    void stop();    // setState(Simulation) → 还原快照

    PlayState state() const { return m_state; }

    Signal<PlayState> stateChanged; // 工具栏按钮态 / 面板只读态

private:
    EditorContext& m_ctx;
    PlayState m_state = PlayState::Edit;
    std::optional<dodoe::SceneRes> m_snapshot;   // 进入 Play 前的 serialize() 结果
};

} // namespace cakery
play()：m_snapshot = scene->serialize(); 然后 world->setState(Runtime)。
stop()：world->setState(Simulation); 然后 scene->deserialize(*m_snapshot); m_snapshot.reset(); 并清空命令栈（Play 期间的改动不应可撤销回编辑态）。
4.9 EventBridge（runtime 事件 ↔ cakery::Signal）
把 runtime EventSystem 的事件（实体增删、组件变更、场景加载）转成 cakery::Signal，供面板订阅，避免面板轮询。
// editor/framework/event/EventBridge.h
#pragma once
#include "runtime/core/uuid.h"
#include "../core/Signal.h"
#include <string>

namespace cakery {

class EditorContext;

class EventBridge {
public:
    explicit EventBridge(EditorContext& ctx);
    ~EventBridge();

    Signal<dodoe::Uuid> entityCreated;
    Signal<dodoe::Uuid> entityDestroyed;
    Signal<dodoe::Uuid> hierarchyChanged;                  // 层级变动 → Hierarchy 树刷新
    Signal<dodoe::Uuid, std::string> componentChanged;     // (entity, componentName) → Inspector 双向绑定

private:
    EditorContext& m_ctx;
    // 订阅 runtime EventSystem 的句柄, 析构时反订阅
};

} // namespace cakery
componentChanged 是解决问题 #7（双向绑定）的关键：markComponentDirty 时发此信号，Inspector 对应抽屉只刷新自己的控件值，不全量重建。

---
5. Property 抽屉体系
取代 GenericComponentEditor 的巨型字符串 switch（问题 #6、#11）。核心是一个按"字段类型/特性 → 抽屉工厂"的注册表，可扩展、可注册第三方类型抽屉。
抽屉是 View 层概念（要建 Qt 控件），因此放 editor/property/，允许依赖 Qt，命名空间仍为 cakery；产出的编辑动作一律走 CommandStack。
5.1 抽屉接口与注册表
// editor/property/PropertyDrawer.h
#pragma once
#include <QWidget>
#include <functional>
#include <memory>
#include <string>
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/uuid.h"

namespace cakery {

class EditorContext;

// 一个字段的绘制上下文
struct PropertyContext {
    EditorContext*    ctx;
    dodoe::Uuid       entity;         // 命令要用 Uuid
    std::string       componentName;
    void*             componentPtr;   // 当前值读取用 (只读, 写回走命令)
    dodoe::FieldAccessor field;
};

// 抽屉: 建控件 + 绑定"控件变化→SetFieldValueCommand", 并支持外部值刷新(双向绑定)
class PropertyDrawer {
public:
    virtual ~PropertyDrawer() = default;
    virtual QWidget* build(const PropertyContext& pc) = 0;
    // 组件被外部改动(如 Gizmo/Play)时, 只更新控件显示值, 不重建
    virtual void updateValue(const PropertyContext& pc) = 0;
};

// 注册表: 按类型名 / 特性匹配抽屉
class PropertyDrawerRegistry {
public:
    static PropertyDrawerRegistry& self();

    using Factory = std::function<std::unique_ptr<PropertyDrawer>()>;
    void registerByType(const std::string& typeName, Factory f);
    void registerByAttribute(const std::string& attrKey, Factory f); // 如 "Range"

    // 依字段类型 + 特性挑抽屉; 复合类型递归 (反射子字段)
    std::unique_ptr<PropertyDrawer> create(const dodoe::FieldAccessor& field);

private:
    // typeName → factory, attrKey → factory
};

} // namespace cakery
5.2 内置抽屉（editor/property/drawers/）
抽屉
处理类型
备注
ScalarDrawer
float/double/int/uint/bool
统一模板, 消除重复
StringDrawer
std::string

VectorDrawer<N>
Vector2f/3f/4f
模板化, 消除 createVector2/3/4Editor 重复(问题 #11)
ColorDrawer
Color
颜色按钮 + RGBA
EnumDrawer
反射枚举
从反射元数据读枚举值列表, 不再硬编码 CameraType(问题 #6)
AssetRefDrawer
PPtr<T>
显示资产名, 支持从 ProjectPanel 拖入, 存 FileID/UUID
CompositeDrawer
任意反射结构体
递归子字段, 兜底
ScalarDrawer 会读取字段特性 Range(min,max) 生成滑条、Tooltip 生成悬浮提示——依赖 §7.1 的反射元数据。
5.3 InspectorPanel 如何用它
onSelectionChanged(uuids):
    清空
    entity = resolve(primary)
    for each 组件 in ComponentDB.entries():
        if entity.has(组件):
            group = 折叠框(组件显示名, 可移除按钮→RemoveComponentCommand)
            meta  = TypeMeta::newMetaFromName(组件名)
            for each field in meta.get_field_list():
                if field 被 [Hidden] 标记: continue
                drawer = PropertyDrawerRegistry.create(field)
                记录 (field → drawer), 加入 group
    "Add Component" 按钮 → 菜单 → AddComponentCommand

onComponentChanged(entity, componentName):   # EventBridge 双向绑定
    for each drawer of that component:
        drawer->updateValue(...)              # 只刷新值, 不重建
多选时可选支持"共同组件的批量编辑"（进阶），初版可只编辑 primary。

---
6. View 层重设计
editor/Cakery/ 保留为纯视图。所有 widget 收到 EditorContext& 注入（构造参数），不再 getInstance()。命名空间仍是 cakery。
6.1 Panel 基类
// editor/Cakery/panels/Panel.h
#pragma once
#include <QWidget>
#include "framework/EditorContext.h"

namespace cakery {

class Panel : public QWidget {
    Q_OBJECT
public:
    explicit Panel(EditorContext& ctx, QWidget* parent=nullptr)
        : QWidget(parent), m_ctx(ctx) {}
protected:
    EditorContext& ctx() { return m_ctx; }
    EditorContext& m_ctx;
    // 子类在此存 ScopedConnection, 析构时统一反订阅
};

} // namespace cakery
6.2 各面板职责变化
面板
旧行为
新行为
EditorWindow(原 MainWindow)
直接 getInstance(), onPlay 直接 setState
持有 EditorContext; 菜单接 CommandStack(Undo/Redo)、SceneDocument(New/Open/Save)、PlayModeController(Play/Pause/Stop); 标题栏绑 document().dirtyChanged; 持 LayoutManager
ScenePanel(原 SceneWidget)
仅渲染 + 2D 相机; 拖拽直接 ImportAsset
编辑相机视口(渲染宿主); 鼠标事件转发 GizmoService(优先)→未命中则 PickingService→SelectionManager; 相机转发 EditorCamera; 框选; 拖拽资产 → CreateEntityCommand; 详见 §14
GamePanel(全新)
无
游戏相机视口; 宽高比/分辨率选择器 + 黑边; Play 时自动聚焦/可全屏; 统计浮层; 详见 §14
HierarchyPanel
tree current item 为选择源; 直接 create/destroy/reparent
订阅 SelectionManager::changed 高亮; 点击 → selection().select(); 增删/拖拽 → 对应 Command; 订阅 EventBridge::hierarchyChanged 刷新
InspectorPanel
巨型 switch, 全量重建
PropertyDrawer 驱动; 订阅 SelectionManager 重建、EventBridge::componentChanged 增量刷新; 增删组件 → Command
ProjectPanel
文件树, 拖裸路径
AssetDatabase 驱动; 拖拽传 GUID; 引擎缩略图(可选)
ConsolePanel
LogService 轮询
基本保留; LogService 可下沉/保留
TerminalPanel(全新)
无
命令行 REPL: 输入 debug 命令 / 作为 AI CLI; 经 CommandRegistry 派发 → 产出 ICommand; 与 Console 同停靠区 Tab; 详见 §16
6.3 渲染帧循环（解决问题 #8）
废弃 paintEvent 里 tick()+update() 忙重绘。改为 EditorWindow 持一个 QTimer：
// EditorWindow 构造
m_frameTimer = new QTimer(this);
m_frameTimer->setTimerType(Qt::PreciseTimer);
connect(m_frameTimer, &QTimer::timeout, this, [this]{
    float dt = m_clock.restart() / 1000.f;
    m_ctx.tick(dt);                          // 引擎 tick + 相机/gizmo update
    m_sceneView->requestViewportRepaint();   // 若需要
});
m_frameTimer->start(0); // 或按目标帧率; Play 时可提频, Edit 时可降频省电
ScenePanel 的 paintEvent 不再驱动 tick，只在需要时触发引擎呈现。进一步可把引擎 tick 放独立线程（runtime 已是 DualThread），Qt 侧只提交/同步——作为后续优化，接口不变。

---
7. Runtime 侧新增能力
以下在 runtime/function/editor_support/（或就近模块），命名空间 dodoe，全部 #ifdef DODOE_EDITOR 隔离。
7.1 反射属性元数据（支撑 §5 抽屉）
现状 FieldAccessor 只暴露 name + typeName。需在反射宏与生成代码里附带特性字典：
// 用法(在组件头, 反射解析器识别)
STRUCT(TransformComponent, Fields)
struct TransformComponent {
    META(Tooltip="世界坐标位置")
    Vector3f position;

    META(Range=(0,360), Tooltip="欧拉角(度)")
    Vector3f rotation;

    META(Hidden)
    Uuid internalId;
};
FieldAccessor 新增（namespace dodoe）：
// 追加到 reflection.h 的 FieldAccessor
bool        hasAttribute(const char* key) const;
const char* attribute(const char* key) const;      // 取字符串值
bool        attributeRange(float& min, float& max) const;
bool        isHidden() const;
bool        isReadOnly() const;
// 枚举字段: 取值名列表 (供 EnumDrawer)
int         enumValues(const char**& outNames, int*& outValues);
需要改动反射代码生成器（clang annotate 解析）以透传特性。若改生成器成本高，可退而求其次：编辑器侧维护一份"组件字段→特性"的外挂配置表（JSON），PropertyDrawerRegistry 查表。推荐前者，一劳永逸。
7.2 Picking 后端
// runtime/function/editor_support/picking_backend.h (DODOE_EDITOR)
namespace dodoe {
class PickingBackend {
public:
    // CPU 版: 射线 vs 场景内实体包围盒, 返回最近命中的 entt::entity
    static entt::entity RaycastNearest(Scene& scene,
                                       const Vector3f& origin, const Vector3f& dir);
    // GPU 版(终极): 需渲染 id-buffer, 读回 (screenX,screenY) 处 id
    static entt::entity ReadObjectIdBuffer(int screenX, int screenY);
};
}
先实现 CPU 版即可打通视口选中。GPU id-buffer 作为精度升级，接口对上层(cakery::PickingService)透明。
7.3 DebugDraw（Gizmo/高亮/网格 即时图元）
// runtime/function/editor_support/debug_draw.h (DODOE_EDITOR)
namespace dodoe {
class DebugDraw {
public:
    static void Line(const Vector3f& a, const Vector3f& b, const Color& c);
    static void Circle(const Vector3f& center, const Vector3f& normal, float r, const Color& c);
    static void Box(const Matrix4f& transform, const Color& c);
    static void Cone(...);                    // 平移 gizmo 箭头
    static void ScreenSpaceQuad(...);         // 缩放 gizmo 方块
    // 每帧渲染系统消费并清空
    static void Flush();
};
}
Gizmo、选中高亮(描边或包围盒)、编辑网格(grid)全部基于它。渲染系统在编辑视口的呈现末尾调 Flush()。
7.4 EditorCameraChannel
新增独立编辑相机数据通道，与游戏相机分离。编辑视口渲染用它；PlayModeController::play() 时渲染切游戏相机、stop() 切回。避免编辑相机污染游戏逻辑（当前 CameraController 直写 MainCameraData 是隐患）。
7.5 多视口渲染 API（支撑 Scene/Game 双视图，见 §14）
好消息：RenderSystem 已持有 DynamicArray<Scope<RenderViewport>> m_render_viewports 并暴露 getRenderViewports()；RenderViewport::buildViewFamily(scene, time, delta, view, proj) 已支持按传入的 view/proj 矩阵渲染；RenderViewport 还内建了 LetterboxMetrics/computeLetterboxMetrics（Game 视图黑边天然可用）。缺的只是公开的创建/销毁与逐视口相机绑定：
// runtime/function/render/render_system.h 追加 (可 DODOE_EDITOR)
namespace dodoe {
struct ViewportDesc {
    Window* window = nullptr;    // 宿主窗口 (Qt panel 的 winId 包成 Window)
    Vector2f logicalSize{1280, 720};
};
// 返回句柄/索引; 编辑器为 Scene / Game 各建一个
RenderViewport* RenderSystem::createViewport(const ViewportDesc& desc);
void            RenderSystem::destroyViewport(RenderViewport* vp);

// 逐视口指定相机来源: 渲染时用该 view/proj 调 buildViewFamily
// (view/proj 由上层每帧写入, 见 ViewportService)
}
renderFrame 需遍历 m_render_viewports，对每个视口用其当前绑定的 view/proj 各渲一遍（Scene 用编辑相机、Game 用激活的游戏相机）。
7.6 激活游戏相机查询（Game 视图取景）
Game 视图需要"场景中当前激活的游戏相机"的 view/proj。runtime 提供：
// 从场景里找主游戏相机 (Camera/Camera2D 组件, 带 primary/enabled 标记)
namespace dodoe {
bool World::getActiveGameCamera(Matrix4f& outView, Matrix4f& outProj,
                                Vector2f viewportSize) const;
}
无激活相机时 Game 视图显示 "No Cameras Rendering"（对齐 Unity 行为）。

---
8. 核心数据流
8.1 选择流（点击视口选中实体）
用户点击视口
  → ScenePanel::mousePressEvent
  → GizmoService::onMouseDown  (命中手柄? 否)
  → PickingService::pick(x,y) → Uuid
  → SelectionManager::select(uuid)
  → SelectionManager::changed.emit
      ├→ HierarchyPanel: 高亮对应树节点
      ├→ InspectorPanel: 重建属性面板
      ├→ ScenePanel: 绘制选中高亮(DebugDraw)
      └→ GizmoService: 更新 gizmo 位姿
8.2 编辑流（Inspector 改一个字段）
用户在 spinbox 输入
  → ScalarDrawer 的 valueChanged 回调
  → 构造 SetFieldValueCommand(uuid, comp, field, oldJson, newJson)
  → CommandStack::execute(cmd)
      ├→ cmd.execute: 写字段 + ComponentDB::markComponentDirty
      │     → EventBridge::componentChanged.emit
      │         → 其他面板/视口按需刷新
      ├→ 入 undo 栈, 清 redo 栈
      └→ CommandStack::changed.emit → SceneDocument::markDirty → 标题 "*"
8.3 撤销流
Ctrl+Z → EditorWindow → CommandStack::undo()
  → cmd.undo(ctx): 恢复旧值 + markDirty
  → 移入 redo 栈
  → changed.emit → 菜单可用态 + 各面板刷新
8.4 Gizmo 拖拽流（合并为单条命令）
onMouseDown 命中平移轴 → CommandStack::beginMerge()
onMouseMove(每帧) → push SetFieldValueCommand(TransformComponent.position ...)
                    → 因 beginMerge, 与上一条合并(mergeWith), 栈里始终只有一条
onMouseUp → CommandStack::endMerge()   → 一次拖拽 = 一次 Undo

---
9. 目录与模块结构
engine/src/editor/
├── framework/                      ← 新增: UI 无关编辑器核心 (静态库 EditorFramework, namespace cakery, 不链 Qt)
│   ├── EditorContext.{h,cpp}
│   ├── core/
│   │   ├── Signal.h                (含 ScopedConnection)
│   │   └── UuidResolve.{h,cpp}     ← Scene + Uuid → Entity
│   ├── command/
│   │   ├── ICommand.h
│   │   ├── CommandStack.{h,cpp}
│   │   ├── CompositeCommand.{h,cpp}
│   │   └── commands/
│   │       ├── CreateEntityCommand.{h,cpp}
│   │       ├── DeleteEntityCommand.{h,cpp}
│   │       ├── ReparentEntityCommand.{h,cpp}
│   │       ├── AddComponentCommand.{h,cpp}
│   │       ├── RemoveComponentCommand.{h,cpp}
│   │       ├── RenameEntityCommand.{h,cpp}
│   │       └── SetFieldValueCommand.{h,cpp}
│   ├── selection/SelectionManager.{h,cpp}
│   ├── document/SceneDocument.{h,cpp}
│   ├── camera/EditorCamera.{h,cpp}
│   ├── viewport/ViewportService.{h,cpp}   ← 新增: 多视口注册(Scene/Game), 每帧驱动各自相机渲染
│   ├── picking/PickingService.{h,cpp}
│   ├── gizmo/GizmoService.{h,cpp}
│   ├── playmode/PlayModeController.{h,cpp}
│   └── event/EventBridge.{h,cpp}
│
├── property/                       ← 新增: 属性抽屉 (静态库 EditorProperty, namespace cakery, 依赖 Qt + framework)
│   ├── PropertyDrawer.h
│   ├── PropertyDrawerRegistry.{h,cpp}
│   └── drawers/
│       ├── ScalarDrawer.{h,cpp}
│       ├── StringDrawer.{h,cpp}
│       ├── VectorDrawer.{h,cpp}
│       ├── ColorDrawer.{h,cpp}
│       ├── EnumDrawer.{h,cpp}
│       ├── AssetRefDrawer.{h,cpp}
│       └── CompositeDrawer.{h,cpp}
│
├── asset/                          ← 新增: 资产数据库 (namespace cakery)
│   └── AssetDatabase.{h,cpp}
│
└── Cakery/                         ← 现有: 瘦身为纯 Qt 视图 (namespace cakery)
    ├── main.cpp
    ├── app/
    │   ├── EditorApplication.{h,cpp}   (原 CakeryApplication, 构造 EditorContext 注入)
    │   ├── EditorWindow.{h,cpp}        (原 MainWindow)
    │   └── LayoutManager.{h,cpp}       ← 新增: ADS 布局保存/还原/预设 (见 §13)
    ├── panels/                         (原 widgets/)
    │   ├── Panel.h
    │   ├── ScenePanel.{h,cpp}
    │   ├── GamePanel.{h,cpp}           ← 新增: Game 视图 (见 §14)
    │   ├── HierarchyPanel.{h,cpp}
    │   ├── InspectorPanel.{h,cpp}
    │   ├── ProjectPanel.{h,cpp}
    │   └── ConsolePanel.{h,cpp}
    ├── project/                        (保留 ProjectManagerWindow/ProjectConfig)
    ├── services/LogService.{h,cpp}     (保留)
    ├── widgets/DragSpinBox.{h,cpp}     (保留, 通用控件)
    └── resources/style.qss

engine/src/runtime/function/editor_support/   ← 新增, namespace dodoe, DODOE_EDITOR 隔离
├── picking_backend.{h,cpp}
├── debug_draw.{h,cpp}
└── editor_camera_channel.{h,cpp}
# 另需在 render_system / world 追加: createViewport/destroyViewport (§7.5)、getActiveGameCamera (§7.6)

---
10. 文件迁移映射表
旧文件
去向
处理
services/EngineManager.*
framework/EditorContext.*
拆解: 引擎生命周期归 EditorContext, FPS 统计可留 View
services/CameraController.*
framework/camera/EditorCamera.*
重写为 3D 相机 + 独立 channel
app/CakeryApplication.*
app/EditorApplication.*
构造 EditorContext, 注入面板
app/MainWindow.*
app/EditorWindow.*
接线命令/文档/播放控制器, 加帧 Timer
widgets/SceneWidget.*
panels/ScenePanel.*
加拾取/gizmo/框选输入; 移除忙重绘
widgets/HierarchyWidget.*
panels/HierarchyPanel.*
改走 Selection + Command
widgets/InspectorWidget.*
panels/InspectorPanel.*
改走 PropertyDrawer + Command + 双向绑定
widgets/GenericComponentEditor.*
拆入 property/drawers/*
巨型 switch 拆成各抽屉
widgets/ComponentEditor.*
删除
被 PropertyDrawer 取代
widgets/ProjectBrowserWidget.*
panels/ProjectPanel.*
接 AssetDatabase
widgets/ConsoleWidget.*
panels/ConsolePanel.*
基本保留
widgets/DragSpinBox.*
保留
通用控件, 抽屉复用
services/LogService.*
保留

project/*
保留
工程选择流程不变

---
11. CMake / 构建改动
拆为三个目标，强制依赖方向（三者均为 namespace cakery，靠"是否链 Qt"在编译期约束 Framework 的纯净）：
# editor/framework/CMakeLists.txt —— 无 Qt 依赖
add_library(EditorFramework STATIC ${FRAMEWORK_SOURCES})
target_link_libraries(EditorFramework PUBLIC DodoeRuntime)
target_compile_definitions(EditorFramework PUBLIC DODOE_EDITOR)
# 注意: 不 link Qt

# editor/property + editor/asset —— 依赖 Qt + framework
add_library(EditorProperty STATIC ${PROPERTY_SOURCES})
target_link_libraries(EditorProperty PUBLIC EditorFramework Qt6::Widgets)
set_target_properties(EditorProperty PROPERTIES AUTOMOC ON)

# editor/Cakery —— 可执行, link 全部
qt_add_executable(Cakery ${CAKERY_SOURCES})
target_link_libraries(Cakery
    PRIVATE EditorFramework EditorProperty
    PRIVATE Qt6::Widgets qtadvanceddocking-qt6)
set_target_properties(Cakery PROPERTIES AUTOMOC ON WIN32_EXECUTABLE ON)
- DodoeRuntime 需要在编辑器构建时开 DODOE_EDITOR 以编入 editor_support/。若 runtime 是共享库且发布版不含编辑器，考虑把 editor_support 作为独立静态库 DodoeEditorSupport，仅编辑器 link。
- 反射代码生成器若改动，需保证 META(...) 特性进入生成的 FieldFuncTuple/元数据表。

---
12. 风险与测试策略
风险点
风险
缓解
反射生成器改动成本高
退路: 编辑器侧外挂特性配置表, 接口不变
GPU id-buffer 拾取需动渲染管线
先上 CPU 包围盒版打通交互, 后替换
命令的序列化撤销(Delete 整棵子树) 依赖组件 JSON 完整性
复用已验证的 Scene::serialize, 对齐其组件覆盖面; 加往返测试
编辑相机 channel 与游戏相机切换时机
PlayModeController 统一管理 state 切换
Framework 层误引 Qt
CMake 不 link Qt 即编译期拦截
Signal 悬空回调
View 用 ScopedConnection, 析构反订阅
测试
- Framework 层可单测（无 Qt）：
  - 命令往返：execute → undo → redo，断言场景状态一致（尤其 Delete/Reparent）。
  - 命令合并：连续 N 条 SetFieldValueCommand 在 merge 窗口内应只留 1 条，且 undo 一次回到起点。
  - 选择模型：select/add/toggle/clear + 信号触发次数。
  - Play 快照往返：play → 改场景 → stop，断言场景 == 快照。
  - Uuid 解析：实体销毁后命令安全跳过。
- 交互冒烟（手动/录制）：点击选中 → gizmo 拖拽 → Ctrl+Z 还原；Play → 改 → Stop 还原；New/Open/Save 往返。
验收标准（"像 Unity/UE"的最小达标线）
1. 视口点击选中实体，Hierarchy/Inspector 同步高亮。
2. W/E/R 切换平移/旋转/缩放 Gizmo，拖拽实时改变 Transform。
3. 任意修改 Ctrl+Z 撤销 / Ctrl+Y 重做，标题栏脏标记正确。
4. Play 后修改场景，Stop 完整还原，无污染。
5. 新增一个组件类型，无需改 Inspector 代码即可正确显示（PropertyDrawer + 反射元数据自动生效）。
6. New/Open/Save/SaveAs 场景可用。

---
13. 编辑器布局系统
对齐 Unity/UE：可停靠、可拖拽重组、可保存/切换的多布局编辑器界面。底层复用现有 qt-advanced-docking-system(ADS)。
13.1 默认布局（Default Layout）
启动即呈现的经典排布（对齐 Unity 默认 + UE 关卡编辑器）：
┌──────────────────────────────────────────────────────────────────────┐
│  菜单栏  File Edit Assets GameObject Component Window Help              │
├──────────────────────────────────────────────────────────────────────┤
│  播放工具栏         [▶ Play] [⏸ Pause] [⏹ Step]     [布局▼] [Layers▼]  │
├────────────┬────────────────────────────────────┬─────────────────────┤
│            │  ┌ Scene │ Game ┐  (Tab 切换/可拆分) │                     │
│ Hierarchy  │  │  工具条: Q W E R T | Shading | 2D │      Inspector      │
│  (层级树)   │  │                                   │      (检视器)        │
│            │  │        视口渲染区                  │                     │
│            │  │  (ScenePanel / GamePanel)          │  组件抽屉...         │
│            │  │                                   │                     │
├────────────┴──┴───────────────────────────────────┴─────────────────────┤
│  Project (资源浏览器)             │  Console (日志)                       │
└──────────────────────────────────────────────────────────────────────┘
- 中央区：ScenePanel 与 GamePanel 默认作为同一停靠区的两个 Tab（Unity 风格），用户可把 Game 拆出并排（UE 风格）。
- 左：Hierarchy。右：Inspector。底：Project 与 Console 并排 Tab。
- 中央区设为 ADS setCentralWidget，四周面板环绕（现有 MainWindow 已是此结构，扩展即可）。
13.2 LayoutManager
// editor/Cakery/app/LayoutManager.h
#pragma once
#include <QString>
#include <QStringList>

namespace ads { class CDockManager; }

namespace cakery {

// 封装 ADS 的 perspective(布局快照) + 磁盘持久化
class LayoutManager {
public:
    explicit LayoutManager(ads::CDockManager* dm) : m_dm(dm) {}

    void applyDefault();                       // 首次启动 / Reset 时构建默认布局
    void saveNamed(const QString& name);       // 存为命名布局(perspective)
    void loadNamed(const QString& name);
    void deleteNamed(const QString& name);
    QStringList namedLayouts() const;

    // 会话持久化: 退出存当前布局, 启动还原 (QSettings: 组织/应用名已在 EditorApplication 设置)
    void restoreSession();
    void saveSession();

    // 内置预设 (对齐 Unity: Default / Tall / Wide / 2by3)
    void applyPreset(const QString& preset);   // "Default"|"Tall"|"Wide"|"2by3"

private:
    ads::CDockManager* m_dm;
};

} // namespace cakery
- 持久化：ADS 提供 CDockManager::saveState()/restoreState()(返回/接收 QByteArray) 与 addPerspective(name)/openPerspective(name)/perspectiveNames()。命名布局用 perspective；会话布局用 QSettings 存 saveState() 的字节流。
- Window 菜单 挂：各面板 toggleViewAction()（现已有）、Layouts ▸（预设 + 已存布局 + "Save Layout…" + "Reset to Default"）。
- 播放工具栏加"布局下拉"（对齐 Unity 右上角 Layout 选择器）。
13.3 Dock 配置与样式
沿用现 MainWindow 的 ADS 配置（OpaqueSplitterResize、FocusHighlighting、DisableStylesheet），主题继续走 style.qss。新增建议：
- CDockManager::setConfigFlag(EqualSplitOnInsertion, true) 让首次插入均分。
- 每个 CDockWidget 设唯一 objectName（perspective 还原依赖它）。
- 面板图标：给 Hierarchy/Inspector/Project/Console/Scene/Game 设 tab 图标，观感对齐 Unity。

---
14. Scene View 与 Game View 双视口
这是本次追加的核心诉求：像 Unity 那样，Scene 视图（编辑者视角，自由飞行 + Gizmo）与 Game 视图（玩家视角，游戏相机取景）并存。
14.1 概念区分

ScenePanel（场景视图）
GamePanel（游戏视图）
相机
EditorCamera（编辑相机，轨道/飞行）
场景中激活的游戏相机（Camera/Camera2D 组件）
Gizmo/网格/高亮
✅ 显示
❌ 不显示（所见即玩家所见）
拾取/选择交互
✅ 点击选中、框选
❌ 只读呈现（Play 时接收游戏输入）
宽高比
跟随面板尺寸
可选 Free / 16:9 / 9:16 / 自定义分辨率 + 黑边
Edit 模式
一直可交互编辑
显示游戏相机预览（无相机则提示 "No Cameras Rendering"）
Play 模式
保持可自由观察
自动聚焦、可"Maximize on Play"、接收输入、显示统计
14.2 ViewportService（框架层多视口管理）
Framework 新增，统一管理"每个视口用哪个相机、绑哪个 RenderViewport、每帧写 view/proj"。
// editor/framework/viewport/ViewportService.h
#pragma once
#include "runtime/core/math/math.h"

namespace dodoe { class RenderViewport; class Window; }

namespace cakery {

class EditorContext;

enum class ViewportKind { Scene, Game };

// 一个编辑器视口: 宿主窗口 + 后端 RenderViewport + 相机来源
struct EditorViewport {
    ViewportKind          kind;
    dodoe::Window*        window   = nullptr;   // 由 Qt panel 的 winId 包成
    dodoe::RenderViewport* backend = nullptr;   // RenderSystem::createViewport 得到
    // Game 视图专用
    float aspect = 0.f;                         // 0 = Free; 否则强制宽高比
};

class ViewportService {
public:
    explicit ViewportService(EditorContext& ctx) : m_ctx(ctx) {}

    // panel 显示时注册 (传 winId + 初始尺寸), 关闭时注销
    EditorViewport* registerViewport(ViewportKind kind, void* hostHandle, int w, int h, float dpr);
    void unregisterViewport(EditorViewport* vp);
    void onResized(EditorViewport* vp, int w, int h, float dpr);

    // 每帧 (EditorContext::tick 内调用): 为每个视口计算并写入 view/proj
    //  Scene → EditorCamera.view/proj
    //  Game  → World::getActiveGameCamera(); 依 aspect 做 letterbox
    void updateAndRenderAll(float dt);

    void setGameAspect(EditorViewport* vp, float aspect); // 0=Free

private:
    EditorContext& m_ctx;
    // std::vector<std::unique_ptr<EditorViewport>> m_viewports;
};

} // namespace cakery
依赖 §7.5 的 RenderSystem::createViewport/destroyViewport 与 §7.6 的 World::getActiveGameCamera。ScenePanel 一直有内容；GamePanel 在 Edit 期也预览游戏相机（Unity 行为）。
14.3 GamePanel（游戏视图）
// editor/Cakery/panels/GamePanel.h
#pragma once
#include "Panel.h"

namespace cakery {

class GamePanel : public Panel {
    Q_OBJECT
public:
    explicit GamePanel(EditorContext& ctx, QWidget* parent=nullptr);
    ~GamePanel() override;

protected:
    void showEvent(QShowEvent*) override;     // 首次: registerViewport(Game, winId)
    void resizeEvent(QResizeEvent*) override; // → ViewportService::onResized
    void paintEvent(QPaintEvent*) override;   // 仅呈现, 不驱动 tick (帧循环见 §6.3)

private:
    void buildToolbar();                      // 宽高比下拉 / Maximize / Stats 开关
    void onAspectChanged(int index);          // Free / 16:9 / 9:16 / 1:1 / 自定义

    EditorViewport* m_vp = nullptr;
    // 工具条: QComboBox 宽高比, QToolButton "Maximize On Play", "Stats"
    // 无游戏相机时叠加一个居中 QLabel "Display 1 - No Cameras Rendering"
};

} // namespace cakery
Game 视图特性（对齐 Unity）：
1. 宽高比/分辨率选择器：Free Aspect / 16:9 / 9:16 / 1:1 / 自定义分辨率。非 Free 时用 RenderViewport 已有的 computeLetterboxMetrics 在面板内居中留黑边。
2. Maximize On Play：勾选后 PlayModeController::play() 时把 GamePanel 临时最大化（ADS 全屏该 dock），stop() 还原布局。
3. 统计浮层（Stats）：右上角叠加 FPS/DrawCall/三角形数（数据取自 RenderSystem/统计通道，可后接）。
4. 无相机提示：getActiveGameCamera 返回 false 时显示 "No Cameras Rendering"。
5. 输入路由：Edit 模式 GamePanel 不接收编辑输入；Play 模式把键鼠事件转发给游戏输入系统（经 runtime 输入通道）。
14.4 与 Play 模式协同
PlayModeController::play()
  → 快照场景 (§4.8)
  → world.setState(Runtime)
  → stateChanged.emit(Playing)
       ├→ GamePanel: 若 "Maximize On Play" → 最大化本面板; 开始接收游戏输入
       ├→ EditorWindow: 若配置"Play 时聚焦 Game" → 激活 Game Tab
       └→ ScenePanel: 保持可编辑观察 (Gizmo 仍可用, 但改动计入 Play 期, Stop 丢弃)

PlayModeController::stop()
  → world.setState(Simulation) → 还原快照
  → stateChanged.emit(Edit)
       ├→ GamePanel: 还原布局; 停止接收输入
       └→ 各面板刷新
14.5 双视口每帧渲染流
EditorWindow QTimer 触发 (§6.3)
  → EditorContext::tick(dt)
      ├→ EditorCamera::update(dt)
      ├→ GizmoService::update()               (仅作用于 Scene 视图图元)
      └→ ViewportService::updateAndRenderAll(dt)
            for vp in viewports:
              if vp.kind == Scene:
                 view,proj = EditorCamera.view/proj
                 (提交 DebugDraw: grid + 选中高亮 + gizmo)
              else /* Game */:
                 if World::getActiveGameCamera(view, proj, size): 依 aspect letterbox
                 else: 标记"无相机" → 面板显示提示, 跳过
              vp.backend.buildViewFamily(scene, t, dt, view, proj)  // §7.5
      → RenderSystem::renderFrame 遍历 m_render_viewports 各渲一遍
每个 Qt 视口面板都是 WA_NativeWindow（现 SceneWidget 已设），各自 winId() 作为独立 Window* 建后端视口。DX12 下即两个 swapchain/呈现表面，互不干扰。
14.6 运行时前置依赖小结（见 §7.5 / §7.6）
能力
现状
需补
多 RenderViewport 容器
✅ 已有 m_render_viewports + getRenderViewports()
公开 createViewport/destroyViewport
逐视口 view/proj 渲染
✅ buildViewFamily(...view, proj) 已支持
renderFrame 遍历各视口用其绑定相机
宽高比黑边
✅ LetterboxMetrics/computeLetterboxMetrics 已有
暴露给 GamePanel 用
激活游戏相机
❌
World::getActiveGameCamera(view,proj,size)
编辑相机独立通道
❌
EditorCameraChannel（§7.4）

---
15. 2D 瓦片地图编辑器 (Tilemap Editor)
目标：在编辑器内提供 Tiled / Unity Tilemap 式的瓦片绘制体验——瓦片调色板、笔刷/擦除/填充/矩形/吸管工具、多图层、网格吸附、可撤销的逐格编辑。架在前述 CommandStack / ViewportService / ScenePanel 之上，不另起炉灶。
15.1 现有基石评估（复用 vs 补齐）
运行时的数据模型 + 渲染 + Tiled 导入 + 脚本桥已就绪，仅缺编辑器交互层与少量运行时小 helper。
层
现状
备注
TilemapComponent(map/tile 尺寸, tilesets, findTilesetByGid)
✅ 已有, 反射+注册(不可 Add)
components/tilemap/tilemap_component.h
TileLayerComponent(DynamicArray<UInt32> tiles 扁平 gid 网格 + visible/opacity/offset)
✅ 已有, 反射+注册(可 Add)
每层一个子实体
TilesetAsset(first_gid/columns/tile_count/image_path/texture_id)
✅ 已有

TilemapRendererSystem(逐格 diff→SpriteRenderObject, gid→UV)
✅ 完整可用
编辑时置 tilemap.dirty=true 触发 resync
Tiled 导入(C# TiledImporter + Tilemap.InstantiateToScene)
✅ 可用
测试工程有真实 .tsj
单格 set/get tile
❌
只有整数组 Native_TileLayerSetData；需补 §15.2
屏幕↔格 坐标换算
❌
需补 §15.2
调色板 / 工具 / 网格 / 激活图层
❌
编辑器新增 §15.3–15.6
15.2 运行时需补的小能力
给 TileLayerComponent 加逐格读写 + 尺寸重建（namespace dodoe，非编辑器专属，故不加 DODOE_EDITOR）：
// 追加到 tile_layer_component.h
struct TileLayerComponent {
    // ... 现有字段 ...

    [[nodiscard]] UInt32 getTile(Int32 x, Int32 y) const {
        if (x < 0 || y < 0 || x >= (Int32)layer_width || y >= (Int32)layer_height) return 0;
        return tiles[(Size_t)y * layer_width + x];
    }
    // 返回是否真的改变(供命令判空 / 合并去重)
    bool setTile(Int32 x, Int32 y, UInt32 gid) {
        if (x < 0 || y < 0 || x >= (Int32)layer_width || y >= (Int32)layer_height) return false;
        Size_t i = (Size_t)y * layer_width + x;
        if (tiles[i] == gid) return false;
        tiles[i] = gid;
        return true;
    }
    void resize(UInt32 w, UInt32 h) {                 // 新建/扩图, 保内容
        DynamicArray<UInt32> next(w * h, 0);
        UInt32 cw = std::min(w, layer_width), ch = std::min(h, layer_height);
        for (UInt32 yy = 0; yy < ch; ++yy)
            for (UInt32 xx = 0; xx < cw; ++xx)
                next[yy * w + xx] = tiles[yy * layer_width + xx];
        tiles = std::move(next); layer_width = w; layer_height = h;
    }
};
编辑器侧 TileCoord 换算（namespace cakery，editor/framework/tilemap/TileCoord.h）——把 Scene 视口的世界坐标映射到瓦片格：
namespace cakery {
struct TileCoord {
    // 世界坐标(编辑相机反投影得到) → 格坐标; 依 tilemap 的 tile_width/height + 实体 transform
    static bool worldToCell(const dodoe::TilemapComponent& tm, const dodoe::Matrix4f& mapWorld,
                            const dodoe::Vector3f& worldPos, int& outX, int& outY);
    static dodoe::Vector3f cellToWorld(const dodoe::TilemapComponent& tm, const dodoe::Matrix4f& mapWorld,
                                       int x, int y); // 格中心/左上, 供网格与高亮
};
}
屏幕→世界用 §4.5 EditorCamera::screenToRay（2D 正交时取 z=0 平面交点）。EditorCamera 需支持 Ortho2D 模式（当前 CameraController 已是正交，迁移即可）。
15.3 TilePaintService（框架层瓦片绘制状态机）
Framework 新增，持有"当前 Tilemap / 激活图层 / 当前笔刷 / 工具模式"，把视口输入翻译成瓦片编辑命令。
// editor/framework/tilemap/TilePaintService.h
#pragma once
#include <vector>
#include "runtime/core/uuid.h"

namespace cakery {

class EditorContext;

enum class TileTool { Brush, Erase, Fill, Rect, Picker, Line };

// 画笔: 一块 gid 矩形(从调色板框选而来), 支持多格图章
struct TileBrush {
    int w = 1, h = 1;
    std::vector<uint32_t> gids{0}; // w*h, 行主序; 全 0 视为擦除
    bool empty() const { for (auto g : gids) if (g) return false; return true; }
};

class TilePaintService {
public:
    explicit TilePaintService(EditorContext& ctx) : m_ctx(ctx) {}

    // 上下文
    void setActiveTilemap(dodoe::Uuid tilemapEntity);   // 选中含 TilemapComponent 的实体时设置
    void setActiveLayer(dodoe::Uuid layerEntity);       // Tilemap 面板选层
    dodoe::Uuid activeTilemap() const { return m_tilemap; }
    dodoe::Uuid activeLayer()  const { return m_layer; }

    void setTool(TileTool t)        { m_tool = t; }
    TileTool tool() const           { return m_tool; }
    void setBrush(TileBrush b)      { m_brush = std::move(b); }
    const TileBrush& brush() const  { return m_brush; }

    // 视口交互 (ScenePanel 在 Tilemap 编辑模式下转发; 已换算成格坐标)
    void onCellDown(int cx, int cy);   // 起笔: CommandStack::beginMerge()
    void onCellDrag(int cx, int cy);   // 拖动: 按工具累积编辑 → PaintTilesCommand(合并)
    void onCellUp();                   // 收笔: endMerge()

    bool hasTarget() const;            // tilemap + layer 均有效
private:
    EditorContext& m_ctx;
    dodoe::Uuid m_tilemap, m_layer;
    TileTool  m_tool = TileTool::Brush;
    TileBrush m_brush;
};

} // namespace cakery
各工具语义（对齐 Tiled）：Brush 图章贴 gid；Erase 置 0；Fill 对同 gid 连通域洪水填充；Rect 拉框矩形填；Picker 吸管，把点中格的 gid（或矩形）设为当前笔刷；Line 两点连线（Bresenham）。
15.4 命令：PaintTilesCommand（一笔一撤销）
复用 §4.2 命令栈。一次连续涂抹（down→drag…→up）经 beginMerge/endMerge 合并成一条命令，记录被改动格的 before/after。
// editor/framework/command/commands/PaintTilesCommand.h
#pragma once
#include "../ICommand.h"
#include "runtime/core/uuid.h"
#include <vector>

namespace cakery {

class PaintTilesCommand : public ICommand {
public:
    struct Cell { int x, y; uint32_t before, after; };

    PaintTilesCommand(dodoe::Uuid tilemapEntity, dodoe::Uuid layerEntity)
        : m_tilemap(tilemapEntity), m_layer(layerEntity) {}

    // 累积一格改动(去重: 同格只留首个 before + 最新 after)
    void addCell(int x, int y, uint32_t before, uint32_t after);
    bool empty() const { return m_cells.empty(); }

    bool execute(EditorContext& ctx) override; // 写 after, 置 tilemap.dirty
    void undo(EditorContext& ctx) override;     // 写 before, 置 tilemap.dirty
    std::string label() const override { return "Paint Tiles"; }

    // 同 layer 的连续笔画合并 (同一 stroke 内)
    bool mergeWith(const ICommand& next) override;

private:
    dodoe::Uuid m_tilemap, m_layer;
    std::vector<Cell> m_cells;
    // 加速去重: map<(x,y)->索引>
};

} // namespace cakery
- execute/undo 解析 layer 实体 → TileLayerComponent::setTile，末尾对 tilemap 实体 TilemapComponent::dirty=true（触发 §15.1 渲染 resync）。
- Fill/Rect 生成的一批格同样进一条 PaintTilesCommand（Rect 不需要 merge，Brush 拖拽需要）。
- 遵守铁律：ScenePanel 不直接改 tiles，一律经此命令。
15.5 View：TilePalettePanel + Tilemap 工具
TilePalettePanel（新面板，editor/Cakery/panels/TilePalettePanel） —— 对齐 Unity Tile Palette：
// 关键职责 (伪代码)
class TilePalettePanel : public Panel {
    // 顶部: 选择活动 Tileset (来自 activeTilemap 的 tilesets, 或从 Project 拖入图片新建 tileset)
    // 中部: 把 tileset 图按 tile_width/height 切成网格显示 (QGraphicsView 或自绘)
    //       单击选一格 → TileBrush{1x1, gid}; 拖拽框选 → 多格图章
    // 底部: 工具条 Brush/Erase/Fill/Rect/Picker/Line → TilePaintService::setTool
    // 选格后: ctx.tilePaint().setBrush(...)
};
ScenePanel 瓦片模式扩展（在 §14 的 ScenePanel 内按"当前是否编辑 Tilemap"分支）：
- 网格叠加：选中 Tilemap 时，用 §7.3 DebugDraw::Line 依 tile_width/height 画网格；鼠标所在格高亮（DebugDraw::Box）；笔刷图章预览（半透明）。
- 输入路由：Tilemap 编辑激活时，左键 down/drag/up → TileCoord::worldToCell → TilePaintService::onCellDown/Drag/Up；右键吸管快捷（Picker）。未激活时走 §14 常规拾取。
- 吸附：格对齐由 TileCoord 天然完成。
Tilemap 主菜单/工具：
- GameObject ▸ 2D ▸ Tilemap：新建 Tilemap 实体（TilemapComponent + 一个默认 TileLayerComponent 子实体）——经 CreateEntityCommand + AddComponentCommand。
- Assets ▸ Import ▸ Tiled Map…：调已有 C# TiledImporter（或等价 C++ 路径）导入 .tmj/.tsj → 建实体树；导入动作包装成一条可撤销命令。
15.6 Tilemap 图层面板（可选，或并入 Hierarchy）
Tilemap 的多图层即其子实体（各带 TileLayerComponent）。两种呈现：
- 轻量：直接用 Hierarchy 树展开 Tilemap 子节点，Inspector 编辑 TileLayerComponent（visible/opacity/offset 已反射，PropertyDrawer 自动出控件）；点选某层即 TilePaintService::setActiveLayer。
- 专业：单独 TileLayersPanel（对齐 Tiled 右侧 Layers），支持排序、可见性开关、锁定、透明度滑条、新增/删除层（均走命令）。初版取"轻量"即可。
15.7 数据流：一次涂抹（Brush 拖拽）
选中 Tilemap 实体 → TilePaintService.setActiveTilemap; Hierarchy 选层 → setActiveLayer
TilePalettePanel 选格 → setBrush(gid)
ScenePanel 左键按下(格 cx,cy)
  → TilePaintService::onCellDown → CommandStack::beginMerge()
      → 新建 PaintTilesCommand(tilemap, layer)
      → 按 brush 图章 addCell(x,y, before=getTile, after=gid)
      → CommandStack::execute(cmd)  // 写 after + tilemap.dirty=true
拖动经过新格(drag)
  → onCellDrag → 生成 PaintTilesCommand → execute
      → 因 beginMerge, mergeWith 上一条(同 layer) → 栈内仍一条, 累积 cells
松开
  → onCellUp → CommandStack::endMerge()      // 整笔 = 一次 Undo
渲染: TilemapRendererSystem 见 tilemap.dirty → syncTilemap 整图重提交 sprite
15.8 目录结构追加
editor/framework/tilemap/            ← 新增 (namespace cakery, 零 Qt)
├── TilePaintService.{h,cpp}
└── TileCoord.{h,cpp}
editor/framework/command/commands/
└── PaintTilesCommand.{h,cpp}        ← 追加
editor/Cakery/panels/
├── TilePalettePanel.{h,cpp}         ← 新增
└── TileLayersPanel.{h,cpp}          ← 可选

runtime 追加(namespace dodoe, 非编辑器专属):
└── tile_layer_component.h: getTile/setTile/resize   (§15.2)
   (可选) script_glue: Native_TileLayerSetTile 供脚本/热重载侧对齐
15.9 验收标准（瓦片编辑最小达标线）
1. 选中/新建 Tilemap，Scene 视口显示对齐网格 + 鼠标格高亮。
2. 从 Palette 选一格作画笔，在视口拖拽绘制，实时可见（sprite 立即刷新）。
3. Erase 擦除、Bucket Fill 洪水填充、Rect 矩形填、Picker 吸管 均可用。
4. 一次连续拖拽 = 一次 Ctrl+Z 完整撤销。
5. 多图层：切换激活层分别绘制；图层 visible/opacity 生效。
6. 导入一张真实 Tiled .tmj（用 DesktopFarm/OnlyOne 数据）→ 正确成图并可继续编辑。

---
16. 命令控制台 / 终端面板 + AI CLI + 类 Git 编辑历史
目标：加一个命令行终端面板，用于输入 debug 命令、并作为 AI 的 CLI 驱动编辑器；把 §4.2 的命令系统向文本/结构化指令扩展，让人和 AI 用同一套通道操作；并在其上做类 Git 的编辑历史，使 AI 的每轮改动可整体撤回。
16.1 核心洞察：三种"命令"统一为一条管线
概念
是什么
关系
ICommand(§4.2)
可撤销的 ECS 变更原子（CreateEntity、SetFieldValue…）
底层，一切修改的唯一执行体
Console 文本命令（本节新增）
文本动词 entity.create name=Foo、tile.fill
解析后产出一个或多个 ICommand，经 CommandStack 执行
工具栏/菜单动作
按钮点击
同样最终产出 ICommand
关键：终端不是新的改数据的旁路。它是命令系统的又一个前端——文本 → CommandRegistry 派发 → handler 构造 ICommand → CommandStack::execute。于是终端/AI 的每一次操作自动可撤销、可归属、可回滚，且严守"View 永不直改 ECS"铁律。
        人在 TerminalPanel 打字 ─┐
        AI 通过 AICommandBridge ─┼─→ CommandRegistry.dispatch(text|structured)
        工具栏/菜单             ─┘         │  handler 构造
                                            ▼
                                   CommandStack.execute(ICommand)   ← 唯一变更入口
                                            │
                                   EditHistory(事务/检查点)  ← 类 Git 回滚
16.2 CommandRegistry（命令注册与派发）
editor/framework/console/CommandRegistry.h（namespace cakery，零 Qt）。命令自描述，供 AI 内省。
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "runtime/core/utils/json.h"

namespace cakery {

class EditorContext;

// 解析后的实参: 位置参数 + key=value; 也可由 AI 直接传 Json
struct CommandArgs {
    std::vector<std::string> positional;
    dodoe::Json named;                    // {"name":"Foo","x":3}
    dodoe::Json raw;                      // AI 结构化调用时的原始载荷
};

struct CommandResult {
    bool ok = true;
    std::string message;                  // 人读输出
    dodoe::Json data;                     // 机读结果 (供 AI)
    static CommandResult Ok(std::string m={}, dodoe::Json d={})   { return {true,  std::move(m), std::move(d)}; }
    static CommandResult Err(std::string m)                       { return {false, std::move(m), {}}; }
};

struct ParamSpec { std::string name, type, help; bool required=false; };

struct CommandSpec {
    std::string name;                     // "entity.create"
    std::string summary;                  // 一行说明 (AI 可读)
    std::string usage;                    // "entity.create name=<str> [parent=<uuid>]"
    std::vector<ParamSpec> params;        // 供 AI 生成调用 / 自动补全
    bool mutating = true;                 // 是否改场景(→ 需事务/可撤销); false=只读查询
    std::function<CommandResult(EditorContext&, const CommandArgs&)> handler;
};

class CommandRegistry {
public:
    static CommandRegistry& self();

    void add(CommandSpec spec);
    bool has(const std::string& name) const;

    // 人类入口: 解析整行文本
    CommandResult execute(EditorContext& ctx, const std::string& line);
    // AI 入口: 结构化调用 (name + Json args), 免解析歧义
    CommandResult executeStructured(EditorContext& ctx, const std::string& name, const dodoe::Json& args);

    // 内省 (AI 用来发现能力 / 生成 tool schema)
    std::vector<CommandSpec> list() const;
    dodoe::Json toolSchema() const;       // 导出为 LLM function-calling schema
    std::string help(const std::string& name) const;

    // 干跑: 校验参数 + 预演, 不落地 (AI 先验证再执行)
    CommandResult dryRun(EditorContext& ctx, const std::string& name, const dodoe::Json& args);
};

// 注册宏/集中注册点: editor/framework/console/builtin_commands.cpp
// 例: entity.create / entity.delete / entity.select / component.add / component.set
//     scene.new|open|save / tile.brush|fill|erase / history.log|undo|redo|revert
} // namespace cakery
内置命令（示例，全部 mutating 的都产出 ICommand）：
命令
作用
产出
entity.create name=<s> [parent=<uuid>]
建实体
CreateEntityCommand
entity.delete <uuid>
删实体
DeleteEntityCommand
entity.select <uuid...>
选择（只读于场景，写 Selection）
SelectionManager
component.add <uuid> <Type>
加组件
AddComponentCommand
component.set <uuid> <Type>.<field> <value>
改字段
SetFieldValueCommand
tile.fill <layer> rect=x,y,w,h gid=<n>
填瓦片
PaintTilesCommand
scene.save / scene.open <path>
场景 IO
经 SceneDocument
query.entities [filter]
列实体（只读）
Json 结果给 AI
history.log / undo / redo / revert <checkpoint>
历史操作
见 §16.6
16.3 命令 → ICommand 桥（复用 §4.2，零特例）
handler 内部一律：解析实参 → 构造 ICommand（或 CompositeCommand）→ ctx.commands().execute(...)。多步命令用 CompositeCommand 打包成一次撤销。只读命令（query.*）不产出 ICommand，直接返回 Json。
这样：终端敲 component.set 与 Inspector 拖 spinbox 落到同一条命令类，历史、脏标记、双向绑定全部自动一致。
16.4 TerminalPanel（终端面板 UI）
editor/Cakery/panels/TerminalPanel（namespace cakery）。与 ConsolePanel(日志) 同停靠区并列 Tab（对齐 Unity/UE 把 Console 与命令行放一起）。
class TerminalPanel : public Panel {
    Q_OBJECT
public:
    explicit TerminalPanel(EditorContext& ctx, QWidget* parent=nullptr);
private:
    void submit(const QString& line);     // → CommandRegistry::execute → 回显结果
    void appendEntry(...);                // 输出区: 命令回显 + 结果/错误, 按来源着色
    // 输入行: QLineEdit + 历史(↑↓) + Tab 自动补全(读 CommandRegistry::list)
    // 来源标记: [user] 蓝 / [ai] 紫 / [system] 灰; 错误红
    // 顶部工具条: Clear / 过滤(only errors) / "AI" 指示灯(AI 会话活动时亮)
    QListWidget* m_output = nullptr;
    QLineEdit*   m_input  = nullptr;
    QStringList  m_history;
};
要点：
- 补全/提示：从 CommandRegistry 拉命令名与 ParamSpec 做 Tab 补全与浮动用法提示。
- 输出复用：结果同时 echo 到 LogService，Console 也能看到（统一日志流）。
- 来源着色：人输入 vs AI 下发 vs 系统回显 分色，一眼区分 AI 干了什么。
- 只读安全：query.* 命令即使 AI 高频调用也不进历史、不置脏。
16.5 AI 接入（AICommandBridge）
editor/framework/console/AICommandBridge。把 CommandRegistry 暴露给 AI，传输层可插拔（进程内嵌 / 本地 stdio / socket / MCP 皆可，接口不变）。
class AICommandBridge {
public:
    explicit AICommandBridge(EditorContext& ctx) : m_ctx(ctx) {}

    // AI 会话: 一次会话 = 一个可整体回滚的事务区(见 §16.6)
    std::string beginSession(const std::string& goal);   // 开事务 + 自动检查点
    void        endSession(bool keep);                    // keep=false → revert 到会话前

    // 三个给 AI 的原语 (结构化 JSON, 便于 LLM function-calling)
    dodoe::Json listCommands();                           // = CommandRegistry::toolSchema()
    dodoe::Json execute(const std::string& name, const dodoe::Json& args); // 记 author="ai"
    dodoe::Json query(const std::string& name, const dodoe::Json& args);   // 只读

    // 安全阀
    void setAllowlist(std::vector<std::string> names);    // 限制 AI 可用命令
    void setConfirmRequired(bool on);                     // 危险命令(delete/save)需人确认
private:
    EditorContext& m_ctx;
};
给 LLM 的心智模型（AI 作为 tool 使用）：
1. listCommands() → 得到自描述 schema（命令名 + 参数 + 只读/写标记）。
2. beginSession("把地面铺上草") → 开事务 + 打检查点。
3. 反复 query.* 观察 + execute(...) 修改（每条自动可撤销、标记 author=ai）。
4. endSession(keep=true/false)：满意则保留（合并为一个"commit"），不满意 keep=false 一键回到会话前（= git reset --hard 到检查点）。
传输建议默认：本地 stdio/JSON-RPC，让外部 AI 代理（如 Claude Code）把编辑器当 MCP/工具驱动；同时保留进程内嵌接口给内建 AI 面板。二者共用 AICommandBridge。
16.6 类 Git 的编辑历史（EditHistory / CheckpointService）
回答"能否做成 git 那种方便 AI 撤回"——能。在线性 CommandStack 上叠一层 git 语义，两级粒度：
Git 概念
编辑器对应
实现
commit
Transaction（一批命令 + message + author + 时间戳）
CompositeCommand 包裹，压入历史带元数据
log
事务列表（人/AI 交错，可归属）
EditHistory::log()
HEAD
当前命令栈位置
CommandStack 指针
undo/redo
细粒度逐命令
现有 §4.2
tag / checkpoint
Checkpoint（命名标记）
轻量=栈位置；重量=Scene::serialize() 快照
reset --hard
revertTo(checkpoint)
轻量=连续 undo 到该位；重量=deserialize 快照
stash / branch(可选)
备选快照
存多份 SceneRes，进阶
// editor/framework/history/EditHistory.h
#pragma once
#include <string>
#include <vector>
#include <optional>
#include "runtime/function/world/scene.h" // SceneRes
#include "../core/Signal.h"

namespace cakery {

class EditorContext;

enum class Author { User, AI, System };
enum class CheckpointMode { StackMarker, SceneSnapshot }; // 轻量 / 重量(git reset --hard)

struct TransactionInfo {
    uint64_t id; Author author; std::string message; /* timestamp 由外部注入 */ size_t commandCount;
};
struct CheckpointInfo { uint64_t id; std::string name; Author author; CheckpointMode mode; };

class EditHistory {
public:
    explicit EditHistory(EditorContext& ctx) : m_ctx(ctx) {}

    // 事务 = commit: begin 后所有 execute 归入本事务, commit 时合并为一条历史项
    void      beginTransaction(Author who, std::string message);
    uint64_t  commit();                  // 返回事务 id
    void      abortTransaction();        // 放弃(逐一 undo 本事务已执行的命令)

    std::vector<TransactionInfo> log() const;   // git log

    // 检查点
    uint64_t createCheckpoint(std::string name, Author who,
                              CheckpointMode mode = CheckpointMode::SceneSnapshot);
    void     revertTo(uint64_t checkpointId);   // git reset --hard / soft
    std::vector<CheckpointInfo> checkpoints() const;

    Signal<> changed;                    // 终端/历史面板刷新
private:
    EditorContext& m_ctx;
    // 事务栈, 检查点表(含可选 SceneRes 快照)
};

} // namespace cakery
设计要点：
- AI 会话映射事务：AICommandBridge::beginSession → beginTransaction(AI, goal) + createCheckpoint(SceneSnapshot)；endSession(false) → revertTo(检查点)；endSession(true) → commit()。一次 AI 干预 = 一个 commit，坏了一键回滚，不污染人类的细粒度 undo。
- 两级粒度的必要性：细粒度 ICommand.undo 覆盖 99% 常规编辑；SceneSnapshot 是对非命令化操作/批量实验的兜底（等价 git reset --hard），最稳。
- 归属与审计：每条历史带 Author，终端可 history.log 打印"谁在何时改了什么"，AI 行为完全可审计、可追责、可撤销。
- 与 PlayMode 复用：SceneSnapshot 复用 §4.8 已有的 serialize/deserialize 快照机制。
16.7 安全与归属
- 命令白名单：AICommandBridge::setAllowlist 限定 AI 可调用集合（如禁止 scene.open 覆盖当前工程）。
- 危险命令确认：delete、scene.save、大范围 revert 可要求人类在 TerminalPanel 点确认（setConfirmRequired）。
- 干跑预览：dryRun 先校验参数/预演影响（如"将删除 12 个实体"），AI 或人确认后再执行。
- 只读隔离：query.*/list 不进历史、不置脏、无副作用，AI 可自由高频观察。
16.8 数据流：AI 一轮改动可整体回滚
AICommandBridge.beginSession("铺草地")
  → EditHistory.beginTransaction(AI,"铺草地") + createCheckpoint(SceneSnapshot)
AI: query.entities → 观察 (只读, 不入历史)
AI: execute("tile.fill", {...}) ×N
  → CommandRegistry.dispatch → PaintTilesCommand → CommandStack.execute
  → 归入当前事务; TerminalPanel 显示 [ai] tile.fill ✓
人看结果不满意 → endSession(keep=false)
  → EditHistory.revertTo(检查点)  // deserialize 快照, 场景回到 AI 动手前
或满意 → endSession(keep=true) → commit()  // 历史里出现一条 [ai]"铺草地" commit, 可再整体 undo
16.9 目录追加 + 验收
editor/framework/console/            ← 新增 (namespace cakery, 零 Qt)
├── CommandRegistry.{h,cpp}
├── builtin_commands.cpp             (注册 entity./component./scene./tile./query./history. 命令)
└── AICommandBridge.{h,cpp}
editor/framework/history/            ← 新增
└── EditHistory.{h,cpp}
editor/Cakery/panels/
└── TerminalPanel.{h,cpp}            ← 新增 (与 ConsolePanel 同 Tab)
验收标准：
1. 终端输入 entity.create name=Foo 建出实体，Hierarchy 立即出现；Ctrl+Z 可撤销。
2. 终端 component.set <uuid> TransformComponent.position 1,2,0 改值，Inspector 同步刷新。
3. history.log 打印人/AI 交错的事务列表，含 author 与 message。
4. AI 经 AICommandBridge 内省命令 → 执行一串修改 → endSession(keep=false) 场景完整回到动手前（git reset 语义）。
5. query.entities 返回结构化 Json，不进历史、不置脏。
6. 白名单/确认/干跑对危险命令生效。

---
附:实施顺序建议(非阶段,仅依赖排序)
虽然本方案为最终版一体交付,但代码落地存在依赖先后,建议按此顺序编写以便随时可编译运行:
EditorContext + Signal (地基)
  → LayoutManager + Panel 基类 + 默认布局 (界面骨架先立起来, §13)
  → SelectionManager + CommandStack + 基础命令
  → SceneDocument (接 New/Open/Save)
  → 反射元数据 + PropertyDrawerRegistry + 抽屉 (重写 Inspector)
  → EditorCamera + EditorCameraChannel
  → 多视口 API (createViewport/getActiveGameCamera) + ViewportService (§7.5/7.6/§14)
  → ScenePanel 接编辑相机视口 · GamePanel 接游戏相机视口 (双视口成型)
  → DebugDraw + PickingService (Scene 视口选中)
  → GizmoService (变换操作)
  → PlayModeController (Play 快照 + Game 视图聚焦/Maximize)
  → EventBridge (双向绑定收尾)
  → AssetDatabase (资源升级)
  → 瓦片编辑器: tile_layer 单格 API + TileCoord + TilePaintService + PaintTilesCommand
                + TilePalettePanel + ScenePanel 网格/绘制 + Tiled 导入接线 (§15)
  → 命令控制台: CommandRegistry + builtin_commands + TerminalPanel (人可用) (§16.2-16.4)
  → EditHistory (事务/检查点, 类 Git) + AICommandBridge (AI CLI + 会话回滚) (§16.5-16.6)
  → 帧循环 Timer 化 + 清理重复代码
每完成一行,编辑器应保持可构建、可运行。

---
文档结束。如需将任一模块(如命令系统、Gizmo、PropertyDrawer)展开为逐文件的实现级代码,可在此基础上继续细化。