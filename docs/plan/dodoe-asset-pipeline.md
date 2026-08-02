# dodoe 资产管线（Asset Pipeline）设计 —— 源文件 → 资产 → 运行时对象

文档版本：v1.0 · 2026-08-02
引擎：dodoe runtime（DX12 / Deferred / DualThread）· Qt6 编辑器（Cakery）· C++20 · nlohmann/json
关联文档：[dodoe-render-architecture-roadmap.md](./dodoe-render-architecture-roadmap.md)（渲染架构）· [dodoe-editor-unity-like.md](./dodoe-editor-unity-like.md)（编辑器资源浏览器 D3）

## 1. 为什么要资产管线

**问题**：源文件 ≠ 资产。一张 `foo.png` 本身只是源文件；要成为引擎能引用、能带导入参数、能一致解析的东西，需要"导入"这一步。当前引擎没有这层，导致：

1. **同一张图无法表达多种用途/参数**。`foo.png` 只能当 texture；想当"ppu=100 的 sprite"、"ppu=50 的 sprite"、九宫格切图的 sprite，没有地方存这些设置。`Sprite` 运行时对象上 `pixelsPerUnit` 目前只能写死 100，改设置无持久化。
2. **运行时直接读源文件**。`Texture2D::Load(path)` 每次 `TextureBlob` 重新解析磁盘文件（[texture_manager.cpp:104-110](engine/src/runtime/function/render/texture/texture_manager.cpp#L104-L110)），没有"导入后产物"的概念。
3. **资产身份与运行时对象割裂**。`Asset`（TextureAsset/MeshAsset…）是元数据包装、无 InstanceID、不可被 PPtr 解析；运行时 `Object`（Texture2D/Sprite）是另一套，按路径惰性创建。两套并存是本次 sprite 类型混淆 bug 的温床之一。
4. **没有 reimport / 变更追踪**。源文件改了，没有"脏→重导"的机制。

**目标（对齐 Unity 资源模型）**：

```
源文件 foo.png
  │  + foo.png.meta（导入设置：Sprite / ppu / pivot / slice / 压缩）
  ▼ Importer（按源类型处理）
资产 SpriteAsset { source, fileid, 设置 }
  │  AssetDatabase 登记（source → asset）
  ▼ 运行时 Loader（按资产设置）
运行时对象 Sprite { texture, uv, pixelsPerUnit }
```

- 编辑器/序列化引用 **AssetHandle\<T\>**（存 fileid，已有）。
- 运行时按 fileid 拿资产 → 按其设置创建运行时对象。
- 改导入设置 → reimport → 所有引用它的地方一起变。

---

## 2. 现状对齐（已有 vs 缺失）

### 已有（可复用）

| 件 | 位置 | 说明 |
|---|---|---|
| `AssetManager` | [asset_manager.cpp](engine/src/runtime/resource/asset/asset_manager.cpp) | `registerAsset(source_path, type)`、`loadAssetSync/Async<T>`、`loadAssets()` 按扩展名扫描 |
| `AssetDatabase` | [asset_database.cpp](engine/src/runtime/resource/asset/asset_database.cpp) | `Configs/asset_database.json` 落盘 file_id + type |
| `Asset` 元数据基类 | [asset.h](engine/src/runtime/resource/asset/asset.h) | TextureAsset/MeshAsset/MaterialAsset/AnimationClipAsset/SceneAsset |
| `TextureAsset::loadFromSource` | [types/texture_asset.cpp](engine/src/runtime/resource/asset/types/texture_asset.cpp) | 解析 `TextureBlob`（像素） |
| `AssetHandle<T>` | [asset_handle.h](engine/src/runtime/resource/asset/asset_handle.h) | 弱引用，序列化成 `{"file_id":{path,uuid}}` |
| `FileID` | [file_id.h](engine/src/runtime/resource/file/file_id.h) | 确定性 id（路径哈希） |
| 运行时 `Texture2D` 加载 | [texture_manager.cpp](engine/src/runtime/function/render/texture/texture_manager.cpp) | `Texture2D::Load` + 缓存 + bindless 注册 |
| 运行时 `Sprite` 加载 | [sprite_loader.cpp](engine/src/runtime/service/sprite/sprite_loader.cpp) | 盐值 FileID + ObjHandle 缓存 + fallback（本次已建） |

### 缺失（本计划要补）

1. **Importer 层**：按源类型处理的导入器（现只有 `TextureAsset::loadFromSource` 一个弱实现）。
2. **导入设置**：`.meta`（或资产库条目）存 ppu/pivot/slice/压缩。现状：无任何持久化设置，Sprite ppu 只能写死。
3. **SpriteAsset**：`AssetType` 无 `Sprite` 成员（[asset.h:12-25](engine/src/runtime/resource/asset/asset.h#L12-L25)），`SpriteAsset` 不存在。
4. **变更追踪 / reimport**：源文件 hash + 设置 → 脏检测。
5. **运行时消费资产**：Loader 按 `SpriteAsset` 设置创建运行时对象，而非每次按路径裸读。

---

## 3. 关键设计决策

### 3.1 资产身份：源文件与资产的关系

Unity 的模型是"一个源文件 + 一组导入设置 = 一个资产"，资产用 guid 标识；.meta 文件存 guid + 设置。

dodoe 已有 `FileID(path)`（路径哈希 → 确定性 id）。决策：

- **默认一源一资产**：`SpriteAsset` 的 FileID = `FileID(path)`（与 Texture2D 相同路径身份）。
- **多配置诉求出现时**（同一图多种 ppu），再引入"资产变体"：`.meta` 里声明多个 sprite 导入配置，各自 `FileID(path, variant_uuid)`（复用 SpriteLoader 已有的盐值 FileID 方案，[sprite_loader.cpp:19-22](engine/src/runtime/service/sprite/sprite_loader.cpp#L19-L22)）。第一阶段只做单配置。
- **运行时对象与资产解耦**：`SpriteAsset`（元数据）≠ `Sprite`（运行时 Object）。`Sprite` 由 Loader 按资产设置创建并缓存。

### 3.2 导入设置存储：`.meta` 同级文件（推荐）

```
Assets/foo.png
Assets/foo.png.meta
```

`.meta` 内容（JSON）：
```json
{
  "guid": "9e37…（确定性，或首次导入生成）",
  "importer": "SpriteImporter",
  "settings": { "pixelsPerUnit": 100, "pivot": [0.5, 0.5], "slice": [0,0,0,0] }
}
```

选 `.meta` 而非集中式 `asset_database.json` 的理由：
- 设置跟着源文件走，版本管理里一起提交/对比。
- 重命名/移动文件时不丢设置。
- Unity 同款心智。
- `asset_database.json` 保留为**索引**（guid → path/type，重导时重建），不再是设置的唯一来源。

### 3.3 Importer 接口

```cpp
// runtime/resource/asset/importer/asset_importer.h
struct ImportContext {
    const FileID& source_file;      // 源文件身份
    const String& source_path;      // 绝对/相对路径
    const Json&   settings;         // 来自 .meta
};

class AssetImporter {
public:
    virtual ~AssetImporter() = default;
    virtual Scope<Asset> import(const ImportContext& ctx) = 0;   // 产出元数据资产
    virtual bool isDirty(const ImportContext& ctx) = 0;          // 源 hash / 设置变了?
};

// 注册表：源扩展名 → importer
class ImporterRegistry {
public:
    static ImporterRegistry& self();
    void registerImporter(const std::string& ext, Scope<AssetImporter> imp);
    AssetImporter* find(const std::string& ext) const;
};
```

内置 importer：
- `TextureImporter`（png/jpg/jpeg/bmp → `TextureAsset`）—— 现有 `loadFromSource` 迁入。
- `SpriteImporter`（同源类型 + .meta 声明 sprite 配置 → `SpriteAsset`）。
- 保留 `ModelImporter`（obj/fbx/gltf → `MeshAsset`）占位，后续接 Assimp。

### 3.4 SpriteAsset 定义

```cpp
// runtime/resource/asset/types/sprite_asset.h
class SpriteAsset : public Asset {
public:
    FileID texture_source{};      // 源纹理路径身份
    Float  pixels_per_unit{100.0f};
    Vector2f pivot{0.5f, 0.5f};
    Rect2f slice{0,0,0,0};        // 九宫格（可选，第一阶段可不做）

    [[nodiscard]] AssetType getType() const override { return AssetType::Sprite; }
};
```

### 3.5 运行时解析链路

```
AssetHandle<SpriteAsset>   （编辑器/序列化，存 fileid）
        │ resolve（AssetManager::findAsset → loadAssetSync<SpriteAsset>）
        ▼
SpriteAsset                 （元数据：ppu / pivot）
        │ SpriteLoader::Load(source_path)  —— 按资产设置创建
        ▼
Sprite runtime Object       （texture + uv + pixelsPerUnit）
```

SpriteLoader 演进（基于本次已建的 [sprite_loader.cpp](engine/src/runtime/service/sprite/sprite_loader.cpp)）：
- `Sprite::Load(path)` 保留（运行时按路径/资产加载的便捷入口）。
- 新增 `Sprite::Load(const SpriteAsset&)`：以资产设置覆盖默认 ppu/pivot。
- Sprite 的盐值 FileID 与 SpriteAsset 的变体身份对齐（多配置阶段）。

### 3.6 序列化与编辑器引用

- 组件里的"资产引用"字段从 `PPtr<Sprite>` 演进为 `AssetHandle<SpriteAsset>`（若走 [dodoe-editor-type-system.md](./dodoe-editor-type-system.md) 的通用资产引用字段）。
- `AssetHandle<T>` 已序列化成 `{"file_id":{path,uuid}}`（[serializer.h:107-108](engine/src/runtime/core/meta/serializer/serializer.h#L107-L108)），round-trip 无需新序列化器。
- 编辑器 Project 面板按 AssetType 过滤、拖拽落 AssetHandle。

---

## 4. 实施步骤

每步保持可构建可运行；与编辑器侧（[dodoe-editor-type-system.md](./dodoe-editor-type-system.md)）解耦，可并行。

**P1 · 资产身份与索引基建**
- `AssetType` 加 `Sprite`；`AssetManager::loadAssets()` 的扩展名映射把"可被 .meta 声明为 sprite"的图标记为 `Sprite` 候选。
- `.meta` 读写：`ImportSettingsIO`（读/写/缺省生成），guid 确定性 = 路径哈希（复用 `FileID`），不新造。
- 验收：给 `foo.png` 生成 `foo.png.meta`，内容可读回。

**P2 · Importer 层落地**
- `AssetImporter` / `ImporterRegistry`（§3.3）。
- `TextureImporter`：现有 `TextureAsset::loadFromSource` 迁入。
- `SpriteImporter`：读 .meta 的 sprite 设置 → 建 `SpriteAsset`。
- 验收：`ImporterRegistry` 按扩展名产出 TextureAsset / SpriteAsset。

**P3 · 变更追踪 / reimport**
- `AssetManager` 登记资产时记源文件 hash（或 mtime）+ 设置签名。
- 启动/资源库 refresh 时 `isDirty()` → 重导 → 更新 asset_database.json 索引。
- 验收：改 `foo.png.meta` 的 ppu → 重启/reimport → SpriteAsset 设置更新。

**P4 · 运行时消费资产**
- `SpriteAsset` → `SpriteLoader::Load(asset)`：ppu/pivot 覆盖默认。
- `SpriteRendererComponent` 资产字段改为 `AssetHandle<SpriteAsset>`（依赖 [dodoe-editor-type-system.md](./dodoe-editor-type-system.md) 的字段体系；若该计划未动组件字段，则先保留 PPtr + 运行时按路径解析，资产设置暂不贯通）。
- 验收：编辑器改 ppu → 运行后 sprite 尺寸按新 ppu 变化。

**P5 · 编辑器接线**
- Project 面板 AssetDatabase 驱动（复用 [dodoe-editor-unity-like.md §3.4](./dodoe-editor-unity-like.md) 的 D3 设计）。
- Inspector 资产引用字段：通用 picker（按 AssetType 过滤，依赖 [dodoe-editor-type-system.md](./dodoe-editor-type-system.md)）。
- 验收：资源库显示 Sprite 类型，拖拽赋值，双击可改导入设置。

---

## 5. 验收（最小达标线）

- [ ] 任一图片都有可读写的 `.meta`（或缺省生成），guid 确定。
- [ ] `SpriteImporter` 能从图片 + .meta 产出 `SpriteAsset`（ppu/pivot 持久化）。
- [ ] 改 .meta → reimport → 运行时 sprite 尺寸/行为随之变化。
- [ ] 运行时不再每次裸读源文件（命中资产缓存）。
- [ ] 同一图片可被多个 sprite 配置引用（多配置阶段）。
- [ ] 与现有 sprite 渲染（[sprite_loader.cpp](engine/src/runtime/service/sprite/sprite_loader.cpp)）全链路回归：正方形源图渲染正方形、空路径 fallback 不崩。

---

## 6. 与本次 sprite bug 修复的关系

本次已完成的修复（真实 `Sprite` 运行时对象 + 盐值 FileID + 类型守卫，见实现计划）是**运行时层**的正确化：让 `PPtr<Sprite>` 真正指向 Sprite、消灭类型混淆。资产管线是在其上层的**资产层**演进：给 Sprite 一个持久的导入身份与设置。两者不冲突，管线 P4 依赖运行时 SpriteLoader 已存在。
