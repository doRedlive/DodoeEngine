Cakery 编辑器 —— Unity 化形态与数据驱动配置设计
本文档是既存架构文档 Cakery编辑器重构方案_类UnityUE最终架构.md（v1.1）的姊妹篇。
- v1.1 讲内核架构：命令层 / 选择 / Gizmo / 双视口 / 反射元数据 / Tilemap / 终端 —— 那些已设计且部分实现。
- 本文档讲界面形态 + 数据驱动配置 + 自定义编辑器：编辑器要长成 Unity 的样子、各种配置不写死改成 JSON、像 Unity 那样自定义编辑器。这是 v1.1 没展开、而本次明确要求的部分。
两文档不冲突：v1.1 的 EditorContext/CommandStack/PropertyDrawer 接口/LayoutManager 接口签名均沿用，本文档给它们补上界面落地质感与JSON 配置层。
文档版本：v1.0 · 2026-07-13
引擎：dodoe runtime（DX12 / Deferred / DualThread）· UI：Qt6 Widgets + qt-advanced-docking-system · C++20 · nlohmann/json
命名空间：编辑器侧统一 namespace cakery，runtime 侧 namespace dodoe

---
目录
1. 当前编辑器现状（界面视角）
2. 目标：Unity 化的整体形态
3. 各面板的 Unity 化规格
4. 视觉与主题系统
5. 数据驱动配置层（不写死）
6. Dock 容器视觉 —— 为什么现在不像 Unity
7. 布局系统 JSON 化
8. 菜单与工具栏 JSON 化
9. 自定义编辑器（Unity CustomEditor）
10. 字段级 PropertyDrawer 与 attribute
11. 落地步骤与验收

---
1. 当前编辑器现状（界面视角）
代码核对结论：视觉骨架已具备，功能形态离 Unity 差很多。
1.1 已经有的（保留）
项
现状
评价
UI 框架
Qt6 + qt-ADS（可停靠/拖拽/tab/浮动）
✅ 比 ImGui 更接近 Unity，底子好
主题
style.qss Dracula 暗色，菜单/工具栏/dock/树/输入/滚动条/滑条/checkbox 全样式化
✅ 质感已达标，只需补 Unity 配色
窗口骨架
EditorWindow = 菜单栏 + 工具栏(Play/Pause/Stop + Q/W/E/R) + 中央停靠 + 状态栏(FPS)
✅ 结构对
反射/组件库
TypeMeta/FieldAccessor/ComponentDB（metaparser clang 代码生成）
✅ 数据驱动基础在
Inspector 接口
PropertyDrawer/PropertyDrawerRegistry（按类型/attribute 注册）
✅ 接口对，实现有缺口
命令/选择
CommandStack/SelectionManager/SceneDocument
✅ v1.1 已实现
1.2 离 Unity 的差距（本文档要解决的）
#
现状
Unity 目标
D1
Inspector 选中实体不显示任何字段（registerBuiltinDrawers() 从未被调用，全树零调用点）
反射驱动显示所有字段
D2
Hierarchy 是平铺 QTreeWidget，无父子嵌套、无图标、无右键菜单、无拖拽 reparent
树形嵌套 + 图标 + 右键菜单 + 拖拽改父 + 搜索框
D3
Project 是裸 QDir 文件树，不接 AssetDatabase、无缩略图、无 GUID 拖拽
AssetDatabase 驱动 + 缩略图网格 + 双击打开 + 拖入场景
D4
菜单 Assets/GameObject/Component 是空占位
完整菜单树，每项可点击产出命令
D5
工具栏只有文字按钮，无图标、无分隔分组、无布局选择器
图标化 + 分组 + 右上角 Layout 下拉
D6
面板全硬编码在 EditorWindow::setupDockWidgets()
PanelRegistry + JSON 布局
D7
LayoutManager 是空壳（applyDefault/restoreSession/saveSession/applyPreset 全空）
预设 JSON + 用户 session 存读
D8
无组件级自定义编辑器（只有字段级 drawer 接口）
Unity CustomEditor 整组件自定义
D9
FieldAccessor 无 attribute（Range/Tooltip/Hidden 无法用）
[Range] 滑条、[Tooltip] 提示、[Hidden] 过滤
D10
根目录残留 imgui.ini（已是 Qt-ADS），里面还混着 ImGui 时代旧 dock 布局
删除
D11
EnumDrawer 硬编码 CameraType；CompositeDrawer 是 stub；PPtrDrawer 只读
反射枚举 / 递归结构体 / 拖拽赋值
D12
Dock 容器视觉不像 Unity（当前最不满意点，详见 §6）：DisableStylesheet:true 导致 ADS 的 qss 全部失效、title bar 粗高且 close/float 按钮常驻、tab 样式默认、视口被 dock 框包着无全屏感
ADS 配旗标 + qss 生效 + 紧凑 title bar + 视口无边框嵌入

