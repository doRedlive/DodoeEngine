# 资产身份整改方案（对齐 Unity 模型）

状态： 实施中
日期： 2026-08-11
范围： Dodoe runtime、资源系统、对象身份、场景序列化

## 1. 背景与问题

当前工程身份全部路径派生：

- `FileID::computeID = uuid ^ (path_hash << 32)`（file_id.cpp），.meta guid = `MakeDeterministicGuid` = 路径哈希（import_settings_io.cpp）。
- `Object` 同时携带 `FileID` + 独立 `UUID` 两个身份字段；`PPtr<T>` 再带一份 `{FileID, UUID, InstanceID}`，guid 多处重复、改名即失效。

要解决的三个问题：

1. **ID 系统混乱**：身份由路径派生，改名/移动资产即失效。
2. **TextureAsset 不应持有 GPU 纹理**：Asset 是纯元数据，运行时对象才拥有数据体 + GPU 句柄。
3. **没有统一的加载契约**：C# `Resources.Load` 直接绕过 AssetManager 调 `Texture2D::Load` / `SpriteLoader::Load`。

## 2. Unity 模型（对齐目标）

- **GUID**（`.meta`）：标识资产**文件**；首次导入生成，随文件移动/改名不变。
- **fileID / Local ID**：标识资产文件内一个**对象**（主对象 + 各子资产各一个）；序列化引用存的就是 `(guid, fileID)`。
- **InstanceID**：运行时进程内 ID，**从不序列化**；加载时由 `(guid, fileID)` 解析并分配。
- **Asset ≠ Object**：一个 Asset 文件可包含多个 Object（纹理 → Texture2D 主对象 + N 个 Sprite 子资产）。
- **PPtr = 身份**：Unity 核心序列化引用就是 `{guid, fileID}`，直接内嵌在序列化数据里，没有独立的 "AssetRef" 类。
- **AssetReference ≠ PPtr**：AssetReference 是 Addressables 包的**异步加载 + 引用计数**高层 API，不是核心序列化引用类型。本项目不需要它。

## 3. 目标模型

### 3.1 身份字段（直接落在类型上，不引入新结构体）

| 名字 | 类型 | 生命周期 | 说明 |
|------|------|---------|------|
| `FileID` | `UInt32` intern 索引 | 进程内（静态字典） | filepath 简单封装：`Make(path)` intern 成整数，传参/比较/哈希不碰字符串。**非身份** |
| `asset_id` | `UUID` | 持久（.meta） | Unity GUID；首次导入 `UUID::Generate()`，改名/移动不变 |
| `local_id` | `UInt32` | 持久（序列化） | Unity fileID；`0` = 主对象，`>0` = 子资产（.meta 中存储） |
| `InstanceID` | `UInt32` | 进程内 | 运行时身份，从不序列化 |

- `Object`：`m_asset_id + m_local_id + m_instance_id`（替换原 `m_file_id + m_uuid`）。
- `PPtr<T>`：`m_asset_id + m_local_id + m_instance_id`（替换原 `{FileID, UUID, InstanceID}`）。
- 序列化引用 = `{asset_id, local_id}`（= Unity `{guid, fileID}`），等价于原方案的 `AssetRef`，只是不再单独造类型。

### 3.2 FileID = interned 路径句柄

```cpp
class FileID {
    UInt32 m_intern_id{0};            // 0 = 空
public:
    FileID() = default;
    explicit FileID(const String& path) { *this = Make(path); }
    static FileID Make(const String& path);      // 静态表 intern
    const String& getPath() const;               // id → path 反查（引用稳定）
    UInt32 getInternID() const;
    Bool isValid() const;                        // m_intern_id != 0
    Bool operator==/!=;
    // hash 按 m_intern_id
};
```

- 静态表：`path_to_id`（查找用）+ `id_to_path`（节点型 map，保证 `getPath()` 返回的引用在后续插入后仍有效）。
- 删除 `getUUID()` / `computeID()` / `m_path / m_uuid / m_id`。
- 用法：`AssetMetaData.source_file`、`resolvePathToRef(FileID)` 入参、编辑器/诊断按路径查找。**序列化身份一律不用它**。

### 3.3 Asset = 纯元数据

`AssetMetaData` 收敛为单条记录：`asset_id + local_id(=0 主对象)`、type、name、`source_file(FileID)`、mtimes、import_signature、`dependencies(按 asset_id)`、is_builtin。

删除 `loadFromSource/saveToSource/unloadRuntime` 纯虚与各数据子类（TextureAsset / SpriteAsset / MeshAsset / MaterialAsset / AnimationClipAsset）。数据体移入运行时 Object。

### 3.4 运行时 Object = 数据体

- `Texture2D`：持有导入 blob + GPU 句柄（原 `TextureAsset::m_gpu_texture` 的位置）。
- `Sprite`：持 ppu/pivot/slice + 父 `PPtr<Texture2D>`。
- 全局表：`InstanceID → Object*` + `hash(asset_id, local_id) → InstanceID`。

### 3.5 资源管理器 = 唯一加载入口

