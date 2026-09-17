# Cakery 编辑器后续完善计划

本文档承接 [cakery-modify.md](cakery-modify.md) 的差距分析。2026-09 的几轮迭代已经落地了其中的核心闭环,本文记录剩余的交互细节(近期)与工作流方向(中期/远期)。

## 已完成(截至 2026-09)

以下条目已落地,源码为准:

| 能力 | 入口 |
|---|---|
| 命令事务化:`execute` 返回 bool、失败回滚、CompositeCommand 逆序 revert | `core/commands/EditorCommand.cpp`、`CompositeCommand.cpp` |
| 多选:批量删除、多实体 Gizmo 拖拽(平移/旋转/缩放)、Inspector 共同组件编辑(原子 Composite) | `EditorSession::deleteEntities/updateComponentOnEntities`、`RuntimeEditorGizmo.cpp` |
| 历史合并组:`m_mergeGroup` 支持拖拽期间按实体各自合并,一次拖拽一条 undo | `core/history/EditorHistory.cpp` |
| 引擎启动拆分:`Application::startup/stepFrame/teardown`,显式 `bootEngine`,`create_default_view_target` 开关 | `runtime/core/application.cpp`、`RuntimeEditorBackend::bootEngine` |
| 时间缩放对齐 Unity:游戏侧 scaled delta、编辑器 preTick(相机/Gizmo)unscaled delta | `Application::stepFrame` |
| 资产浏览器:多选批量操作、树/网格拖拽移动(.meta 随行保 GUID)、外部文件拖入导入、Duplicate(新 GUID)、删除前引用检查、重命名失败回滚、网格右键菜单 | `cakery/ui/panels/ProjectPanel.cpp` |
| 多资产拖入场景:`scene.import_asset` 循环处理 guid/path 对 | `RuntimeEditorBackend::importDroppedAsset` |
| 实体剪贴板:Copy/Cut/Paste/Duplicate(子树快照 + UUID 重映射),`InsertEntitiesCommand` 保证一条 undo | `EditorSession::copyEntities/pasteEntities/duplicateEntities` |
| Hierarchy 快捷键:Delete/F2/Ctrl+C/X/V/Ctrl+D(WidgetWithChildrenShortcut,不与 Console 冲突) | `HierarchyPanel.cpp` |
| Hierarchy 搜索框(命中自动展开父级,refresh 后保持) | `HierarchyPanel::applyFilter` |
| 拖拽期间延迟 document_changed 全量同步,drag 结束统一通知 | `EditorSession::m_transformDragging` |
| EditHistory 事务 commandCount 通过订阅 history 信号递增 | `core/history/EditHistory.cpp` |
| Prefab 首批:拖 `.prefab` 进场景实例化(marker 常驻 + 实例挂其下),Hierarchy 右键 Save As Prefab 导出选中子树 | `InstantiatePrefabCommand`、`services/PrefabService.cpp`、`HierarchyPanel::onSaveAsPrefab` |
| reconcile 感知 Prefab:展开裸 marker、keep-loop 跳过 `PrefabNodeComponent`、rebuildHierarchy 保留实例节点 | `RuntimeEditorScene.cpp` |
| Inspector 组件操作:Copy / Paste Component Values / Reset to Default / Move Up / Move Down | `InspectorPanel.cpp`、`MoveComponentCommand` |
| Gizmo 吸附:`scene_mouse_move/down` 携带修饰键,拖拽按步长取整(0.25 / 15° / 0.1),工具栏吸附开关与 T/R/S 步长 | `RuntimeEditorGizmo::updateDrag`、`EditorWindow::createDocks` |
| Play 编辑保护:EditorSession 感知 `play_state_changed`,进 Play 快照文档,Stop 弹出丢弃/保留 | `EditorSession::onPlayStateChanged/stopPlay` |
| 孤儿资产引用扫描:资产库 finalize/打开文档时扫描,Console 提示缺失数量,Inspector 显示 "Missing" | `RuntimeEditorBackend::reportMissingAssetReferences`、`EditorJsonWidget::buildAssetReferenceField` |
| Ctrl+A 全选:Hierarchy 仅可见项 / Project 树与网格 | `HierarchyPanel::onSelectAll`、`ProjectPanel` |

## P1 近期:编辑交互细节

1. **Gizmo 吸附**

   - [x] `scene_mouse_move` payload 已扩展 `x,y,ctrl,shift,alt`,`scene_mouse_down` 追加 `ctrl,shift`;
   - [x] `RuntimeEditorGizmo::updateDrag` 平移/旋转/缩放按步长取整(默认 0.25 / 15° / 0.1),开关开启或按住 Ctrl 生效;
   - [x] 场景工具栏吸附开关与 T/R/S 步长输入(`gizmo_snap` / `gizmo_snap_step`);SettingsPanel 接入可选。

2. **Play 模式编辑保护**

   - [x] `EditorSession::PlayState` 感知后端 `play_state_changed`,编辑器 UI 据此判定;
   - [x] 进入 Play 前快照 `EditorDocument`,Play 期间标记文档改动;
   - [x] Stop 时若文档有改动,弹出"丢弃运行期改动/保留/取消",按选择恢复快照(清空历史)或重同步。

3. **Inspector 组件操作**

   - [x] 组件上下文菜单:Copy Component / Paste Component Values / Reset to Default / Move Up / Move Down / Remove;
   - [x] 组件折叠状态在 refresh 后保持(`m_componentExpanded`,多实体切换按索引保持)。

