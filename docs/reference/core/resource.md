# Resource

## 目录

`engine/src/runtime/resource/`

| 子模块 | 关键类型 |
|---|---|
| `resource_manager.*` | `ResourceManager`、资产加载与关闭 |
| `asset/*` | `Asset`、`AssetManager`、`AssetDatabase`、`AssetHandle`、importer、asset types |
| `file/*` | `FileID`、`FileSystem`、路径与资源标识 |
| `res_type/*` | `SceneRes`、`EntityRes`、`ComponentRes` |
| `parser/*` | texture、mesh 二进制解析 |

## Asset

`Asset` 包含 UUID、source path、type、load state、依赖与 metadata。`AssetType` 覆盖 Texture、Sprite、Mesh、Material、Scene、Prefab、Tileset、Animation 等类型。

`AssetManager` 管理 asset database、path 到 UUID 的映射、已加载 asset、同步/异步加载、导入、刷新、重导入、保存和依赖查询。`AssetHandle<T>` 使用 `ObjectID` 引用资产与 sub-object，不持有裸 `Asset*`。

`asset/types/*` 定义 texture、sprite、mesh、material、scene、tileset、animation 等资源对象。`SceneAsset` 持有 `SceneRes` 并读写场景 source。

## Import

`AssetImporter` 是 importer 基类。texture、sprite、model importer 生成或更新 import metadata 与 asset database 条目。`ImportSettingsIO` 读写 import settings。

## 文件与序列化资源

`FileSystem` 提供 engine/project/resource 路径；`FileID` 表示资源文件与 UUID。`SceneRes` 包含场景名称和 `EntityRes[]`；`EntityRes` 包含 UUID、名称、native component 与 managed component 载荷；`ComponentRes` 保存类型名与 JSON payload。

## Project

`engine/src/runtime/core/project/project.*` 定义 `Project`、project directory、asset directory、binaries directory 和 script assembly path。`ProjectSerializer` 读写 `.doproj` 中的 Name、ProjectPath、AssetDirectory、StartSceneName。
