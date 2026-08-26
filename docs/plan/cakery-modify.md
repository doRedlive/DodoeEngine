静态分析结论：当前 Cakery 已经是一个“可运行的编辑器原型 + Runtime 垂直切片”，但距离 Unity/Unreal/Godot 这类专业引擎的编辑器，还缺少大量生产级工作流。核心问题不是某一个控件没做，而是“文档模型、资产系统、反射 Inspector、命令系统、编辑器 UI”还没有真正闭环。

另外，交接文档里说 Gizmo 拖拽尚未实现，但当前源码已经有 `beginDrag/updateDrag/endDrag` 和 Transform 回写逻辑，因此这部分文档已经过时，应以源码为准。

**当前已经具备的基础**

- Editor Core：Session、文档模型、选择、Undo/Redo、命令和基础 JSON 序列化。
- Runtime Backend：编辑器相机、视口挂载、场景同步、点选、平移/旋转/缩放 Gizmo、Play/Pause/Stop。
- Qt 工作区：Scene、Hierarchy、Inspector、Project、Console、History 面板。
- Editor-Only 产品：`NullEditorBackend` 可以在不加载 Runtime 的情况下编辑场景。
- Runtime 侧已有 AssetDatabase、TilePaint、Reparent 命令、AICommandBridge 等能力雏形。

**P0：最影响“专业可用性”的缺口**

1. **场景层级仍然是扁平列表**

`EditorEntity` 有 `parent` 字段，但 HierarchyPanel 把所有实体都作为顶层节点加入，实际上没有树状层级、拖拽重父、展开折叠或父子选择。[HierarchyPanel.cpp:45](C:/Users/33235/Redlive/dodoe/engine/src/editor/cakery/ui/panels/HierarchyPanel.cpp:45)

需要补齐：

- 真正的父子树结构；
- 拖拽 Reparent；
- 防止父节点设为自己的子孙；
- 保持运行时 HierarchyComponent 和文档同步；
- 子树复制、删除、移动；
- 层级搜索、隐藏、锁定、标签和排序。

当前虽然存在 `ReparentEntityCommand`，但没有对应的可用 UI 工作流。

2. **多选状态存在，但 UI 仍然只支持单选**

`EditorSelection` 已经有 `selectMany/selectedAll`，但 HierarchyPanel 只调用单选接口，Inspector 和 Gizmo 也只围绕一个 primary entity 工作。[EditorSelection.h:40](C:/Users/33235/Redlive/dodoe/engine/src/editor/core/EditorSelection.h:40)

专业编辑器必须支持：

- Ctrl/Shift 多选；
- 框选；
- 多对象一起移动、旋转、缩放；
- Pivot/Center、Local/World；
- 多选 Inspector 的 mixed value；
- 多对象 Undo 合并。

3. **Inspector 还是 JSON 编辑器，不是反射驱动属性系统**

Inspector 只遍历 `nativeComponents`，组件字段由通用 `EditorJsonWidget` 动态生成。[InspectorPanel.cpp:156](C:/Users/33235/Redlive/dodoe/engine/src/editor/cakery/ui/panels/InspectorPanel.cpp:156)

目前缺少：

- C++/C# 反射元数据自动生成 Inspector；
- managed component 的编辑；
- 枚举、Flags、AssetHandle、UUID 引用；
- LayerMask、颜色、曲线、Gradient、Object picker；
- Range、Tooltip、Hidden、ReadOnly 等属性真正生效；
- 组件校验和错误提示；
- 组件 Reset、Copy/Paste、Paste Values；
- 多选字段的 mixed 状态；
- 资源拖拽到字段。

`FieldAttributeRegistry::registerDefaults()` 目前为空，说明属性系统还未形成完整注册流程。[FieldAttributes.cpp:17](C:/Users/33235/Redlive/dodoe/engine/src/editor/adapters/runtime/services/FieldAttributes.cpp:17)

4. **资产数据库已经存在，但没有形成 Content Browser**

`AssetDatabase` 目前主要只有刷新、按类型过滤、GUID 查询和 Save Dirty。[AssetDatabase.cpp:8](C:/Users/33235/Redlive/dodoe/engine/src/editor/adapters/runtime/services/AssetDatabase.cpp:8)