4. **框选与全选**

   - [ ] 视口橡皮筋框选:需要 `ReadObjectIdBuffer` 真实实现(当前返回空实体,`picking_backend.cpp:121`);渲染侧已有 pick pass,但仅 Debug 路径暴露,仍需渲染/编辑器接口打通;
   - [x] Hierarchy Ctrl+A(仅选中可见项)与 Project 树/网格 Ctrl+A。

5. **多选体验**

   - [ ] mixed value 显示(多实体同名字段值不同时显示"—",任一输入后统一写入,现有原子 Composite 已支持写入侧);
   - [ ] Pivot/Center、Local/World gizmo 切换(工具栏 toggle + `beginDrag` 数学分支);
   - [ ] 实体隐藏/锁定(Hierarchy 图标列 + `ActiveComponent`/新增 `LockedComponent`,过滤视口拾取)。

6. **资产引用重定向**

   - [x] 移动/重命名依赖 `.meta` 随行保 GUID;当 meta 丢失或手工移动导致断链时,`asset.import` 会生成新 GUID;
   - [x] 启动/刷新(资产库 finalize)时扫描文档中不在 AssetDatabase 的 `asset_id`,Inspector 资产字段显示 "Missing" 警告图标并可重新指定;缺失数量会输出到 Console。

## P2 中期:工作流闭环

1. **Prefab 实例化 UI**(关联 [dodoe-prefab-system.md](dodoe-prefab-system.md))

   - [x] 拖 `.prefab` 资产进场景生成实例实体(挂 `PrefabInstanceComponent` 引用源资产);
   - [x] Hierarchy 右键 Save As Prefab 导出选中子树为 `.prefab` 资产;
   - [ ] Hierarchy 中实例节点显示标记、Override 高亮、Apply/Revert 菜单;
   - [ ] 双击 prefab 资产进入隔离编辑模式(临时文档切换);
   - [ ] 实例子节点在 Hierarchy 展开显示(PrefabNodeComponent 树投影到文档视图)。

2. **场景可靠性与多文档**

   - 场景 dirty 状态已有 `EditorDocumentModel::m_dirty`,补:关闭前未保存确认、自动保存(定时 + 崩溃恢复副本)、原子写入(QSaveFile 先写临时再替换);
   - 多场景/Scene Tab:`openDocument` 支持并行文档,Tab 切换时保存/恢复各文档的 selection + history。

3. **EditorConfig 真正接入**

   - 布局保存/恢复(qt-ads `savePerspective`/`loadPerspective` 接入 `Reset Layout` 空实现处);
   - 菜单/快捷键从 `Configs/*.json` 驱动,替代 EditorWindow 硬编码。

4. **Game View 分离**

   - Play 时独立 Game View 窗口(第二个 RenderViewTarget,游戏相机而非编辑器相机);
   - 分辨率预设、Aspect 比例锁定、Stats overlay。

## P3 远期

- Profiler 面板(tracy 已集成 runtime 侧,编辑器侧接 UI)、Frame Debugger;
- Animation/Timeline、Material/Shader 图形化编辑器;
- C# 脚本组件 Inspector + 热重载(关联 [srp-csharp-api-design.md](srp-csharp-api-design.md));
- Build Profile 与一键打包(关联 [dodoe-network-module.md](dodoe-network-module.md) 之外的独立交付链路);
- 编辑器自动化测试(面板行为、命令回放)。

## 大方向路线图(2026-09 评审)

基于现状的六条主线,依赖关系与并行性如下:

| # | 方向 | 现有基础 | 依赖 |
|---|---|---|---|
| 1 | Prefab 闭环 | runtime 侧完整:`SceneImporter::InstantiatePrefab`、`PrefabInstanceComponent`、导出 API、`.prefab` 资产导入(`asset_manager.cpp:319`);编辑器侧仅缺接入 | 资产浏览器/引用链(已就绪) |
| 2 | 反射 Inspector 收尾 | `inspectComponent` 已返回 Range/Tooltip/AssetHandle 元数据;`registerDefaults` 已有 Camera/Rigidbody 条目 + EditorConfig JSON 覆盖;缺常用组件批量补齐、专用控件、组件操作菜单 | 无,可独立 |
| 3 | Play/编辑双态分离 + Game View | `RenderViewManager` 多 target、`create_default_view_target` 开关已铺路 | 渲染侧配合 |
| 4 | C# 脚本工作流 | `ScriptSystem`、`script.tool_action`、SRP API plan | runtime 侧 |
| 5 | 性能/调试面板 | tracy 已集成、Console + 命令注册表已有 | 低,可当间隙任务 |
| 6 | 工程化收尾 | qt-ads、EditorConfig 雏形 | 最后 |

执行顺序:**1 与 2 并行**(Prefab 靠编辑器架构,反射 Inspector 靠字段注册,互不阻塞)→ 3 紧随 → 4/5 按需 → 6 收尾。

**Prefab 编辑器接入的三个切入(方向 1 首批)**:

- 拖 `.prefab` 资产进场景:`importDroppedAsset` 增加 `.prefab` 分支,实例化并镜像到文档(参照 `ImportTiledMapCommand` 模式);
- Hierarchy 右键"Save As Prefab...":选中子树导出为 `.prefab` 资产(runtime 导出 API 已有);
- Hierarchy 中 `PrefabInstanceComponent` 实例节点标记与 Revert(读 prefab 源 SceneRes 对比,后续)。

**反射 Inspector 首批(方向 2)**:

- 常用组件字段属性补齐(Light/Sprite/TileLayer/AudioSource 等的 Range/Tooltip);
- 组件头部上下文菜单:Copy Component / Paste Component Values / Reset(默认值来自组件注册表)/ Remove;
- 组件上下移动(需 MoveComponentCommand,注意索引失效)。


