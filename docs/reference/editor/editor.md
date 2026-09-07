# Editor

Qt 外壳、对象生命周期、事件流和排查方法见：[Cakery Qt 架构说明](cakery-qt-architecture.md)。

## 目录

| 目录 | 关键类型 |
|---|---|
| `engine/src/editor/core/` | `EditorSession`、selection、signal、command、history、document、console |
| `engine/src/editor/bridge/` | `IEditorBackend`、capability、status、command/event、viewport descriptors |
| `engine/src/editor/adapters/null/` | `NullEditorBackend` |
| `engine/src/editor/adapters/runtime/` | `RuntimeEditorBackend`、`EditorCamera`、tile/asset/UUID services、runtime commands |
| `engine/src/editor/cakery/app/` | `EditorApplication`、project selection |
| `engine/src/editor/cakery/ui/` | workspace、window、Hierarchy、Inspector、Project panel、JSON widget |
| `engine/src/editor/products/` | Cakery 与 CakeryPreview 进程入口 |
| `engine/src/editor/services/` | editor config、resource locator |

## Session 与 Document

`EditorSession` 持有 `IEditorBackend`、`ProjectDescriptor`、`EditorDocumentModel`、`EditorSelection`、`EditorHistory`、`EditHistory`。它执行 open/save document、editor command、scene surface attach、viewport metrics、undo/redo 和 backend event 处理。

`EditorDocumentModel` 保存 EditorEntity、native component、managed component 和层级文档数据。`EditorDocumentSerializer` 负责文档 JSON 读写。`EditorCommand`、`CompositeCommand`、`EditorHistory`、`EditHistory` 管理可撤销变更。

## Backend Contract

`IEditorBackend` 包含：

- capability、status、diagnostic；
- project/document open；
- command execute 与 backend event callback；
- scene surface attach/detach、viewport resize、safe-point tick；
- shutdown。

`BackendCapabilities` 区分 document read/write、scene preview、simulation。`BackendEventMessage` 将 backend 事件传回 EditorSession。

## Runtime Backend

`RuntimeEditorBackend` 创建 runtime application、editor camera、scene view target，并将 EditorDocument 同步到 active scene。它处理 scene surface 输入、viewport metrics、picking、gizmo、play/pause/resume/stop 和 runtime safe-point tick。

selection 由 ray cast 结果发送为 `selection_changed`。gizmo transform 由 `transform_changed` 事件回写 EditorDocument。Play 保存 `SceneRes` snapshot，Stop 恢复 snapshot。

## Null Backend 与 UI

`NullEditorBackend` 提供 project/document authoring 状态；scene preview 与 simulation capability 关闭。

`EditorApplication` 创建 session、workspace、resource locator、project window 和 editor window。Hierarchy、Inspector、Project panel 通过 workspace/session 操作文档和命令。Qt UI 不直接持有 World、RenderSystem 或 runtime backend 私有对象。
