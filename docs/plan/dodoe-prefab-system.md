# Prefab 系统方案（Level 2：链接式实例，Unity 同款）

状态： 设计中
日期： 2026-08-13
范围： Dodoe runtime、资源系统、场景序列化、Cakery 编辑器

## 1. 背景与目标

引擎已有完整组件序列化链路（`SceneRes` + `ComponentDB` + `Serializer`）与实体实例化模板（`Scene::deserialize`、`DeleteEntityCommand::undo` 的建树+回链）。`AssetType::Prefab` 枚举、`.prefab` 扩展名映射、`assetTypeToString/FromString` 均已定义，但**无资产实现、无 importer、无 `createAssetInstance` 分支、无扫描分支**。

需求：Prefab 做到**链接式（Unity 同款）**：

1. 场景里的实例带一个到源 prefab 资产的引用 + 一份**覆盖（override）列表**；
2. 编辑 prefab 资产 → 变更传播到所有实例；
3. 实例上被用户改过的字段（override）在传播时保留；
4. 覆盖按**字段级（property path）** diff/merge，与 Unity 逐字段合并语义一致。

`PrefabAsset` 复用 `SceneRes` 做序列化载荷，零新增场景序列化格式。实例化复用 `DeleteEntityCommand::undo` 的回链模式。节点稳定身份 = prefab 资产的实体 UUID。

## 2. 核心模型

### 2.1 持久化标记组件（真实 ComponentDB 组件，随 EntityRes 自动序列化）

```
PrefabNodeComponent                      // 实例的每个实体（含根）
  UUID instance_id;          // 实例根实体的场景 UUID（分组/快速查找）
  UUID prefab_entity_uuid;   // 该节点在 prefab 资产里的稳定身份（Unity 的 m_FileID）

PrefabInstanceComponent                  // 仅实例根实体
  AssetHandle<PrefabAsset> source_prefab;      // 引用源 prefab（AssetHandle 已有序列化分支 {asset_id, sub_object_id}）
  std::vector<PrefabOverride> overrides;       // 相对 prefab 基态的字段级差异
```

两者注册进 `ComponentDB::registerBuiltinComponents`（`addable=false`），并在 `InspectorPanel` 中隐藏（与 `IDComponent`/`TagComponent` 同一 skip 列表）。

### 2.2 覆盖记录

```
PrefabOverride
  UUID node;             // 目标 prefab 节点 UUID
  String component_type; // "" = 实体级（名字）；否则组件类型名
  String path;           // JSON 路径："" = 整组件；"*add"/"*remove" = 增删组件
  String value;          // JSON 编码的值（String 承载，避开反射对 Json 字段的限制；与 ComponentRes.m_component 同构）
```

### 2.3 序列化模型（全量数据 + 覆盖双存）

- **实例实体照常完整序列化**（现有 `SerializeNativeComponents`/`SerializeManagedComponents` 不变）——场景文件包含实例完整组件数据 + 标记组件 + 根上的 overrides。
- **覆盖在保存时用 diff 自动计算**，不做运行时属性追踪：
  - `Scene::serialize()` 不变；`Scene::save()` 在 `saveAssetFile` 前调用 `PrefabService::CaptureInstanceOverrides(*this, res)`，把每个实例的 overrides 填进根实体 EntityRes 的 `PrefabInstanceComponent`。两遍处理：先正常序列化，再回填根节点该 ComponentRes。
  - diff：实例组件 JSON 与 prefab 基态节点组件 JSON 逐字段递归比较，只产出真正不同的叶子路径 → 改一个字段只存一个 override → prefab 改其他字段仍能合并进来。
- **载入**：按现有 `deserialize` 完整载入，实例不依赖 prefab 也能独立呈现；标记组件随 EntityRes 还原。
- **传播（Sync）**：`merged = 新基态组件 JSON 上按 path 应用 overrides`，`merged != 当前实例组件` 时经 `ComponentDB.readJson` 写回并 markDirty → 自愈一致。