ProjectPanel 实际上只是递归扫描文件系统并显示文件树，不是资产浏览器。[ProjectPanel.cpp:42](C:/Users/33235/Redlive/dodoe/engine/src/editor/cakery/ui/panels/ProjectPanel.cpp:42)

缺少：

- 缩略图和预览；
- 按类型、标签、名称、GUID 搜索；
- 资产拖拽到 Scene 和 Inspector；
- 资产重命名、移动、删除；
- Import Settings 面板；
- Reimport、依赖关系、缺失资源检测；
- 文件系统监听；
- 异步导入和导入进度；
- 资源版本/缓存失效处理；
- Mesh、Texture、Material、Sprite、Animation、Tileset 等专用预览器。

更直接的问题是，`asset.import` 命令目前直接返回成功，但没有执行任何导入操作。[builtin_commands.cpp:324](C:/Users/33235/Redlive/dodoe/engine/src/editor/core/commands/builtin_commands.cpp:324)

5. **编辑器配置系统没有真正接入 UI**

`EditorConfig` 支持菜单、面板、Inspector、布局和快捷键配置，但源码中没有看到应用启动时加载它的流程；EditorWindow 仍然硬编码菜单、工具栏和面板。[EditorConfig.cpp:16](C:/Users/33235/Redlive/dodoe/engine/src/editor/services/EditorConfig.cpp:16) [EditorWindow.cpp:300](C:/Users/33235/Redlive/dodoe/engine/src/editor/cakery/ui/shell/EditorWindow.cpp:300)

明确的断点包括：

- `Reset Layout` 是空 lambda，没有实际恢复布局。[EditorWindow.cpp:365](C:/Users/33235/Redlive/dodoe/engine/src/editor/cakery/ui/shell/EditorWindow.cpp:365)
- `menus.json` 中的命令没有自动生成菜单；
- `shortcuts` 配置没有统一注册；
- `panels.json` 中引用了许多不存在或未注册的面板图标；
- ProjectSettings/Editor 覆盖配置没有完整接入；
- 没有用户级布局保存、布局版本迁移和多工作区。

**P1：专业场景编辑能力**

- 透视/正交、2D/3D、相机书签、F 聚焦、Frame Selected；
- 网格、吸附、角度吸附、缩放吸附；
- Local/World、Pivot/Center、坐标轴约束；
- 线框、实体、材质预览、碰撞体和光照可视化；
- 框选、穿透选择、选择循环；
- 真实对象 ID picking。

当前 `ReadObjectIdBuffer` 直接返回空实体，因此点选主要依赖简化 AABB 射线测试。[picking_backend.cpp:121](C:/Users/33235/Redlive/dodoe/engine/src/runtime/service/editor/picking_backend.cpp:121)

此外，`DebugDraw::Flush()` 目前只是清空缓存，并没有提交到渲染管线。[debug_draw.cpp:100](C:/Users/33235/Redlive/dodoe/engine/src/runtime/service/editor/debug_draw.cpp:100)

还需要补齐：

- Prefab、Prefab Variant、嵌套 Prefab、Override；
- 多场景编辑、Additive Scene、Scene Tab；
- Scene Template、Entity Template、批量替换；
- Tilemap Scene 编辑器和 Tile Palette；
- UI 布局编辑器；
- Animation/Animator/Timeline 编辑器；
- Material/Shader 参数和节点编辑器；
- Particle、NavMesh、Physics、Audio 等专用工具。

**P1：Play Mode 和调试闭环**

当前 Play/Pause/Stop 已经有基础实现，但还不够专业：

- 单帧执行、逐帧暂停；
- Restart Scene；
- Runtime Hierarchy；
- Runtime Inspector；
- 编辑态与运行态差异查看；
- 运行时实体/组件变化追踪；
- 输入模拟；
- Game View；
- 多窗口/多分辨率预览；
- 远程调试；
- Console 命令输入；
- 性能 Profiler、内存统计、渲染统计；
- Frame Debugger；
- GPU/CPU 时间线。

当前 Console 主要展示静态诊断文本，Terminal 也是占位面板，不是完整调试工具。

