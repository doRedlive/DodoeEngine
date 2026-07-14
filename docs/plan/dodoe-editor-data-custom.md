Cakery 编辑器增量实施计划 —— 数据驱动配置 + 自定义编辑器
本文档是对既存架构文档 Cakery编辑器重构方案_类UnityUE最终架构.md（v1.1, 1979 行）的增量补全，不是替代。
既存文档已设计到位的部分（Framework 层、CommandStack、SelectionManager、SceneDocument、PropertyDrawer 接口、LayoutManager 接口、CommandRegistry、反射元数据接口签名），本计划不重复设计，只标注其代码实现缺口并排成可执行步骤。
本计划专门补两件既存文档未覆盖、而用户本次明确要求的事：
1. "各种配置不要写死，写成配置 json 文件" —— 面板注册、菜单/工具栏动作、布局预设、自定义编辑器映射全部 JSON 化。
2. "像 Unity 那样自定义编辑器" —— 既存文档只有字段级 PropertyDrawer + attribute，缺组件级 CustomEditor（整组件自定义 Inspector）这一层。
文档版本：v1.0（增量）· 2026-07-13
适用引擎：dodoe runtime（DX12 / Deferred / DualThread）· Qt6 + qt-ADS · C++20 · nlohmann/json

---
0. 现状对齐（实现 vs 既存文档）
代码已按 v1.1 文档实现了 Framework 骨架（EditorContext/CommandStack/SelectionManager/SceneDocument/EditorCamera/PickingService/GizmoService/PlayModeController/EventBridge/ViewportService 全在）与 PropertyDrawer 接口。但以下实现缺口已通过代码核对确认：
#
缺口
位置
严重度
G1
PropertyDrawerRegistry::registerBuiltinDrawers() 从未被调用，全树 grep 零调用点 → Inspector 对所有字段返回 nullptr，不产出任何 widget
editor/property/PropertyDrawerRegistry.cpp:50
P0
G2
PropertyDrawerRegistry::create() 的 attribute 分支是坏的：无条件 return 第一个 factory
editor/property/PropertyDrawerRegistry.cpp:42-47
P0
G3
FieldAccessor 无 attribute/enum 元数据接口（文档 §7.1 已设计签名，未实现）
runtime/core/meta/reflection/reflection.h:117
P1
G4
EnumDrawer 硬编码 CameraType 枚举值，不读反射
editor/property/drawers/EnumDrawer.cpp
P1
G5
CompositeDrawer 是 stub，只显示标签，不递归子字段
editor/property/drawers/CompositeDrawer.cpp
P1
G6
PPtrDrawer 只读，无拖拽赋值
editor/property/drawers/PPtrDrawer.cpp
P2
G7
InspectorPanel 选中即全量 clearEditors()+重建，未走 diff + updateValue（文档 §5.3 已设计增量刷新，未实现）
editor/Cakery/panels/InspectorPanel.cpp:76-145
P1
G8
InspectorPanel 用字符串 "IDComponent"/"TagComponent" 跳过内置组件，未用 Entry::addable 标志
editor/Cakery/panels/InspectorPanel.cpp:108
P2
G9
面板全部硬编码在 EditorWindow::setupDockWidgets()，无 PanelRegistry
editor/Cakery/app/EditorWindow.cpp:159-197
P1（本计划核心）
G10
LayoutManager 是空壳：applyDefault/restoreSession/saveSession/applyPreset 全空实现
editor/Cakery/app/LayoutManager.cpp
P1（本计划核心）
G11
菜单 Assets/GameObject/Component 是空占位，无动作注册
editor/Cakery/app/EditorWindow.cpp:91-93
P2（本计划核心）
G12
根目录残留 imgui.ini（编辑器已是 Qt-ADS），误导
repo root
P3
既存文档未覆盖（本计划新增）：
#
新增项
说明
N1
JSON 配置层
面板/菜单/工具栏/布局预设/Inspector 映射全部 JSON 驱动
N2
PanelRegistry
按工厂名创建 panel，配合 JSON 布局
N3
组件级 CustomEditor
整组件自定义 Inspector + JSON 映射（字段级 drawer 文档已有）
N4
字段级 attribute 外挂表（JSON fallback）
文档 §7.1 建议改 codegen 透传 attribute；本计划给出"先 JSON 外挂表、后切 codegen"的低风险路径