## 3. 文件改动

### 3.1 资产层

- `engine/src/runtime/resource/asset/types/prefab_asset.h/.cpp` — 仿 `SceneAsset`：持 `SceneRes m_res`，`loadFromSource` 读 `.prefab` JSON → `Serializer::read`，`saveToSource` 写回；`kStaticType = AssetType::Prefab`；非只读。
- `engine/src/runtime/resource/asset/asset_manager.h` — include `types/prefab_asset.h`。
- `engine/src/runtime/resource/asset/asset_manager.cpp`：
  - `kPrefabExt = ".prefab"` 常量（放 `kSceneExt` 旁）。
  - `createAssetInstance`（:81-91）加 `case AssetType::Prefab: return create_scope<PrefabAsset>();`。
  - `loadAssets()` 目录扫描（:331-360）加 `else if (ext == kPrefabExt)` 分支，形态与 `.doscn` 分支一致（`registerAsset` → 创建 `PrefabAsset` → `loadFromSource` → 存 `m_assets`）。重启路径已由 DB 重建循环（:294-307）自动覆盖。
  - `ensureImported` 放行 `.prefab`：扩展名校验加入 `kPrefabExt`；未注册则走直接注册路径（`registerAsset` + 创建 `PrefabAsset` + `loadFromSource`），已注册则直接 `resolvePathToRef`。Save As Prefab 与拖入共用此入口。

### 3.2 组件

- `engine/src/runtime/function/world/components/prefab_node_component.h`
- `engine/src/runtime/function/world/components/prefab_instance_component.h`（含 `PrefabOverride`，或独立头）
  - `STRUCT` + `META(Enable)` 字段；metaparser 递归扫描 `src`，自动生成序列化特化（CMake `GLOB_RECURSE CONFIGURE_DEPENDS` 已覆盖新头文件）。
- `engine/src/runtime/core/meta/component_db.cpp` `registerBuiltinComponents` — 注册两个标记组件（`addable=false`）。
- `engine/src/editor/cakery/panels/InspectorPanel.cpp` — 组件枚举处把两个 prefab 标记名加入隐藏 skip。

### 3.3 服务 `engine/src/runtime/service/world/prefab_service.h/.cpp`

- `Instantiate(scene, source_prefab, new_root_uuid, name)`：
  1. `loadAssetSync<PrefabAsset>` 取 `SceneRes`，建 prefab_uuid → EntityRes 映射；
  2. 为每个节点生成**新场景 UUID**，`scene.createEntity`（自带 IDComponent，identity 用场景 UUID，**跳过基态 IDComponent**）；组件经 `DeserializeNativeComponents`/`DeserializeManagedComponents` 载入（managed 组件按场景 uuid 回填，天然可移植）；
  3. 每个实体加 `PrefabNodeComponent{instance_id = 根场景UUID, prefab_entity_uuid}`，根加 `PrefabInstanceComponent{source_prefab}`；
  4. 回链（`DeleteEntityCommand::undo` 模式）：读 `HierarchyComponent.parent_uuid`（prefab 空间）→ 映射成场景实体 → `AttachChild`。
- `SaveAsPrefab(scene, root, name)`：`CollectSubtree` → 每节点分配新 prefab UUID → 构建 SceneRes（过滤 `IDComponent`/`TagComponent`/两个标记组件；HierarchyComponent 的 `parent_uuid` 改写为 prefab 空间：子树内父 → 映射 UUID，子树外/根 → 0）→ `saveAssetFile(res, "Prefabs/<name>.prefab")` → `ensureImported` 得 ObjectID → 把现有子树原地转成实例（补标记）→ 返回 ObjectID。
- `CaptureInstanceOverrides(scene, scene_res)`：遍历带 `PrefabInstanceComponent` 的根 → 载入基态 → 子树逐节点逐组件 diff（含 managed）→ 回填 overrides。
- `Sync(scene)` / `SyncPrefab(scene, prefab_ref)`：载入新基态，按 overrides 合并，写回实例实体。
- `Revert(scene, instance_root)`：清 overrides、把实例实体恢复为基态。
- `Unpack(scene, instance_root)`：移除所有标记组件，实体保留。
- JSON diff/apply/merge 助手（TU 内匿名空间）：`diffJson(base, inst, prefix)` 递归产出叶子差异；`setAtPath(Json&, path, value)`；`mergeJson(base, overrides)`。`"*add"`/`"*remove"` 处理组件增删。