---
2. 目标：Unity 化的整体形态
2.1 目标布局（对齐 Unity 默认 + 优化）
┌─────────────────────────────────────────────────────────────────────────┐
│ File  Edit  Assets  GameObject  Component  Window  Help        [Layout▼]│  菜单栏
├─────────────────────────────────────────────────────────────────────────┤
│  [▶Play][⏸][⏹]   ┃   [Hand][Move][Rotate][Rect/Scale]   ┃ [搜索]  [布局]│  工具栏（图标+分组）
├──────────┬──────────────────────────────────────┬───────────────────────┤
│          │  ┌ Scene │ Game ┐ (Tab)              │                       │
│Hierarchy │  │ Q W E R T | Shading▼ | 2D▼ | Gizmo│      Inspector        │
│  🔍 搜索  │  │                                      │   Entity Name [...]  │
│  ▾ Scene  │  │         视口渲染区                  │   ┌─ Transform ────┐ │
│    🗊 DirL│  │      (ScenePanel/GamePanel)        │   │ Position  x y z │ │
│    ○ Cube │  │                                      │   │ Rotation  x y z │ │
│    ▾ Env  │  │                                      │   │ Scale     x y z │ │
│      ☀ Sun│  │                                      │   └─────────────────┘ │
│          │  │                                      │   ┌─ Camera ───────┐ │
├──────────┴──┴──────────────────────────────────────┤   │ Projection ▼   │ │
│  Project (资源浏览器)           │  Console (日志)    │   │ FOV  [───●──]  │ │
│  🔍 [□网格▼]                   │  [Clear][Collapse] │   └─────────────────┘ │
│  [🗂][🖼] 双栏: 树 + 缩略图网格  │  ▸ Warn  ⚠ x      │   [+ Add Component]   │
└─────────────────────────────────────────────────────────────────────────┘
                                          状态栏: Ready · 60 FPS ·坐标