**P1：文档、保存和数据可靠性**

当前场景保存仍然偏原型级：

- 没有 dirty 状态；
- 没有关闭前未保存确认；
- 没有自动保存和崩溃恢复；
- 没有外部文件变化检测；
- 没有原子写入和备份文件；
- 没有多文档 Tab；
- 没有文档锁和冲突解决；
- `Save As` 后没有更新 `EditorDocumentModel::m_path`，后续普通保存可能仍指向旧路径。[EditorDocumentModel.cpp:34](C:/Users/33235/Redlive/dodoe/engine/src/editor/core/document/EditorDocumentModel.cpp:34)

序列化也缺少：

- 文件版本号；
- Schema 校验；
- UUID 重复检查；
- parent 是否存在的检查；
- parent cycle 检查；
- 缺失组件类型处理策略；
- 迁移脚本；
- 错误位置和用户可读诊断。

`fromJson` 对很多格式错误会直接生成空文档或部分文档，而不是拒绝加载。[EditorDocumentSerializer.cpp:66](C:/Users/33235/Redlive/dodoe/engine/src/editor/core/document/EditorDocumentSerializer.cpp:66)

**P1：Undo/Redo 和命令系统仍不完整**

Undo/Redo 基础结构已经有，但还需要：

- 所有编辑操作统一命令化；
- 多对象操作使用 CompositeCommand；
- 拖拽、字段编辑、资源操作的连续合并；
- 命令失败回滚；
- 事务日志；
- 命令标签和可视化详情；
- 跨资源 Undo；
- Save Point dirty 判断。

`EditHistory` 的 `m_commandCount` 被初始化和提交，但没有实际递增，因此事务记录中的 commandCount 会失真。[EditHistory.cpp:20](C:/Users/33235/Redlive/dodoe/engine/src/editor/core/history/EditHistory.cpp:20)

AI/命令系统还有两个具体问题：

- `executeStructured` 只设置了 `cargs.raw`，没有填充 `named/positional`，很多结构化命令无法读取参数。[CommandRegistry.cpp:63](C:/Users/33235/Redlive/dodoe/engine/src/editor/core/console/CommandRegistry.cpp:63)
- `AICommandBridge::query` 没有强制只读，理论上可以执行修改型命令。

**P2：脚本和工程生产能力**

如果要成为真正可用的游戏引擎，还需要：

- 内置脚本编辑器或外部 IDE 集成；
- C# 编译、热重载、脚本错误显示；
- 断点、调用栈、变量查看；
- Build Profile；
- 平台、分辨率、输入、图形设置；
- 一键运行、打包、部署；
- Asset Validation、Scene Validation；
- Source Control 集成；
- 资源冲突和合并工具；
- 插件/扩展 API；
- 编辑器自动化测试；
- Editor UI 测试和回归测试。

目前测试目录里主要是游戏项目和资源，没有形成编辑器行为测试体系。

**建议的实施顺序**

1. 先建立可靠的 `EditorDocument + Command + Dirty + Save` 核心。
2. 把 `EditorConfig`、菜单、快捷键、布局和资源 manifest 真正接入。
3. 完成层级树、Reparent、多选和基本 Scene View 操作。
4. 用反射元数据重做 Inspector，覆盖 managed/native component 和 AssetHandle。
5. 把 AssetDatabase 做成真正的 Content Browser，并打通 Import/Reimport/拖拽。
6. 完成 Prefab、Tilemap、Animation、UI 等内容生产工具。
7. 最后补齐 Profiler、Debugger、Build、VCS、插件和自动化测试。

当前最值得优先投入的不是继续增加零散面板，而是完成这四条闭环：

`资源导入 → 资产引用 → Inspector 编辑 → 场景保存`

`Hierarchy → Selection → Gizmo → Undo → 文档同步`

`命令注册 → 菜单/快捷键 → 执行 → 诊断`

`Play Mode → Runtime 状态 → 调试查看 → Stop 恢复`

在这四条闭环完成之前，Cakery 更适合称为“引擎编辑器原型”，还不能称为专业游戏引擎编辑器。以上结论基于源码和文档静态检查，未进行编译或运行验证。