---
1. 配置文件总览（"不写死"的落点）
engine/res/editor/                       # 内置默认（进版本管理，团队共享）
├── editor.json                          # 全局：默认布局预设名、主题 qss、快捷键、各开关
├── layouts/
│   ├── default.layout.json              # 预设布局（JSON，可读可 diff）
│   ├── wide.layout.json
│   ├── tall.layout.json
│   └── 2by3.layout.json
├── menus.json                           # 菜单 + 工具栏动作表（command 名 → 参数）
├── inspectors.json                      # 组件名 → CustomEditor 工厂名；字段 attribute 外挂覆盖
└── panels.json                          # 内置 panel 工厂名清单 + 默认可见性/图标（可选）

<工程目录>/Configs/editor/               # 项目级覆盖（随工程走）
└── (同上结构，存在则覆盖内置)

%APPDATA%/Cakery/<project-name>/         # 用户级（不入版本管理）
├── session.json                         # 上次布局(perspective 名 + 各 dock 可见性)、最近场景、窗口几何
├── preferences.json                     # 用户偏好覆盖（主题、字号、栅格吸附…）
└── layouts/                             # 用户自存布局（Qt-ADS saveState 字节流 base64）
加载与合并顺序（后者优先级更高，逐键合并，用 nlohmann merge_patch）：
内置默认 engine/res/editor/*
  ↓ merge_patch
项目级 <工程>/Configs/editor/*
  ↓ merge_patch
用户级 %APPDATA%/Cakery/<project>/{preferences.json, session.json}
复用既有 runtime/core/utils/json.h（nlohmann alias 为 dodoe::Json）。新增 editor/framework/config/EditorConfig.{h,cpp}（零 Qt，namespace cakery）负责三级加载与合并；EditorContext::boot() 末尾构造它并供各服务查询。

---
2. PanelRegistry + JSON 布局（修 G9 / G10，新增 N1 / N2）
2.1 PanelRegistry
// editor/Cakery/app/PanelRegistry.h   (namespace cakery, 允许依赖 Qt)
class PanelRegistry {
public:
    static PanelRegistry& self();
    using Factory = std::function<Panel*(EditorContext& ctx, QWidget* parent)>;
    void registerPanel(const std::string& factoryName, Factory f);
    Panel* create(const std::string& factoryName, EditorContext& ctx, QWidget* parent) const;
    bool has(const std::string& factoryName) const;
private:
    std::unordered_map<std::string, Factory> m_factories;
    void registerBuiltinPanels();   // Scene/Game/Hierarchy/Inspector/Project/Console/Terminal
};
- 每个 panel 在启动时自注册（或集中 registerBuiltinPanels() 一次性注册，二选一；推荐集中注册，简单可测）。
- 工厂名（"Hierarchy"/"Scene"/"Inspector"…）即 JSON 布局里引用的 factory 字段。
2.2 布局 JSON schema
// engine/res/editor/layouts/default.layout.json
{
  "name": "Default",
  "panels": [
    { "id": "Scene",      "factory": "Scene",      "area": "Center", "central": true },
    { "id": "Game",       "factory": "Game",       "area": "Center", "tabWith": "Scene" },
    { "id": "Hierarchy",  "factory": "Hierarchy",  "area": "Left" },
    { "id": "Inspector",  "factory": "Inspector",  "area": "Right" },
    { "id": "Project",    "factory": "Project",    "area": "Bottom", "relativeTo": "Hierarchy" },
    { "id": "Console",    "factory": "Console",    "area": "Bottom", "relativeTo": "Inspector" },
    { "id": "Terminal",   "factory": "Terminal",   "area": "Center", "tabWith": "Console" }
  ],
  "sizes": { "left": 320, "right": 360, "bottom": 260 }
}
字段语义：
- area: Center | Left | Right | Top | Bottom（映射 ads::DockWidgetArea）。
- central: true：作为 setCentralWidget（全布局有且仅一个）。
- tabWith：与已注册的某 panel 同区 tab 并列。
- relativeTo：addDockWidget(area, dw, relativeDock->dockAreaWidget()) 的锚 panel id。
- id：dock widget 的唯一 objectName（Qt-ADS perspective 还原依赖它，文档 §13.3 已强调）。
- sizes：主分割条尺寸（best-effort，Qt-ADS 还原 perspective 后可二次校准）。
2.3 LayoutManager 落地（填空壳 G10）
LayoutManager 现有空壳方法全部实现，两层存储（呼应用户选择"JSON 预设 + 用户 session 二选一存"）：
方法
实现
applyPreset(name)
读 engine/res/editor/layouts/<name>.layout.json → 经 PanelRegistry 创建 panel → 按 area/tabWith/relativeTo 塞 CDockManager
applyDefault()
applyPreset(config().defaultLayout())
saveNamed(name)
m_dm->addPerspective(name)（用户拖拽后的快照，存进 %APPDATA%/Cakery/<project>/layouts/）
loadNamed(name)
m_dm->openPerspective(name)
deleteNamed(name)
m_dm->removePerspective(name) + 删用户布局文件
namedLayouts()
m_dm->perspectiveNames()
saveSession()
session.json 写：当前 perspective 名 + m_dm->saveState() 的 base64 + 各 dock 可见性 + 窗口几何
restoreSession()
读 session.json：先 applyDefault() 建 panel，再 m_dm->restoreState() 还原用户拖拽；失败 fallback applyDefault()
关键：预设（JSON）与用户 session（字节流）职责分离——预设进版本管理可 diff，用户私有布局不污染团队。这正是 Unity 没做好、我们做对的地方。
2.4 EditorWindow 改造
setupDockWidgets() 删除硬编码的 7 个 new XxxPanel，改为：
PanelRegistry::self().registerBuiltinPanels();
m_layoutManager->applyDefault();   // 由 JSON 驱动创建并停靠
EditorWindow 仍持有各 panel 指针（如 m_scenePanel），改为从 CDockManager 按 objectName 查回（m_dockManager->findDockWidget("Scene")->widget()），保证 setupFrameTimer 等处的强引用不丢。
Window 菜单（文档 §13.2）加 Layouts ▸ 子菜单：列出预设 + 用户布局 + "Save Layout…" + "Reset to Default"。

---
3. 组件级 CustomEditor（新增 N3）
既存文档 §5 只有字段级 drawer。Unity 的 CustomEditor(typeof(T)) 是替换整组件 Inspector。补这一层：
3.1 ComponentDB::Entry 扩展
// runtime/core/meta/component_db.h  Entry 追加
using InspectorDrawFunc = void (*)(Entity&, EditorContext*);  // 见下注
// 或保持零 Qt：用 std::function<void(void* compPtr, const InspectorContext&)>
因 ComponentDB 在 runtime 层（零 Qt、零编辑器依赖），CustomEditor 的注册表应放编辑器侧而非 Entry 内。最终设计：
// editor/property/CustomEditorRegistry.h   (namespace cakery, 依赖 Qt)
struct InspectorContext {
    EditorContext* ctx;
    dodoe::Uuid    entity;
    std::string    componentName;
    void*          componentPtr = nullptr;
    QWidget*       parent = nullptr;          // drawer 在此父下建控件
};

class CustomEditor {
public:
    virtual ~CustomEditor() = default;
    virtual QWidget* build(const InspectorContext& ic) = 0;   // 整组件区域（含标题折叠框）
    virtual void refresh(const InspectorContext& ic) = 0;     // 外部变更后只刷新值
};

class CustomEditorRegistry {
public:
    static CustomEditorRegistry& self();
    using Factory = std::function<std::unique_ptr<CustomEditor>()>;
    void registerByComponent(const std::string& componentName, Factory f);
    void registerByName(const std::string& editorName, Factory f);   // 供 inspectors.json 按名引用
    std::unique_ptr<CustomEditor> create(const std::string& componentName) const;
private:
    std::unordered_map<std::string, Factory> m_byComponent;  // 组件名 → 工厂
    std::unordered_map<std::string, Factory> m_byName;       // 编辑器名 → 工厂
    std::unordered_map<std::string, std::string> m_comp2name; // 组件名 → 编辑器名（来自 JSON）
};
3.2 InspectorPanel 改造
rebuildForEntity 中，对每个组件：
auto editor = CustomEditorRegistry::self().create(entry.name);
if (editor) {
    w = editor->build(ic);           // 走自定义整组件编辑器
} else {
    w = buildDefaultComponentGroup(entry);  // 现有反射遍历 + PropertyDrawer（默认路径，覆盖 90%）
}
3.3 JSON 映射（呼应用户选择"JSON 映射 + C++ 工厂注册"）
// engine/res/editor/inspectors.json
{
  "customEditors": {
    "TransformComponent": "TransformInspector",
    "CameraComponent":    "CameraInspector"
  },
  "fieldAttributes": {                    // N4: attribute 外挂表（codegen 前的 fallback）
    "TransformComponent.rotation": { "Range": [0, 360], "Tooltip": "欧拉角(度)" },
    "TransformComponent.internalId": { "Hidden": true }
  }
}
- customEditors：组件名 → 编辑器名。C++ 侧 registerByName("TransformInspector", []{...}) 注册工厂。CustomEditorRegistry::create() 先查 JSON 映射得编辑器名，再查 m_byName 取工厂。切换/禁用自定义编辑器不用重编译（改 JSON 即可），新增编辑器才需写 C++。
- fieldAttributes：在 codegen 透传 attribute（§5）落地前，作为 fallback 让 PropertyDrawerRegistry::create() 查此表补充 Range/Hidden/Tooltip。
3.4 示范：TransformInspector
第一个 CustomEditor，做 Unity 风格 Transform（三行 Position/Rotation/Scale，联动 Gizmo，显示本地/世界切换）。验证整条链路通后，其余组件按需补。

---
4. 反射 attribute 透传（修 G3，落地文档 §7.1）
文档 §7.1 已给 FieldAccessor 签名（hasAttribute/attribute/attributeRange/isHidden/isReadOnly/enumValues）。落地分两步：
步骤 A（先做，低风险）：JSON 外挂表（N4）
不改 codegen，PropertyDrawerRegistry 在 create() 时查 inspectors.json 的 fieldAttributes，把 attribute 临时挂到 drawer 上下文。立即让 [Range]/[Hidden]/[Tooltip] 可用。
步骤 B（后做，一劳永逸）：codegen 透传
- metaparser field.cpp 已解析 META(...) annotation；reflection_generator 增加生成每个字段的 attribute 字典（key→value，值统一存字符串，Range 存 "0,360"）。
- FieldFuncTuple 追加 GetAttrsFunc，FieldAccessor 实现 §7.1 的接口。
- EnumDrawer（G4）改读 enumValues()；InspectorPanel（G8）改用 isHidden() 过滤字段。
- 完成后 JSON 外挂表退化为"覆盖"语义（JSON 优先于 codegen 值），仍保留，用于不改 C++ 即可调 UI。

---
5. PropertyDrawer 缺口修复（修 G1/G2/G4/G5/G6/G7/G8）
缺口
修复
G1 registerBuiltinDrawers 未调用
在 EditorContext::boot() 末尾（或 EditorApplication 构造后）调 PropertyDrawerRegistry::self().registerBuiltinDrawers()。第一步就做，立即让 Inspector 出 widget。
G2 create() attribute 分支坏
改为：①查字段 attribute 命中（Range/Color 等）→ 用对应 drawer；②查 m_byType；③复合类型（有子字段）→ CompositeDrawer；④兜底只读文本。
G4 EnumDrawer 硬编码
步骤 B 后读 enumValues()；步骤 B 前读 JSON 外挂表里的枚举值列表。
G5 CompositeDrawer stub
用 TypeMeta::newMetaFromName(fieldTypeName) 取子字段，递归 PropertyDrawerRegistry::create()，缩进呈现。
G6 PPtrDrawer 只读
接 ProjectPanel 拖拽（dragEnterEvent/dropEvent），落 FileID；显示资产名。
G7 Inspector 全量重建
按 (组件名,字段名) diff：未变复用 drawer 调 updateValue()，只增删变化项。选 EventBridge::componentChanged 时只刷新对应组件。
G8 字符串跳过内置组件
改用 entry.addable == false（IDComponent/TagComponent 注册时标 addable=false）过滤；加 Hidden attribute 兜底。

---
6. 菜单/工具栏数据驱动（修 G11，新增 N1）
// engine/res/editor/menus.json
{
  "menus": [
    { "path": "GameObject/3D Object/Empty",   "command": "entity.create",  "args": { "name": "GameObject" } },
    { "path": "GameObject/3D Object/Cube",    "command": "entity.create",  "args": { "preset": "Cube" } },
    { "path": "GameObject/Light/Directional", "command": "component.add",  "args": { "component": "DirectionalLightComponent" } },
    { "path": "Component/Physics/Rigidbody",  "command": "component.add",  "args": { "component": "RigidbodyComponent" } },
    { "path": "Assets/Import...",             "command": "asset.import",   "args": {} }
  ],
  "toolbar": [
    { "id": "Play",  "command": "playmode.play",  "checkable": true, "group": "play" },
    { "id": "Pause", "command": "playmode.pause", "checkable": true, "group": "play" },
    { "id": "Stop",  "command": "playmode.stop" },
    { "id": "Move",    "command": "gizmo.setmode", "args": { "mode": "Translate" }, "shortcut": "W", "checkable": true, "group": "gizmo" },
    { "id": "Rotate",  "command": "gizmo.setmode", "args": { "mode": "Rotate" },    "shortcut": "E", "checkable": true, "group": "gizmo" },
    { "id": "Scale",   "command": "gizmo.setmode", "args": { "mode": "Scale" },     "shortcut": "R", "checkable": true, "group": "gizmo" },
    { "id": "Hand",    "command": "gizmo.setmode", "args": { "mode": "None" },      "shortcut": "Q", "checkable": true, "group": "gizmo" }
  ]
}
- command 名映射到 文档 §16.2 的 CommandRegistry（已设计）。mutating 命令 handler 内部一律构造 ICommand 走 CommandStack——菜单动作天然带 Undo/Redo，这是 Unity 菜单没有的统一撤销。
- EditorWindow::setupMenuBar()/setupToolBar() 改为读 menus.json 递归建菜单/工具栏；shortcut 注入 QKeySequence。
- 若 §16.2 CommandRegistry 尚未实现，先建最小版（只 add/execute(name, Json args)），够菜单用即可，后续再接 AI/终端。

---
7. 配置加载器 EditorConfig（新增 N1 基座）
// editor/framework/config/EditorConfig.h   (namespace cakery, 零 Qt)
class EditorConfig {
public:
    void load(const std::string& engineResDir,
              const std::string& projectConfigDir,
              const std::string& userConfigDir);
    const dodoe::Json& get() const;          // 合并后的完整配置
    const dodoe::Json& layout(const std::string& presetName) const;
    const dodoe::Json& menus() const;
    const dodoe::Json& inspectors() const;
    std::string defaultLayoutName() const;
private:
    dodoe::Json m_merged;
    std::unordered_map<std::string, dodoe::Json> m_layouts;  // presetName → json
    void loadDir(const std::string& dir, dodoe::Json& out);
};
- EditorContext 持有 std::unique_ptr<EditorConfig>，boot() 时加载，供 LayoutManager/菜单构建/CustomEditorRegistry 查询。
- 三级目录不存在则跳过；用户级缺失不报错（首次运行）。

---
8. 落地步骤（依赖排序，每步可独立编译运行）
与既存文档"附:实施顺序建议"对齐：本计划聚焦其未覆盖的配置层与 CustomEditor，并补 PropertyDrawer 实现缺口。每步完成编辑器须保持可构建可运行。
步
内容
修缺口
产出可验证点
S1
调用 registerBuiltinDrawers()（在 EditorContext::boot 末尾）；修 create() attribute 分支 bug；加兜底只读 drawer
G1, G2
Inspector 选中实体后出现字段 widget（此前为零）
S2
EditorConfig 三级加载器 + editor.json/menus.json/inspectors.json/layouts/*.json 骨架文件
N1
配置可加载、可合并、日志打印合并结果
S3
PanelRegistry + LayoutManager 落地（填空壳）；EditorWindow::setupDockWidgets 改 JSON 驱动；session 存读
G9, G10, N1, N2
启动呈现默认布局；拖拽后重启还原；Window 菜单可切预设
S4
CustomEditorRegistry + inspectors.json 映射；InspectorPanel 接入；示范 TransformInspector
N3
Transform 组件显示自定义 UI；改 JSON 可切换回默认
S5
JSON attribute 外挂表（N4）：Range/Hidden/Tooltip 生效；InspectorPanel 用 addable+Hidden 过滤字段
G3(部分), G8
字段按 JSON attribute 呈现滑条/隐藏/提示
S6
CompositeDrawer 递归；EnumDrawer 读外挂表枚举值；PPtrDrawer 接拖拽
G4, G5, G6
嵌套结构体/枚举下拉/资产拖入均可用
S7
InspectorPanel diff + updateValue 增量刷新
G7
改一个字段不再全列重建；外部改值回流不重建
S8
菜单/工具栏 JSON 驱动（最小 CommandRegistry + menus.json）；空菜单 Assets/GameObject/Component 接线
G11, N1
菜单项可点击产出命令、带 Undo
S9
codegen 透传 attribute（FieldAccessor §7.1 接口 + generator）；JSON 外挂表退为覆盖语义
G3(完整)
C++ 内 META(Range=...) 直接生效
S10
清理：删根目录 imgui.ini（G12）；panel 源文件改 GLOB_RECURSE（文档 §11）
G12
无残留；构建维护性提升
依赖：S1 独立最先做。S2 是 S3/S4/S5/S8 的基座。S3 不依赖 S4。S9 依赖 S5 验证过 JSON 路径。S6/S7 可与 S3-S5 并行。

---
9. 风险与回退
风险
缓解
Qt-ADS restoreState 跨版本易碎
session 还原失败即 fallback applyDefault()（JSON 预设稳定）
JSON 配置写错导致编辑器起不来
EditorConfig 加载用 schema 校验（轻量手写校验，不引依赖）；关键字段缺失走内置默认，记 warning 不 fatal
codegen 改动影响反射/序列化生成（S9）
S9 前先用 S5 的 JSON 外挂表验证 UI 路径；codegen 改动单独分支，生成产物 diff 审查
CustomEditor 与默认路径行为不一致
CustomEditorRegistry::create() 返回 nullptr 时一律走默认反射路径；示范 TransformInspector 完整覆盖 position/rotation/scale 三字段后再启用映射
三级配置合并冲突
逐键 merge_patch，用户级覆盖项目级覆盖内置；数组（如菜单项）按"内置 + 项目追加"语义而非替换（菜单 JSON 用 merge 标记区分数组策略）

---
10. 验收标准（"像 Unity 且配置不写死"的最小达标线）
[] Inspector 选中实体显示字段 widget（S1，当前为零，必须先达标）
[] 面板/布局/菜单/工具栏均可由 engine/res/editor/*.json 改动生效，无需重编译（S3/S8）
[] 拖拽面板后重启编辑器，布局还原（S3）
[] TransformComponent 由 TransformInspector 自定义绘制；改 inspectors.json 可切回默认（S4）
[] 字段挂 Range/Hidden/Tooltip（JSON 或 META）后 Inspector 正确响应（S5/S9）
[] 菜单"GameObject/3D Object/Cube"点击后创建实体，且 Ctrl+Z 可撤销（S8）
[] Inspector 改字段不再全列闪烁重建（S7）

---
本计划是对 v1.1 架构文档的增量。如需将任一步骤展开为逐文件实现级代码，可在此基础上继续细化。