### 3.4 场景

- `engine/src/runtime/function/world/scene.cpp` `save()` — `serialize()` 后调用 `PrefabService::CaptureInstanceOverrides`。`deserialize()` 不变。

### 3.5 导入

- `engine/src/runtime/service/world/scene_importer.cpp` — `ImportAsset` 派发加 `.prefab` → `ImportPrefab(path)`（`ensureImported` → `PrefabService::Instantiate` 到活动场景，形态与 `ImportModel` 一致）。

### 3.6 编辑器

- `engine/src/editor/framework/command/commands/InstantiatePrefabCommand.h/.cpp` — 可撤销实例化（execute 建实例，undo 销毁实例子树）。
- `engine/src/editor/framework/console/builtin_commands.cpp` — 注册 `prefab.save`（用 `primarySelection`；注意 `executeStructured` 只填 `args.raw`，菜单 args 到不了 handler，故靠选中实体）、`prefab.sync`、`prefab.revert`、`prefab.unpack`。
- `engine/src/editor/cakery/panels/HierarchyPanel.cpp` — `onCustomContextMenu`（:222-262）在有效选中分支加 "Prefab/Save As Prefab"；选中实体带 `PrefabInstanceComponent` 时再加 "Prefab/Revert / Sync / Unpack"。
- `engine/res/editor/menus.json` — `GameObject/Prefab/Save As Prefab` → `prefab.save`。
- 场景打开后 `Sync`：编辑器 `scene.open`/载入路径调 `PrefabService::Sync(scene)`（也可挂 prefab 资产重导入后）。

## 4. 实施顺序

1. **Milestone 1 — 资产 + 组件 + 实例化**：PrefabAsset；asset_manager 四处接线；两个标记组件 + ComponentDB 注册 + Inspector 隐藏；PrefabService::Instantiate；SceneImporter::ImportPrefab。
2. **Milestone 2 — 覆盖 diff/merge + 保存回填**：PrefabOverride + JSON diff/apply/merge；Scene::save 调 CaptureInstanceOverrides；Revert。
3. **Milestone 3 — 编辑闭环**：SaveAsPrefab；Sync；HierarchyPanel 菜单 + builtin_commands 注册 + menus.json；场景打开/资产变更触发 Sync。

## 5. 边界与风险

- **结构化树变更不同步**：v1 的 Sync 只按节点 UUID 对齐、同步组件/名字/变换字段值；prefab 增删节点需重新拖入实例化（完整结构同步留后续）。
- **丢失 prefab 资产**：Capture 载入失败 → overrides 清空，场景仍完整可载入（实例"失联但可用"）。
- **实例内组件引用场景本地实体**（如动画引用其他场景实体）：存进 prefab 会变空引用，Save As Prefab 不拦截，行为与 Unity 一致地"警告但不阻止"。
- **UUID JSON 是 uint64 数字**（serializer.cpp:121-130）；HierarchyComponent 仅序列化 `parent_uuid`+`child_count`，`parent`/`children` 句柄需回链重建。
- **首次保存场景时 overrides 为空的实例**：diff 为空，随后编辑经下次保存捕获。
- **执行序**：serialize 两遍回填在 `Scene::save()`，不侵入 `serialize()` 本身。