```cpp
template<class T> T* ResourceManager::loadObject(const UUID&, UInt32);   // 加载/导入 + 缓存 + 登记
template<class T> T* ResourceManager::loadObjectByPath(const FileID&);   // FileID → resolvePathToRef → loadObject
template<class T> T* ResourceManager::findLoaded(const UUID&, UInt32);   // 只查缓存，不加载
```

缓存 `(asset_id, local_id) → Object*`；同一资源三条路径（C++/C#/场景加载）返回**同一 native InstanceID**。

### 3.6 PPtr

```cpp
template<class T> class PPtr {
    UUID m_asset_id{};
    UInt32 m_local_id{0};
    InstanceID m_instance_id{0};
    // getLoaded() 只查已加载 / resolve() 经 ResourceManager 加载 / isAssigned() 有引用
};
```

注意 `UUID()` 默认构造 = 随机，空引用必须用 `UUID(0)`。

### 3.7 序列化（严格新格式，双读后单写）

- **写**：`{ "asset_id": "<uuid>", "sub_object_id": <u32> }`。
- **读**：兼容 3 种 —— 新格式、旧 FileID `{path, uuid}`、旧 PPtr `{file_id:{path,file_uuid}, uuid}`；缺 uuid 走 path 查找 + **一次**迁移告警。保存后统一写新格式。
- FileID（路径句柄）序列化为 `{path}`。

## 4. 迁移兼容

- 现有 .meta 的路径哈希 guid **已存盘**，随文件移动；新代码读到合法 guid 原样保留，**只有新建 .meta 用随机 guid**。旧场景引用仍能解析。
- 旧 Sprite 引用（伪独立文件 + hash(path)^salt uuid）无法映射到新的 `(texture_guid, local_id)`，需在编辑器重新指向。已知限制。

## 5. 分阶段

### Phase 1 — 三层身份 + 序列化新格式

- `FileID` 重定位 interned 路径句柄（file_id.h/.cpp）。
- `Object` 身份 `{asset_id, local_id}`；`FindInstanceID(asset_id, local_id)`；`AllocateInstanceID` 对已存在 ref 直接返回；`m_registered` 标志防 shadow 对象析构误删。
- `PPtr<T>` 收敛 `{asset_id, local_id, instance_id}`。
- .meta 新文件随机 guid（import_settings_io.cpp）。
- Serializer 新格式 + 三格式兼容读（serializer.h/.cpp）。
- AssetManager/AssetDatabase 按 guid 索引 + `resolvePathToRef(FileID)`。
- 全工程 FileID 使用点迁移（身份语义 → asset_id/local_id；路径语义 → FileID interned）。

**完成条件**：移动/改名资产文件（.meta 随行）后场景引用仍解析；旧格式场景可读并输出一次迁移告警；新保存写新格式。

### Phase 2 — Asset 元数据化 + Texture/Sprite 运行时 Object（含子资产）

- `Asset` 收敛元数据记录，删数据纯虚与 per-type 数据类。
- `.meta` 新增 `sprites[]` 数组（name / local_id / ppu / pivot / slice / rect），导入产出 main Texture2D + N 个 Sprite 子对象；首次导入为每个 sprite 生成稳定 local_id 并写回 .meta。
- ResourceManager `loadObject` / `loadObjectByPath` / `findLoaded`；`(asset_id, local_id) → Object*` 缓存。
- TextureManager / SpriteLoader 降为后端，不再作对外加载 API。

**完成条件**：C++、C#、场景加载三条路径对同一资产拿到同一 native InstanceID。

### Phase 3 — Object 生命周期

- `ObjectHandle{InstanceID, generation}`；`Object::isAlive`；generation 随释放递增；销毁通知。
- C# Object 持 handle，`IsValid` 调 native `isAlive`；cache 失效驱逐 + `WeakReference`；shutdown 先通知脚本再清资源。

**完成条件**：卸载资源或 shutdown 后，任何旧 C# wrapper `IsValid == false`。

### Phase 4 — PPtr + 代码生成器 + C# 统一工厂

- PPtr `getLoaded/resolve/isAssigned`；构造校验运行时类型。
- 生成器删 `PPtr<Texture>` 死特判；通用 PPtr getter/setter 做类型校验；生成 C# 允许 null。
- C# `RegisterFactory` 注册表取代手写 switch；`Resources.Load<T>(path/asset_id)` / `Find<T>`；删 `typeof(T)` 分支。

### Phase 5 — 扩展全部资产类型 + 清理

- 按同一模板接入 Material / Mesh / AnimationClip / Shader / AudioClip / TextureCubemap。
- 删除废弃 path 裸读 API 与冗余缓存；补 reimport 事件广播。

## 6. 验证

无 C++ 单测框架，验证走构建 + 示例工程：

1. 构建：`scripts/build.bat Cakery`（metaparser 变更自动重跑 DodoeParser）。
2. 运行 `bin/editor-debug/Cakery.exe` 打开 `tests/Projects/OnlyOne`，查 engine.log 无资产加载 / sprite resolve 错误。
3. 验收：移动/改名纹理文件重开场景 Sprite 仍渲染；旧格式场景加载无错 + 一次告警，保存落新格式；C# 赋值 → 保存 → 重载 → 重读一致。
