# dodoe 资产层（Asset Layer）设计方案 —— 壳 + 数据体，复用组件机制

文档版本：v1.1 · 2026-08-02
引擎：dodoe runtime（DX12 / Deferred / DualThread）· Qt6 编辑器（Cakery）· C++20 · entt ECS
关联文档：[dodoe-asset-pipeline.md](./dodoe-asset-pipeline.md)（资产管线现状）· [dodoe-id-system.md](../dodoe-id-system.md)（ID 体系）

## 0. 核心思想：资产层

1. **资产存在"资产层"**。material 这类资产的本质是**字段集合**：字段可序列化/反序列化、可训练（字段即参数）。它与 `foo.png`（二进制像素，需导入解析）不是一回事。
2. **资产应像流水线一样加载**：可流式、可序列化，源文件 → 加载 → 数据体 → 消费。
3. **纯数据容器与 ECS 组件同构**：组件就是"可反射、可序列化的数据容器"，因此资产数据体**复用组件机制**（反射/序列化/编辑器面板/脚本绑定/可微标记）。
4. 结论：**资产 = 壳 + 数据体**；数据体即组件式容器。

## 1. 要解决的问题

1. **结构化资产被当二进制资产处理**。`material`/`anim clip`/`scene` 的源文件本身就是字段集合（可序列化、可训练），却和 `foo.png` 一样被塞进 `Asset` 派生类、逐字段手写读写（[material_asset.cpp:9-76](engine/src/runtime/resource/asset/types/material_asset.cpp#L9-L76) 手写 8 个字段）。
2. **数据重复定义**。`MaterialProperties`（[material.h:11-21](engine/src/runtime/function/render/material/material.h#L11-L21)）与 `MaterialAsset`（[material_asset.h:13-21](engine/src/runtime/resource/asset/types/material_asset.h#L13-L21)）字段各存一份，无同步保证。
3. **资产与组件机制割裂**。组件已有成熟反射（`REFLECTION_TYPE`/`STRUCT`/`META` + metaparser + `Serializer`），资产数据体却没复用，无法白拿反射、序列化、编辑器面板、脚本绑定、可微标记。
4. **运行时割裂**。`Texture2D::Load(path)`（[texture_manager.cpp:17](engine/src/runtime/function/render/texture/texture_manager.cpp#L17)）与 `SpriteLoader`（[sprite_loader.cpp:23-28](engine/src/runtime/service/sprite/sprite_loader.cpp#L23-L28)）按路径裸读，绕开资产系统。
5. **guid 不持久**。结构化资产走 `registerAsset`，guid 运行时现场造，改名/重启即变。

## 2. 核心模型：资产 = 壳 + 数据体

```
资产 Asset
  ├── 壳（所有资产共有）：AssetMetaData{file_id, type, name, source_path, mtime, signature, tags, deps}
  └── 数据体（与组件同构）：反射标记的纯数据容器，Serializer 整体读写
```

两类资产，同一壳，数据体不同：

| | 结构化资产 | 二进制资产 |
|---|---|---|
| 例子 | material / anim clip / scene / prefab / config | texture 像素 / mesh 顶点 |
| 源文件 | 字段 json（.domat/.doaniclip/.doscn） | 像素/几何（.png/.obj） |
| 加载 | **反序列化**（Serializer::read 整体还原） | **导入解析**（importer 产 TextureBlob/MeshData） |
| 可训练 | 字段即参数（可微标记） | 解析结果可当参数 |
| 壳 | 要 | 要 |

数据体与 ECS 组件的差别仅存于**存储位置**（资产在 AssetManager 仓库、组件挂实体），**数据定义与序列化机制完全复用**——这与 Bevy（资产 T 与组件共享 Reflect）和 Unity（ScriptableObject 与 Component 共用 SerializedObject）一致。

## 3. 数据体复用组件机制（阶段 1 核心）

### 3.1 反射化数据体

`MaterialProperties` 改为与组件相同标记（参照 [id_component.h:10-30](engine/src/runtime/function/world/components/id_component.h#L10-L30)）：

```cpp
REFLECTION_TYPE(MaterialProperties)

namespace dodoe {
    STRUCT(MaterialProperties, WhiteListFields) {
        REFLECTION_BODY(MaterialProperties)

        META(Enable)
        Vector4f color{1.0f, 1.0f, 1.0f, 1.0f};
        META(Enable)
        Vector3f emissive{0.0f, 0.0f, 0.0f};
        META(Enable)
        Float metallic{0.0f};
        META(Enable)
        Float roughness{1.0f};
        META(Enable)
        FileID base_color_texture{};
        META(Enable)
        FileID normal_texture{};
        META(Enable)
        FileID metallic_roughness_texture{};
        META(Enable)
        FileID emissive_texture{};
    };
}
```

metaparser 自动生成反射（`_generated/reflection`），无需手写注册。

### 3.2 数据体化 Asset 壳

```cpp
class MaterialAsset : public Asset {
    MaterialProperties m_data{};   // 唯一数据源，消灭双份字段

public:
    [[nodiscard]] Bool loadFromSource(const String& path) override;  // Serializer::read
    [[nodiscard]] Bool saveToSource(const String& path) const override;  // Serializer::write
    [[nodiscard]] MaterialProperties& getData() { return m_data; }
};
```

`loadFromSource`/`saveToSource` 从手写 8 字段收敛为一次整体读写：

```cpp
Bool MaterialAsset::loadFromSource(const String& path) {
    // 读文件 → Json → Serializer::read(json, m_data)
}
Bool MaterialAsset::saveToSource(const String& path) const {
    // Serializer::write(m_data) → dump → 写文件
}
```

### 3.3 复用收益（白拿）

反射 → 编辑器属性面板、脚本绑定、`Serializer` 整体序列化、可微标记，全部随组件机制自动获得。

## 4. .meta / guid 统一（阶段 1）

按 [dodoe-id-system.md](../dodoe-id-system.md) 方案 A 收敛：

- **`.meta` 为所有资产的权威**：`{guid, importer, settings}`。结构化资产 `.meta` 只写 `guid`（settings 无意义）；二进制资产写全。
- **guid 持久化**：首次导入随机生成写 `.meta`，此后不变（`ImportSettingsIO::LoadOrCreate` 已有复用逻辑，把 `MakeDeterministicGuid` 换为随机生成即可）。
- `asset_database.json` 保持索引职责（mtime/signature/依赖），从 `.meta` 派生，可重建。

## 5. 流水线加载（阶段 1）

```
源文件(.domat/.doscn/foo.png)
  → loader 分流
      结构化 → 读 json → Serializer::read 反序列化 → 数据体
      二进制 → ImportSettingsIO 读 .meta → importer 解析 → TextureBlob/MeshData
  → 数据体（组件式容器，可缓存、可序列化、可训练）
  → 消费（运行时 / 训练）
  → 写回（Serializer::write / 导出）
```

结构化资产体积小，启动同步反序列化即"流式"；纹理等大数据按需懒解析（现状 `loadAssetSync` 已支持）。此层统一走 `AssetManager`，消灭第二套路径。

## 6. 具体资产落位：texture / sprite

### 6.1 texture：二进制资产，不设独立资产文件

三层模型：

```
foo.png                源文件（用户资产，不可变）
foo.png.meta           .meta（guid + importer + settings，导入输入）
TextureAsset           （内存）壳 + TextureBlob 数据体（导入产物）
[可选] 磁盘缓存         （Unity Library 式，产物序列化落盘）
```

决策：

- **阶段 1 不落盘**：导入即解析、内存持有（[asset_manager.cpp:262-346](engine/src/runtime/resource/asset/asset_manager.cpp#L262-L346)）。理由：脏检测已就绪（mtime + `import_signature`），将来加缓存零成本；避免现在引入缓存失效/清理/跨平台问题。
- **可选后续**：冷启动解析成瓶颈时，按 Unity Library 式加产物缓存层（`Library/<signature>.bin` 存解析后 blob，signature 命中免重解析）。
- **可训练导出**：像素当参数优化后写回源文件或新资产，与缓存无关。

结论：**texture 的"资产"是内存中的解析产物；独立文件只在缓存或导出时需要，不是资产定义的一部分**。

### 6.2 sprite：结构化资产，采用 Unity 子资产模型

数据体（反射容器）：

```cpp
REFLECTION_TYPE(SpriteData)

namespace dodoe {
    STRUCT(SpriteData, WhiteListFields) {
        REFLECTION_BODY(SpriteData)

        META(Enable)
        FileID texture_source{};      // 引用父 texture
        META(Enable)
        Float pixels_per_unit{100.0f};
        META(Enable)
        Vector2f pivot{0.5f, 0.5f};
        META(Enable)
        Rect2f slice{};
    };
}
```

现状问题：`SpriteImporter` 挂在空扩展名槽（`""`），而 `loadAssets`/`refreshAssets` 只对图片/模型扩展名调 `importSourceFile`（[asset_manager.cpp:215-218](engine/src/runtime/resource/asset/asset_manager.cpp#L215-L218)）——**空扩展名永远不会被路由触发，sprite 资产目前没有真实源文件**。

决策：

- **方案 A（推荐）：Unity 子资产**。TextureImporter 导入 texture 时按 `.meta` 的 `sprites` 数组生成 `SpriteData` 子资产（texture_source=父 tex，guid=父guid+索引），无独立文件，随父资产重建。
- **方案 B：独立 sprite 资产**。需定义 sprite 源格式（如 `.sprite` json），数据体写回该文件。

引用关系：`SpriteData.texture_source` 存 `FileID`，经 AssetManager 解析出父 TextureAsset——资产内引用与阶段 2"Handle 进组件"用同一套句柄机制。

## 7. 运行时接线（阶段 2，Bevy 借鉴）

- **Handle 进组件**：`SpriteRendererComponent` 等改持 `AssetHandle<SpriteAsset>`（[asset_handle.h](engine/src/runtime/resource/asset/asset_handle.h) 已有），实体只挂句柄，数据留在仓库。
- **统一消费入口**：`Texture2D::Load`/`SpriteLoader` 内部改走 AssetManager（先取 TextureAsset 再取 blob），消灭裸读。
- **`AssetEvent`**：reimport 成功广播 Added/Modified/Removed，ECS 系统监听刷新运行时对象，补上"改设置后运行时不知情"。

## 8. 可训练支持（阶段 3，远期）

- 数据体字段加 `META(Trainable)` 标注，反射 attrs（`GetAttrsFunc`，[reflection.h:84](engine/src/runtime/core/meta/reflection/reflection.h#L84)）可收集可微参数集。
- 数据体即参数容器的初始值；优化后经 `saveToSource`（结构化）或导出（二进制）写回。
- 资产层本身不感知"可微"——只是"数据能变回去"。

## 9. 分阶段实施

| 阶段 | 内容 | 范围 |
|---|---|---|
| 1（本轮） | MaterialProperties 反射化；MaterialAsset 改壳+数据体、整体读写；其余结构化资产（AnimClip/Scene）同构；TextureImporter 按 `.meta` sprites 数组产出 SpriteData 子资产；.meta/guid 持久化 | 资产层，不动运行时 |
| 2（后续） | Handle 进组件；Texture2D::Load/SpriteLoader 走资产；AssetEvent | 运行时接线 |
| 3（远期） | Trainable 标记、参数导出、可微训练消费 | 训练侧 |

## 10. 涉及文件清单（阶段 1）

| 文件 | 改动 |
|---|---|
| [material.h](engine/src/runtime/function/render/material/material.h) | `MaterialProperties` 加反射标记 |
| [material_asset.h](engine/src/runtime/resource/asset/types/material_asset.h) | 字段收敛为 `m_data`（壳+数据体） |
| [material_asset.cpp](engine/src/runtime/resource/asset/types/material_asset.cpp) | load/save 改整体 `Serializer` 读写 |
| [animation_clip_asset.h/.cpp](engine/src/runtime/resource/asset/types/animation_clip_asset.h) | 同构改造（`AnimClipData` 数据体） |
| [scene_asset.h/.cpp](engine/src/runtime/resource/asset/types/scene_asset.h) | 同构改造 |
| [import_settings_io.cpp](engine/src/runtime/resource/asset/importer/import_settings_io.cpp) | `MakeDeterministicGuid` → 随机生成持久化 |
| [asset_manager.cpp](engine/src/runtime/resource/asset/asset_manager.cpp) | 结构化资产路径补 `.meta`/guid 读写 |
| [dodoe-id-system.md](../dodoe-id-system.md) | 按方案 A 执行 |
