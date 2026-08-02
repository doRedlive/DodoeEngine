# dodoe 编辑器反射类型体系（Type System）设计 —— 让任意字段像 Unity [SerializeField] 一样可见可编辑

文档版本：v1.0 · 2026-08-02
引擎：dodoe runtime（DX12 / Deferred / DualThread）· Qt6 编辑器（Cakery）· C++20
关联文档：本计划是**反射/Inspector 的"类型内核"**。界面形态与 JSON 配置见 [dodoe-editor-unity-like.md](./dodoe-editor-unity-like.md)（v1.1 姊妹篇）；JSON 配置/CustomEditor 增量见 [dodoe-editor-data-custom.md](./dodoe-editor-data-custom.md)（G1-G11）。

## 1. 问题：为什么"让任意类型在编辑器可见"现在很别扭

用户诉求（等价 Unity `[SerializeField]`）：**任何 C++ 字段，不管什么类型（bool / float / Vector / 枚举 / 嵌套结构体 / 资产引用），标记一下就能在 Inspector 面板显示、编辑、序列化。**

现状把这件事做成了"每个类型一种特殊处理 + 按类型名字符串匹配"，并且 PPtr 被滥用成"让任意类型可见"的载体。三个硬伤（代码核对）：

### 硬伤 1：类型分发靠字符串
字段只有 `FieldAccessor::getFieldTypeName()` 返回的 `const char*`（[reflection.h:142](engine/src/runtime/core/meta/reflection/reflection.h#L142)），编辑器靠它去 `PropertyDrawerRegistry` 匹配（[PropertyDrawerRegistry.cpp:43](engine/src/editor/property/PropertyDrawerRegistry.cpp#L43)）。
- 类型名带命名空间、模板参数、改个名 → 静默匹配不上。实例：注册了 `"PPtr<dodoe::Texture>"` 但实际字段是 `"PPtr<Sprite>"`，直接落空。

### 硬伤 2：反射只注册"整个组件"
`TypeMeta::newMetaFromName(name)` 只对 `STRUCT(Component, ...)` 注册过的类型有效。任何**嵌套结构体**（`struct Settings { float x; bool y; }`）没注册 → `CompositeDrawer` 里 `newMetaFromName("Settings")` 无效 → 显示 `"(Settings)"`，无法展开（[CompositeDrawer.cpp:20-22](engine/src/editor/property/drawers/CompositeDrawer.cpp#L20-L22)）。

### 硬伤 3：兜底是 UB
匹配不到的类型掉到 `ScalarDrawer`，它把一个 `int` 硬写进任意类型的内存（[ScalarDrawer.cpp:85-87](engine/src/editor/property/drawers/ScalarDrawer.cpp#L85-L87)）。

**结论**：这不是某个 drawer 的 bug，是**反射对"任意字段类型"的描述能力缺失**。Unity 没有这个问题，因为 C# 反射给的是真实的 `System.Type`，Inspector 按类型图递归渲染。

---

## 2. 目标：Unity 式类型图（type graph）

```
META(Enable) 标记字段
        ▼
FieldAccessor 携带 类型 token（真实类型身份，不是名字）
        ▼
通用 Inspector 循环：按 token 选控件
  primitive → Scalar      enum → EnumDropdown
  vector   → VectorDrawer struct → Foldout + 递归子字段
  assetref → AssetPicker   array → 列表
```

- `META(Enable)` 等价 `[SerializeField]`，**与字段类型无关**。
- 类型分发按 token（枚举/id），精确匹配，永不靠字符串。
- 任意结构体自动折叠展开（注册所有可序列化类型，或字段携带内联子字段描述）。
- PPtr 回归"资产引用"语义；普通数据不再蹭 PPtr。

与既存两份文档的关系：
- [dodoe-editor-unity-like.md](./dodoe-editor-unity-like.md) S1/S7/S8/S13、[dodoe-editor-data-custom.md](./dodoe-editor-data-custom.md) G1-G6 是对**抽屉与 attribute** 的修补（纸面）。本计划是**类型内核**改造，让这些修补变健壮：字符串匹配被 token 取代、嵌套结构体可递归、兜底不再 UB。
- 本计划不改界面形态/JSON 配置（那是另两份文档的范畴）。

---

## 3. 关键设计

### 3.1 字段携带类型 token（核心）

metaparser 代码生成器解析字段声明时**本来就拿到真实的 C++ 类型**。在生成 `field_type_name_` 字符串的同时，生成一个类型 token：

```cpp
// runtime/core/meta/reflection/reflection.h
enum class FieldType : uint8_t {
    Unknown = 0,
    Bool, I32, U32, F32, F64, String,         // primitives
    Enum,                                     // 枚举（值表由 codegen 输出）
    Vec2, Vec3, Vec4, Vec2i, Vec3i, Vec4i, Color,
    Struct,                                   // 结构体（可递归，见 3.2）
    Ptr,                                      // PPtr<T>（资产引用，见 3.3）
    Array,                                    // 数组
    Count
};

class FieldAccessor {
    ...
    [[nodiscard]] FieldType getFieldType() const;   // 新增：token 分发
    [[nodiscard]] const char* getFieldTypeName() const;  // 保留：显示/调试
};
```

分发：
- `PropertyDrawerRegistry::create(field)` 先查 `getFieldType()`（switch token），再按名字（兼容层）。
- `CompositeDrawer` 遇到 `Struct` token → 查子字段递归。

### 3.2 结构体递归：两种注册策略（选一，推荐 A）

**策略 A（推荐）：注册所有可序列化类型**
扩展 `STRUCT(...)` / 反射注册，让**任意** `REFLECTION` 结构体（不只是组件）都进 classmap：
- `TypeMetaRegisterInterface::register2classmap` 已存在（[reflection.h:73-81](engine/src/runtime/core/meta/reflection/reflection.h#L73-L81)），codegen 对每个 `STRUCT` 结构体都生成注册。
- `CompositeDrawer` 就能 `newMetaFromName(typeName)` 取到子字段 → 递归。
- 现状只对组件生效是因为 `STRUCT` 宏当前只用于组件；放开即可。

**策略 B（轻）：字段携带内联子字段描述**
`FieldAccessor` 额外携带 `subfield_count` + 子字段描述表，`Struct` token 直接用，不依赖全局 classmap。适合"只想编辑器可见、不想全注册"的类型。

第一阶段用 A（改动集中在 codegen + 注册），`Settings` 这类嵌套结构体即可见。

### 3.3 资产引用：PPtr / AssetHandle 是一种字段类型，不是特例

- `PPtr<T>` 生成 `FieldType::Ptr` token，且（codegen）在 token 旁带 **资产类型**（`T` 的注册名，或 `META(AssetType, "Sprite")` 显式声明）。
- 编辑器用**一个通用 `AssetPickerDrawer`**：
  1. 读字段声明的资产类型（token 或 META）。
  2. 从 AssetDatabase 列出该类资产（[dodoe-asset-pipeline.md](./dodoe-asset-pipeline.md) 的 AssetType）。
  3. 点选/拖拽 → 写回 FileID/AssetHandle。
- 不再需要 `"PPtr<Sprite>"` 这种字符串注册（[PropertyDrawerRegistry.cpp:77](engine/src/editor/property/PropertyDrawerRegistry.cpp#L77) 当前的 hack）。

### 3.4 META 提示（UI hint）→ 控件定制

`META(Enable)` 已存在。扩展 hint，让控件按提示定制（等价 Unity attribute）：
```cpp
META(Enable, Range(0.0f, 100.0f))          // 滑条
META(Enable, Tooltip("..."))
META(Enable, Hidden)                        // 不显示
META(Enable, Header("Physics"))
META(Enable, AssetType("Sprite"))           // 资产 picker 过滤
META(Enable, EnumValues({A, B, C}))
```
- codegen 透传 annotation（[dodoe-editor-data-custom.md](./dodoe-editor-data-custom.md) G3/S13 已规划），`FieldAccessor::attribute()` 读取。
- JSON 外挂表（`inspectors.json` fieldAttributes）作为 codegen 前的 fallback / 覆盖语义。

### 3.5 通用 Inspector 渲染循环

```
InspectorPanel 选中实体
  → 遍历组件（ComponentDB）
  → 遍历字段（FieldAccessor）
  → PropertyDrawerRegistry::create(field)：
      ① META hint 命中（Range→滑条 / Hidden→跳过 / AssetType→picker）
      ② FieldType token 命中（Scalar / Enum / Vector / Color / Struct→递归 / Ptr→AssetPicker / Array）
      ③ 兜底：只读文本（不再写 int 进未知类型）
```

---

## 4. 实施步骤

每步保持可构建可运行。与资产管线（[dodoe-asset-pipeline.md](./dodoe-asset-pipeline.md)）解耦。

**P1 · 类型 token 落地（核心）**
- `FieldType` 枚举 + `FieldAccessor::getFieldType()`。
- metaparser codegen：`field.cpp` 解析字段时，把 C++ 类型映射成 token 并生成。映射表（`type name → FieldType`）在生成器里维护一份；未知类型 → `Struct`（走递归）或 `Unknown`（只读兜底）。
- `PropertyDrawerRegistry::create()` 改 switch token。
- 验收：bool/i32/f32/string/vec/enum 全走 token 命中，不再靠字符串列表。

**P2 · 嵌套结构体递归**
- `STRUCT` 放开到所有可序列化结构体（codegen 生成注册）。
- `CompositeDrawer` 遇 `Struct` token 递归子字段（含缩进、Tooltip、Hidden）。
- 验收：`struct Settings { float x; bool y; Vector3f p; }` 作为组件字段，Inspector 折叠展开、可编辑。

**P3 · 枚举与数组**
- `EnumDrawer` 读 codegen 输出的枚举值表（`FieldAccessor::enumValues()`），删除 `CameraType` 硬编码。
- `ArrayAccessor` 已有（[reflection.h:165](engine/src/runtime/core/meta/reflection/reflection.h#L165)）；补 ArrayDrawer（列表 + 增删，第一阶段可只读）。
- 验收：任意枚举字段下拉正确；数组字段可浏览。

**P4 · 通用资产 picker**
- `PPtr`/`AssetHandle` → `FieldType::Ptr` + 资产类型信息。
- `AssetPickerDrawer`：按资产类型从 AssetDatabase 列资源、拖拽/点选写回。
- 替换 [dodoe-editor-data-custom.md](./dodoe-editor-data-custom.md) G6 的 PPtrDrawer hack。
- 验收：拖图到 sprite 字段，写入正确 FileID，序列化 round-trip。

**P5 · META hint 全量**
- codegen 透传 annotation → attribute 字典；`Range/Hidden/Tooltip/Header/AssetType/EnumValues` 全生效。
- JSON 外挂表退为覆盖语义。
- 验收：`META(Enable, Range(...))` 直接出滑条，无需 JSON。

**P6 · 兜底只读 + 增量刷新**
- 未知 token → 只读文本（消灭 ScalarDrawer 写 int 的 UB）。
- Inspector diff + `updateValue()` 增量刷新（对齐 [dodoe-editor-data-custom.md](./dodoe-editor-data-custom.md) G7）。
- 验收：未知类型不崩、只读显示；改字段不整列重建。

---

## 5. 验收（最小达标线）

- [ ] 所有 primitive/vector/enum 字段按 token 命中正确控件，无字符串匹配落空。
- [ ] 任意嵌套结构体字段折叠展开、递归可编辑。
- [ ] 资产引用字段用通用 picker（按资产类型过滤），PPtr 字符串注册 hack 移除。
- [ ] 未知类型兜底只读文本，无 UB。
- [ ] `META(Enable, Range/Hidden/Tooltip/Header/AssetType)` 全部直接生效。
- [ ] 序列化 round-trip 不变（token 只影响编辑器，不影响 `Serializer`）。

---

## 6. 与本次 sprite bug 修复的关系

本次已完成的修复（[sprite_renderer_system.cpp](engine/src/runtime/function/world/systems/sprite_renderer_system.cpp) 走真实 Sprite + 类型守卫 + `PPtr<Sprite>` 编辑器注册）是**存量正确化**：让当前 PPtr 机制别再 UB。本计划是**下一代形态**：用类型 token + 通用 picker 取代"PPtr 当万能载体"，从根上消除这一类 bug（字符串匹配落空、嵌套类型不可见、兜底 UB）。