2.2 相对 Unity 的三点优化（这是"优化"的落点）
1. 布局完全数据驱动 + 多预设可切换 + 进版本管理。Unity 布局存二进制、跨机器丢失、不可 diff。Cakery 预设用 JSON（engine/res/editor/layouts/*.json），团队共享、可 diff；用户拖拽后的私有布局存 %APPDATA%，互不干扰。
2. Inspector 增量刷新不重建。Unity 改一个字段整列 widget 闪烁重建，是 Inspector 掉帧元凶。Cakery 按 (组件,字段) diff，未变复用 drawer 调 updateValue() 只刷新值。
3. 面板懒创建。Unity 首次切到某面板会卡一下。Cakery dock widget 先注册元信息，首次 visibilityChanged 才实例化（含 Scene/Game 渲染窗口），冷启动更快。

---
3. 各面板的 Unity 化规格
3.1 Hierarchy（层级面板）
当前：QTreeWidget 平铺，只 addTopLevelItem，无嵌套。
目标：
- 树形嵌套，读 HierarchyComponent（父子关系，已有）。
- 每项图标（实体类型：空对象/灯光/相机/网格…，图标映射见 panels.json）。
- 顶部搜索框（按名过滤，Unity t: 类型搜索可选进阶）。
- 右键菜单：Create Empty / Create Parent / Duplicate / Rename / Delete / Reparent。
- 拖拽改父 → ReparentEntityCommand（走命令栈，可撤销）。
- 双击重命名（inline edit）→ RenameEntityCommand。
- 订阅 SelectionManager::changed 高亮、EventBridge::hierarchyChanged 刷新。
- 顶部 tab：Scene（当前场景）/ 加号（多场景，可选）。
- 左上角小工具：锁定选择、搜索过滤。
3.2 Inspector（检视器面板）—— 核心
当前：选中实体不出字段（D1）。
目标（分三层，详见 §8 §9）：
1. 实体头：图标 + 名称输入框 + Active checkbox + Static checkbox + Tag 下拉 + Layer 下拉（对齐 Unity）。
2. 组件区：每个组件一个可折叠 QGroupBox（带齿轮菜单：Reset / Remove / Copy / Paste / Move Up/Down）。
  - 默认走反射 + PropertyDrawer（覆盖 90% 组件）。
  - 挂了 CustomEditor 的组件走整组件自定义绘制（§8）。
3. Add Component 按钮：弹可搜索菜单（按组件分类，读 ComponentDB::entries() 的 addable 过滤）→ AddComponentCommand。
修复清单（对应 D1/D9/D11）：
- EditorContext::boot() 末尾调 PropertyDrawerRegistry::self().registerBuiltinDrawers()。
- 修 PropertyDrawerRegistry::create() 的 attribute 分支 bug（现无条件 return 第一个 factory）。
- FieldAccessor 补 attribute 接口（§9）。
- CompositeDrawer 递归子字段；EnumDrawer 读反射枚举；PPtrDrawer 接拖拽。
- 跳过内置组件改用 entry.addable==false 而非字符串比较。
- 选中变更走 diff + updateValue，不全量重建。
3.3 Scene / Game（双视口）
v1.1 §14 已详设计（Scene=编辑相机+Gizmo，Game=游戏相机）。界面侧补：
- Scene 顶部工具条：Q/W/E/R/T 工具 + Shading 模式下拉（Shaded/Wireframe/Albedo/Normal…）+ 2D/3D 切换 + Gizmo 显隐 + 场景相机保存/对齐。
- Game 顶部：分辨率/宽高比下拉（Free Aspect / 16:9 / 1920×1080…）+ Scale 滑条 + Maximize on Play。
- 两者同停靠区 tab，可拆出并排（Unity 拖 tab 出来）。
3.4 Project（资源浏览器）
当前：QDir 裸文件树。
目标（对齐 Unity 双栏）：
- 左栏：资源目录树（只读文件夹层级）。
- 右栏：缩略图网格（图标可调大小），文件名在下。
- 顶部：搜索框 + 过滤下拉 + 网格/列表切换 + 一键打开工程目录。
- 由 AssetDatabase 驱动（已有 GUID 元数据缓存），不再裸读 QDir。
- 双击：场景→SceneDocument::openScene，脚本→打开外部编辑器，预制体→进入预制体编辑（可选）。
- 拖入 Scene 视口 → CreateEntityCommand（带 prefab/资产引用）。
- 右键：Create / Import / Reimport / Delete / Show in Explorer。
- 底部状态：选中资产 GUID、类型、大小。
3.5 Console / Terminal
- Console：日志列表（LogService 驱动），按级别过滤（Info/Warn/Error）、Clear、Collapse（相同合并计数）、搜索。
- Terminal：命令行 REPL（v1.1 §16），经 CommandRegistry 派发，产出 ICommand。与 Console 同 tab。
3.6 状态栏
当前：Ready + FPS。补：选中实体计数、鼠标世界坐标、栅格吸附开关、构建状态。

---
4. 视觉与主题系统
4.1 配色：Unity 化（保留暗色，调色板对齐 Unity）
当前 Dracula（紫粉 #BC92F9/#FF79C6）偏紫。Unity 暗色是中性深灰 + 蓝色强调。建议新增 engine/res/editor/themes/unity-dark.qss，主色调整：
用途
当前(Dracula)
Unity 暗色目标
窗口背景
#191A21
#383838
面板背景
#282A36
#2D2D2D
强调色
#BC92C9 紫
#2D7DFF 蓝（选中/聚焦）
文字主
#F8F8F2
#C4C4C4
文字次
#6272A4
#8C8C8C
警告/错误
#FF5555
#FF5A5A / 保持
主题走 editor.json 的 theme 字段切换，EditorApplication 启动按值加载对应 qss。多主题（unity-dark / dracula / light）并存，JSON 选。
4.2 图标
- 图标统一放 engine/res/editor/icons/（SVG 优先，qss 可着色）。
- 工具栏/面板 tab/树节点/组件头都用图标，不再纯文字。
- 图标映射在 panels.json（面板）、menus.json（菜单/工具栏）、inspectors.json（组件图标）里以路径引用。
4.3 qss 资源化
当前 style.qss 硬路径加载（EditorApplication 读 resources/style.qss）。改为 editor.json 指定主题名 → 加载 themes/<name>.qss，支持热重载（编辑器运行中改主题即时生效，便于调样式）。

---
5. 数据驱动配置层（不写死）
5.1 配置文件总览
engine/res/editor/                       # 内置默认（进版本管理，团队共享）
├── editor.json                          # 全局：默认布局/主题/快捷键/各开关
├── themes/
│   ├── unity-dark.qss                   # Unity 暗色主题
│   ├── dracula.qss                      # 现有主题
│   └── light.qss                        # 亮色（可选）
├── layouts/
│   ├── default.layout.json              # 预设布局（JSON，可读可 diff）
│   ├── wide.layout.json
│   ├── tall.layout.json
│   └── 2by3.layout.json
├── menus.json                           # 菜单 + 工具栏动作表
├── panels.json                          # 面板工厂名 + 图标 + 默认可见性
└── inspectors.json                      # 组件→CustomEditor 映射 + 字段 attribute 外挂

<工程目录>/Configs/editor/               # 项目级覆盖（随工程走，可选）
└── (同上结构，存在则覆盖内置)

%APPDATA%/Cakery/<project-name>/         # 用户级（不入版本管理）
├── session.json                         # 上次布局/可见性/窗口几何/最近场景
├── preferences.json                     # 用户偏好（主题/字号/吸附…）
└── layouts/                             # 用户自存布局（Qt-ADS saveState base64）
5.2 三级加载与合并
内置 engine/res/editor/*
  ↓ merge_patch（逐键，后者覆盖）
项目级 <工程>/Configs/editor/*
  ↓ merge_patch
用户级 %APPDATA%/Cakery/<project>/{preferences,session}.json
新增 editor/framework/config/EditorConfig.{h,cpp}（namespace cakery，零 Qt）负责加载/合并/查询，EditorContext::boot() 构造它。用 nlohmann merge_patch。加载失败走内置默认 + warning，不 fatal。
5.3 editor.json 示例
{
  "version": 1,
  "theme": "unity-dark",
  "defaultLayout": "default",
  "shortcuts": {
    "edit.undo": "Ctrl+Z",
    "edit.redo": "Ctrl+Y",
    "scene.new": "Ctrl+N",
    "scene.save": "Ctrl+S",
    "tool.move": "W", "tool.rotate": "E", "tool.scale": "R", "tool.hand": "Q",
    "edit.duplicate": "Ctrl+D", "edit.delete": "Del"
  },
  "editor": {
    "gridSnap": true, "gridSize": 0.25,
    "showGizmos": true, "autoSaveIntervalSec": 0
  }
}

---
6. Dock 容器视觉 —— 为什么现在不像 Unity
本节专治"dock 面板跟 Unity 差距很大"这一当前最不满意点。
关键认知："dock 长什么样"和"布局怎么存"是两回事。§7 讲布局 JSON 存储，本节讲 dock 容器本身的视觉。现在不像 Unity 的根因在容器视觉，不在布局存储。
6.1 根因诊断（代码核对）
当前 EditorWindow 构造里（EditorWindow.cpp:53-56）：
ads::CDockManager::setConfigFlag(ads::CDockManager::OpaqueSplitterResize, true);
ads::CDockManager::setConfigFlag(ads::CDockManager::XmlCompressionEnabled, false);
ads::CDockManager::setConfigFlag(ads::CDockManager::FocusHighlighting, true);
ads::CDockManager::setConfigFlag(ads::CDockManager::DisableStylesheet, true);   // ← 元凶
DisableStylesheet: true 是第一眼不像 Unity 的主因。 它让 Qt-ADS 跳过自身样式表机制，于是 style.qss 里精心写的 ads--CDockWidgetTab / ads--CDockAreaTitleBar / ads--CDockAreaTabBar 全部不生效，dock 标题栏退化成 Qt 原生灰条、tab 是默认外观。Unity 那种紧凑深色 tab 条根本没出现。
由此衍生的问题清单：
#
问题
Unity 对照
V1
qss 对 ADS 全部失效（DisableStylesheet）
ADS 应走 qss，tab/titlebar 全样式化
V2
title bar 粗高（~28px+），close/float 按钮常驻显眼
Unity 面板头 ~20px 紧凑，close 悬停才出现
V3
tab + 按钮 + 菜单按钮挤一行，按钮多
Unity 只有 tab + 右侧一个小 close（悬停显）
V4
视口（Scene/Game）被 dock title bar 包着，无全屏感
Unity 视口嵌主窗口、四周面板浮其上，视口本身无边框
V5
tab 文字默认大小/颜色，无激活态高亮区分
Unity tab 小写灰字、激活高亮、非激活淡
V6
分割条默认粗细/颜色
Unity 分割条极细（1-2px）、悬停高亮
V7
浮动窗口带系统标题栏
Unity 浮动窗口无系统框、自定义头
V8
imgui.ini 残留混入 ImGui 旧 dock 布局，视觉层未收拾干净
删除
6.2 ADS 旗标修正（修 V1/V3/V7）
EditorWindow 构造里，去掉 DisableStylesheet，改设一组对齐 Unity 的旗标：
// 关掉 DisableStylesheet —— 让 qss 对 ADS 生效（修 V1）
ads::CDockManager::setConfigFlag(ads::CDockManager::OpaqueSplitterResize, true);
ads::CDockManager::setConfigFlag(ads::CDockManager::XmlCompressionEnabled, false);
ads::CDockManager::setConfigFlag(ads::CDockManager::FocusHighlighting, true);

// Unity 化：去掉多余按钮，tab 紧凑（修 V3）
ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasCloseButton, true);        // 保留 close
ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasUndockButton, false);      // 去掉 float 按钮
ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasTabsMenuButton, false);    // 去掉 tab 菜单按钮
ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHideDisabledButtons, true);
ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaDynamicTabsMenuButtonVisibility, true);

// tab 行为对齐 Unity
ads::CDockManager::setConfigFlag(ads::CDockManager::TabCloseButtonIsTabBarScrollButton, false);
ads::CDockManager::setConfigFlag(ads::CDockManager::EqualSplitOnInsertion, true);          // 首次插入均分
ads::CDockManager::setConfigFlag(ads::CDockManager::FloatingContainerHasWidgetIcon, false);
ads::CDockManager::setConfigFlag(ads::CDockManager::FloatingContainerHasWidgetTitle, false); // 浮动窗口去系统框（修 V7）

// 拖拽时半透明预览，更像 Unity
ads::CDockManager::setConfigFlag(ads::CDockManager::DragPreviewIsDynamic, true);
ads::CDockManager::setConfigFlag(ads::CDockManager::DragPreviewShowsContentPixmap, false);
旗标名以实际 Qt-ADS 版本为准（engine/external/qt-ads），落地时核对 DockManager.h 的 eConfigFlag 枚举，个别名称可能略有差异。
6.3 紧凑 title bar（修 V2/V5）
Qt-ADS 的 title bar 高度由 qss 的 ads--CDockAreaTitleBar padding/min-height 控制。当前 qss 已有该选择器但被 DisableStylesheet 屏蔽。修正旗标后，调整 qss：
/* 紧凑 title bar，对齐 Unity ~20px */
ads--CDockAreaTitleBar {
    background: #2D2D2D;
    padding: 0px 4px;
    border: none;
    min-height: 22px;        /* ← 压矮，原 ~30px */
    max-height: 22px;
}

/* tab：小写、非激活灰、激活高亮 */
ads--CDockWidgetTab {
    background: transparent;
    color: #8C8C8C;
    padding: 4px 10px;
    border: none;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    margin-right: 1px;
    font-size: 11px;
    min-width: 0px;
}
ads--CDockWidgetTab[activeTab="true"] {
    background: #383838;
    color: #C4C4C4;
}
ads--CDockWidgetTab:hover:!activeTab { color: #C4C4C4; }

/* close 按钮只在悬停 tab 时显形，常驻时弱化（修 V3） */
#tabCloseButton {
    background: transparent; border: none; border-radius: 3px;
    min-width: 14px; min-height: 14px; padding: 0px;
}
#tabCloseButton:hover { background: #FF5A5A; }
ads--CDockWidgetTab:!active #tabCloseButton { /* 非激活 tab 的 close 弱化 */
    background: transparent; opacity: 0.3;
}
6.4 视口无边框全屏感（修 V4）
Unity 的 Scene/Game 视口是"嵌在主窗口、面板浮其上"的观感，视口本身没有 dock 头。两种做法：
做法 A（推荐，简单）：视口 dock 设为"不可关闭、不可浮动、无 title bar"的中央件，且其 title bar 用 CDockWidget::setFeature(CDockWidget::DockWidgetDeleteOnClose, false) + 自定义空 title bar 隐藏。即视口区看起来就是一块干净的渲染区，左右上下是其它面板的 dock。
做法 B（更彻底，对齐 Unity）：不用 setCentralWidget，而是把视口作为主窗口背景层（一个铺满的 QWidget），所有面板 dock 浮在其上（CDockManager 容器透明）。这是 Unity 的真实做法，但 Qt-ADS 实现成本高，初版用做法 A。
做法 A 落地：
auto* sceneDock = new ads::CDockWidget(tr("Scene"));
sceneDock->setWidget(m_scenePanel);
sceneDock->setFeature(ads::CDockWidget::DockWidgetClosable, false);
sceneDock->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
sceneDock->setFeature(ads::CDockWidget::DockWidgetMovable, false);
m_dockManager->setCentralWidget(sceneDock);   // 中央件，不可拖出
视口 dock 的 title bar 通过 qss ads--CDockAreaTitleBar 在中央区单独压到 0 高度，或自定义 CDockWidgetTitleBar 空实现。
6.5 分割条与边距（修 V6）
ads--CDockSplitter::handle { background: #1A1A1A; width: 1px; }
ads--CDockSplitter::handle:hover { background: #2D7DFF; }   /* 悬停蓝 */
ads--CDockContainerWidget { background: #2D2D2D; }          /* 面板间隙色 */
Unity 面板间是 1px 细缝，当前 3-4px 偏粗。
6.6 清理残留（修 V8）
删除根目录 imgui.ini（已是 Qt-ADS，残留且含 ImGui 旧 dock 布局，误导视觉层判断）。
6.7 dock 视觉走主题文件
以上 ADS qss 全部进 themes/unity-dark.qss（§4），不写进 C++。editor.json 的 theme 切主题时 dock 视觉一起切。dock 视觉本身也是数据驱动——换主题/调样式不用重编译。

---
7. 布局系统 JSON 化
7.1 PanelRegistry（修 D6）
// editor/Cakery/app/PanelRegistry.h   (namespace cakery)
class PanelRegistry {
public:
    static PanelRegistry& self();
    using Factory = std::function<Panel*(EditorContext& ctx, QWidget* parent)>;
    void registerPanel(const std::string& factoryName, Factory f);
    Panel* create(const std::string& factoryName, EditorContext& ctx, QWidget* parent) const;
    void registerBuiltinPanels();  // Scene/Game/Hierarchy/Inspector/Project/Console/Terminal
};
工厂名（"Scene"/"Hierarchy"…）即 JSON 里引用的 factory。
7.2 布局 JSON schema
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
  "sizes": { "left": 300, "right": 360, "bottom": 260 }
}
字段：area(Center/Left/Right/Top/Bottom → ads::DockWidgetArea)、central(唯一中央)、tabWith(与某 panel 同区 tab)、relativeTo(锚 panel)、id(dock 的 objectName，perspective 还原依赖它)、sizes(分割条尺寸 best-effort)。
7.3 LayoutManager 落地（填空壳 D7，呼应用户选的"JSON 预设 + 用户 session"）
方法
实现
applyPreset(name)
读 layouts/<name>.layout.json → PanelRegistry 建 panel → 按 area/tabWith/relativeTo 塞 CDockManager
applyDefault()
applyPreset(config.defaultLayoutName())
saveNamed(name)
m_dm->addPerspective(name) → 存 %APPDATA%/Cakery/<project>/layouts/
loadNamed(name)
m_dm->openPerspective(name)
deleteNamed(name)
m_dm->removePerspective(name) + 删用户文件
namedLayouts()
m_dm->perspectiveNames()
saveSession()
session.json 写：当前 perspective 名 + m_dm->saveState() base64 + dock 可见性 + 窗口几何
restoreSession()
读 session.json：先 applyDefault() 建 panel，再 restoreState() 还原拖拽；失败 fallback applyDefault()
预设(JSON, 进版本管理) 与用户 session(字节流, 私有) 职责分离 —— 这是 Unity 没做好、Cakery 做对的地方。Window 菜单加 Layouts ▸：预设 + 用户布局 + Save Layout… + Reset to Default。工具栏右上角加 Layout 下拉。
7.4 EditorWindow 改造
setupDockWidgets() 删除硬编码 7 个 new，改为：
PanelRegistry::self().registerBuiltinPanels();
m_layoutManager->applyDefault();   // JSON 驱动
EditorWindow 仍持各 panel 强指针，改从 m_dockManager->findDockWidget(id)->widget() 查回。

---
8. 菜单与工具栏 JSON 化
8.1 menus.json
{
  "menus": [
    { "path": "File/New Scene",        "command": "scene.new",   "shortcut": "Ctrl+N" },
    { "path": "File/Save",             "command": "scene.save",  "shortcut": "Ctrl+S" },
    { "path": "GameObject/3D Object/Empty", "command": "entity.create", "args": { "preset": "Empty" } },
    { "path": "GameObject/3D Object/Cube",  "command": "entity.create", "args": { "preset": "Cube" } },
    { "path": "GameObject/Light/Directional","command": "component.add", "args": { "component": "DirectionalLightComponent" } },
    { "path": "Component/Physics/Rigidbody","command": "component.add", "args": { "component": "RigidbodyComponent" } },
    { "path": "Assets/Import...",      "command": "asset.import", "args": {} }
  ],
  "toolbar": [
    { "id": "Play",  "command": "playmode.play",  "icon": "icons/play.svg",  "checkable": true, "group": "play" },
    { "id": "Pause", "command": "playmode.pause", "icon": "icons/pause.svg", "checkable": true, "group": "play" },
    { "id": "Stop",  "command": "playmode.stop",  "icon": "icons/stop.svg",  "group": "play" },
    { "separator": true },
    { "id": "Hand",   "command": "gizmo.setmode", "args": { "mode": "None" },     "icon": "icons/hand.svg",  "shortcut": "Q", "checkable": true, "group": "gizmo" },
    { "id": "Move",   "command": "gizmo.setmode", "args": { "mode": "Translate" }, "icon": "icons/move.svg",  "shortcut": "W", "checkable": true, "group": "gizmo" },
    { "id": "Rotate", "command": "gizmo.setmode", "args": { "mode": "Rotate" },    "icon": "icons/rotate.svg","shortcut": "E", "checkable": true, "group": "gizmo" },
    { "id": "Scale",  "command": "gizmo.setmode", "args": { "mode": "Scale" },     "icon": "icons/scale.svg", "shortcut": "R", "checkable": true, "group": "gizmo" },
    { "separator": true },
    { "id": "Layout", "command": "layout.menu", "icon": "icons/layout.svg", "group": "tail" }
  ]
}
8.2 command 名映射
command 字段映射到 v1.1 §16.2 的 CommandRegistry（已设计）。mutating 命令 handler 内部一律构造 ICommand 走 CommandStack → 菜单动作天然带 Undo/Redo（Unity 菜单没有的统一撤销）。若 CommandRegistry 尚未实现，先建最小版（add/execute(name, Json args)）够菜单用。
EditorWindow::setupMenuBar()/setupToolBar() 改为读 menus.json 递归建菜单/工具栏；shortcut 注入 QKeySequence；同 group 的 checkable 按钮互斥。

---
9. 自定义编辑器（Unity CustomEditor）
Unity 自定义编辑器两层：字段级 PropertyDrawer（按字段类型/attribute）+ 组件级 CustomEditor（整组件替换 Inspector）。字段级 v1.1 §5 已设计；组件级本文档补。
9.1 组件级 CustomEditor
// editor/property/CustomEditorRegistry.h   (namespace cakery, 依赖 Qt)
struct InspectorContext {
    EditorContext* ctx;
    dodoe::Uuid    entity;
    std::string    componentName;
    void*          componentPtr = nullptr;
    QWidget*       parent = nullptr;
};

class CustomEditor {
public:
    virtual ~CustomEditor() = default;
    virtual QWidget* build(const InspectorContext& ic) = 0;  // 整组件区（含标题折叠框）
    virtual void refresh(const InspectorContext& ic) = 0;    // 外部变更后只刷新值
};

class CustomEditorRegistry {
public:
    static CustomEditorRegistry& self();
    using Factory = std::function<std::unique_ptr<CustomEditor>()>;
    void registerByName(const std::string& editorName, Factory f);          // C++ 注册工厂
    void mapComponent(const std::string& componentName, const std::string& editorName); // JSON 映射
    std::unique_ptr<CustomEditor> create(const std::string& componentName) const;       // 查 JSON→查工厂
private:
    std::unordered_map<std::string, Factory> m_byName;
    std::unordered_map<std::string, std::string> m_comp2name;  // 来自 inspectors.json
};
CustomEditor 放编辑器侧（依赖 Qt），不入 runtime ComponentDB::Entry，保持 runtime 零 Qt。
9.2 InspectorPanel 接入
auto editor = CustomEditorRegistry::self().create(entry.name);
if (editor) {
    w = editor->build(ic);                   // 自定义整组件编辑器
} else {
    w = buildDefaultComponentGroup(entry);   // 反射遍历 + PropertyDrawer（默认，覆盖 90%）
}
9.3 JSON 映射（呼应用户选的"JSON 映射 + C++ 工厂注册"）
// engine/res/editor/inspectors.json
{
  "customEditors": {
    "TransformComponent": "TransformInspector",
    "CameraComponent":    "CameraInspector"
  }
}
C++ 侧 registerByName("TransformInspector", []{ return std::make_unique<TransformInspector>(); })。create() 先查 JSON 得编辑器名，再查 m_byName 取工厂。改 JSON 可切换/禁用自定义编辑器，无需重编译；新增自定义编辑器才需写 C++。
9.4 示范：TransformInspector
Unity 风格 Transform：三行 Position/Rotation/Scale（每个 3 个 DragSpinBox，已有此 widget），Local/World 切换，联动 Gizmo。编辑走 SetFieldValueCommand。第一个跑通验证整链路。

---
10. 字段级 PropertyDrawer 与 attribute
10.1 FieldAccessor 补 attribute 接口（v1.1 §7.1 签名）
// runtime/core/meta/reflection/reflection.h  FieldAccessor 追加
bool        hasAttribute(const char* key) const;
const char* attribute(const char* key) const;
bool        attributeRange(float& min, float& max) const;
bool        isHidden() const;
bool        isReadOnly() const;
int         enumValues(const char**& outNames, int*& outValues);
落地两步：
- 先（低风险）：JSON 外挂表 inspectors.json 的 fieldAttributes，PropertyDrawerRegistry::create() 查表挂 attribute。立即让 Range/Hidden/Tooltip 可用。
- 后（一劳永逸）：metaparser codegen 透传 META(...) annotation（已解析，见 field.cpp），生成 attribute 字典进 FieldFuncTuple。完成后 JSON 外挂表退为"覆盖"语义（JSON 优先）。
// inspectors.json 的 fieldAttributes（codegen 前的 fallback）
"fieldAttributes": {
  "TransformComponent.rotation":  { "Range": [0, 360], "Tooltip": "欧拉角(度)" },
  "TransformComponent.internalId":{ "Hidden": true }
}
10.2 PropertyDrawer 修复清单
缺口
修复
registerBuiltinDrawers 未调用
EditorContext::boot() 末尾调（第一步，立即让 Inspector 出 widget）
create() attribute 分支 bug
改为：①attribute 命中→对应 drawer；②byType；③复合类型→CompositeDrawer；④兜底只读文本
CompositeDrawer stub
TypeMeta::newMetaFromName(typeName) 取子字段递归，缩进呈现
EnumDrawer 硬编码
读 enumValues()（codegen 后）/ JSON 外挂表（codegen 前）
PPtrDrawer 只读
接 ProjectPanel 拖拽落 FileID，显示资产名
Inspector 全量重建
(组件,字段) diff + updateValue() 增量刷新
字符串跳过内置组件
改用 entry.addable==false + isHidden()

---
11. 落地步骤与验收
与 v1.1 "附:实施顺序建议"对齐，本表聚焦界面形态 + 配置层 + 自定义编辑器。每步完成编辑器须保持可构建可运行。
步
内容
修缺口
验证点
S1
调 registerBuiltinDrawers()；修 create() bug；加兜底只读 drawer
D1
Inspector 选中实体出现字段 widget
S2
Dock 容器视觉修正（§6）：去掉 DisableStylesheet、设 Unity 化 ADS 旗标、紧凑 title bar qss、视口无边框、分割条细化、删 imgui.ini
D12(最不满意)
dock 一眼像 Unity：紧凑深色 tab 条、title bar 矮、视口干净、无残留
S3
EditorConfig 三级加载器 + editor.json/menus.json/inspectors.json/layouts/*.json/panels.json 骨架 + themes/unity-dark.qss
D5(主题)
配置可加载合并；主题可切
S4
PanelRegistry + LayoutManager 落地；EditorWindow 改 JSON 驱动；session 存读；Window 菜单 Layouts
D6,D7
启动呈默认布局；拖拽后重启还原；可切预设
S5
Hierarchy 树形嵌套 + 图标 + 右键菜单 + 拖拽 reparent + 搜索
D2
层级正确、可拖拽改父、可撤销
S6
CustomEditorRegistry + inspectors.json 映射；示范 TransformInspector
D8
Transform 自定义 UI；改 JSON 可切回默认
S7
JSON attribute 外挂表（Range/Hidden/Tooltip）；Inspector 用 addable+Hidden 过滤
D9
字段按 attribute 呈现滑条/隐藏/提示
S8
CompositeDrawer 递归；EnumDrawer 读外挂表；PPtrDrawer 接拖拽
D11
嵌套结构体/枚举/资产拖入可用
S9
Inspector diff + updateValue 增量刷新
优化2
改字段不重建；外部改值回流
S10
菜单/工具栏 JSON 驱动（最小 CommandRegistry + menus.json）；空菜单接线；工具栏图标化分组 + Layout 下拉
D4,D5
菜单可点击产出命令、带 Undo
S11
Project 改 AssetDatabase 驱动 + 缩略图网格 + 双栏 + 拖入场景
D3
资源浏览/拖拽/双击打开
S12
Scene/Game 工具条（Shading/2D/分辨率/Maximize）
v1.1 §14
双视口工具条齐全
S13
codegen 透传 attribute（FieldAccessor §10.1 接口 + generator）；JSON 外挂表退为覆盖语义
D9(完整)
META(Range=...) 直接生效
S14
清理：panel 源文件改 GLOB_RECURSE
—
构建维护性提升
依赖：S1、S2 独立可并行最先做（S2 是当前最不满意点，建议优先可见）。S3 是 S4/S6/S7/S10 基座。S4 不依赖 S5/S6。S5/S11 可并行。S13 依赖 S7 验证过 JSON 路径。
验收（"像 Unity 且配置不写死"的最小达标线）
[] Inspector 选中实体显示字段 widget（S1）
[] dock 视觉像 Unity：紧凑 tab 条、矮 title bar、视口干净、无 imgui 残留（S2，最关键）
[] 面板/布局/菜单/工具栏由 engine/res/editor/*.json 改动生效，无需重编译（S4/S10）
[] 拖拽面板后重启，布局还原；可切换预设（S4）
[] Hierarchy 树形 + 拖拽改父 + 右键菜单（S5）
[] Transform 由自定义编辑器绘制；改 JSON 可切回默认（S6）
[] 字段挂 Range/Hidden/Tooltip 后 Inspector 正确响应（S7/S13）
[] 菜单"GameObject/3D Object/Cube"点击创建实体，Ctrl+Z 可撤销（S10）
[] Project 资源可浏览/拖入场景/双击打开（S11）
[] 主题可切（unity-dark），图标化（S3/S10）

---
本文档与 v1.1 架构文档互补：v1.1 给内核，本文档给界面形态、JSON 配置层与自定义编辑器。如需将任一步骤展开为逐文件实现级代码，可在此基础上继续细化。