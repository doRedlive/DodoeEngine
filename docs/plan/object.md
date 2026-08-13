Object / Asset / C# 桥接体系收敛整改方案
状态： Draft
日期： 2026-08-10
范围： Dodoe runtime、资源系统、脚本绑定和场景序列化
1. 结论与目标
当前工程同时存在两条彼此绕开的资源路径：
1. Object -> InstanceID -> C# Object wrapper：已经用于 Texture2D、Sprite 和 PPtr<T>。
2. Asset / AssetManager / AssetHandle<T>：负责资产扫描、导入元数据、依赖和 TextureAsset、SpriteAsset、MeshAsset 等资产记录。
两者目前没有形成所有权和加载关系。C# Resources.Load<T> 直接调用 TextureManager / SpriteLoader，绕过 AssetManager；因此导入设置、稳定资产身份、热重载和更多资产类型都无法自然进入 C# 的 Object 引用体系。
整改目标是保留 Object，但将其严格定位为 native 拥有、已加载、可被 C# 引用的运行时对象；Asset 则保持为 磁盘资产及其导入元数据的唯一真相来源。
源文件 / .meta / 导入设置
          │
          ▼
AssetManager ── Asset（稳定 AssetId、依赖、导入设置）
          │ LoadRuntime<T>
          ▼
Object（native 运行时 Texture2D / Sprite / Material / Mesh ...）
          │ InstanceID + generation
          ├──────────► PPtr<T>（序列化资产身份，按需解析）
          └──────────► C# Object wrapper（只持有 native 句柄）
本方案不要求 Entity、GameObject、Component、System 或 Behaviour 继承 Object。它们分别是世界句柄、托管视图或 ECS 数据，不具有独立、共享的 native 资源生命周期。
2. 当前状态与问题
区域
当前实现
问题
native Object
Object 用 InstanceID -> Object* 和 FileID -> InstanceID 两张表管理对象
仅有对象注册；没有统一的资源加载契约、对象销毁通知或 generation。
C# Object
GreenCake.Object 缓存 InstanceID -> wrapper，按 native 类型名手写创建 Texture、Sprite
仅支持两个类型；缓存永不清理；IsValid 只检查 ID 非零，native 已销毁时可能误报有效。
C# Resources
按 typeof(T) 手写分支，直接 native load texture/sprite
不通用，绕过 AssetManager，路径通常被解析为绝对路径。
Object 引用
PPtr<T> 序列化 FileID(path + file_uuid) 和 uuid
get() 只能查已加载对象，不具备通用按需加载；路径派生 ID 对改名/迁移不稳健。
Asset 系统
Asset 与 TextureAsset、SpriteAsset 等不继承 Object
合理的层次，但尚未产出或管理对应 Object；TextureAsset::m_gpu_texture 当前没有赋值点。
组件桥接
生成器可把 PPtr<T> 字段转换成 C# Object wrapper
实际组件只有 PPtr<Sprite>；生成器中还有针对 PPtr<Texture> 的过时特判，实际纹理类型是 Texture2D。
3. 目标模型与硬性约束
3.1 类型边界
类型
身份 / 生命周期
是否继承 Object
Texture2D、TextureCubemap、Sprite、Mesh、Material、Shader、AnimationClip、AudioClip
native 独立实例，可共享、缓存、被 C# 和组件引用
是
TextureAsset、SpriteAsset、MeshAsset 等
磁盘资产记录、导入数据、依赖和 stable AssetId
否
Entity、GameObject、Component
World/Scene 生命周期或 ECS 存储中的数据
否
帧内 RenderGraph handle、CommandContext
帧内临时资源
否
Object 不能成为 C# 侧的万物基类。一个类型只有同时满足以下条件才进入该体系：
1. native 端拥有其实际生命周期；
2. 它有独立、可共享的运行时身份；
3. 它需要作为引用穿过 C++ / C# 或存入 PPtr<T>；
4. 它不是 ECS 组件数据或帧临时对象。
3.2 身份模型
- AssetId：持久且稳定的资产 GUID；创建 .meta 时生成，文件移动或改名不变。
- InstanceID：一次进程运行中已加载 Object 的短生命周期 ID；只用于 native/C# 调用，不能作为序列化主键。
- generation：随 InstanceID 返回和校验，防止 ID 重用或 native 销毁后 C# wrapper 误认新对象。
- PPtr<T>：持久化 AssetId（以及需要时的 subObjectId）；不持久化 InstanceID。它可以为空，且在资源未加载时仍是合法引用。
现有 JSON 中的 { file_id: { path, file_uuid }, uuid } 必须继续可读。迁移后保存新格式，例如：
{
  "asset_id": "9d914c...",
  "sub_object_id": "main",
  "legacy_path": "optional during migration"
}
加载旧格式时，优先由旧 file_uuid 映射 AssetId；仅在缺少 UUID 时以 path 查找并输出一次可定位的迁移告警。保存时统一写新格式。
4. 运行时加载与引用契约
4.1 Asset 到 Object
AssetManager 新增或统一下面的职责：
template<class TObject>
TObject* AssetManager::loadRuntimeObject(AssetId asset_id);

template<class TObject>
TObject* AssetManager::loadRuntimeObjectByPath(const String& path);
- 检查 Asset 类型与 TObject 的匹配；不匹配时返回明确错误，不能 static_cast 后继续运行。
- 同一 (AssetId, RuntimeObjectType, subObjectId) 在一个运行时只创建一个 cached Object。
- TextureAsset 加载时读取导入设置、创建并登记 Texture2D，并将 m_gpu_texture 设为该对象。
- SpriteAsset 加载时解析其源 TextureAsset，创建/获取 Sprite；像素密度、pivot、slice 从 SpriteAsset 而非 SpriteLoader 的硬编码默认值生成。
- Resources.Load<T> 只能调用这条路径；不再直接绕过 AssetManager 调 Texture2D::Load 或 SpriteLoader::Load。
TextureManager 可继续负责 GPU 资源创建和缓存，但它不再以原始路径作为对外资产加载 API；其输入应来自 AssetManager 已解析的资产身份和导入数据。
4.2 PPtr
保留“不隐式 I/O”的语义，区分两种 API：
T* PPtr<T>::getLoaded() const;          // 仅查当前已加载对象，不触发加载
T* PPtr<T>::resolve() const;            // 经 AssetManager 按 AssetId 加载并返回
bool PPtr<T>::isAssigned() const;       // 是否有资产引用，不代表已加载
- 渲染热路径若需要对象，显式使用 getLoaded()，缺失时走 fallback 或调度预加载。
- 场景激活、组件同步、脚本读取资源字段等非热路径可使用 resolve()。
- PPtr<T>(Object*) 必须校验运行时类型；错误类型不得只靠 static_cast 存入。
- 移除 SpriteRendererSystem 中“从路径单独重新加载 Sprite”的特殊恢复逻辑，改为 sr.sprite.resolve()。
4.3 Object 生命周期与 C# 有效性
native 侧新增只读查询：
struct ObjectHandle { InstanceID id; UInt32 generation; };
bool Object::isAlive(ObjectHandle handle);
const char* Object::getObjectTypeName(ObjectHandle handle);
C# Object 持有该 ObjectHandle，其 IsValid 调 native isAlive，不再仅判断 InstanceID != 0。对象销毁时：
1. native 从 Object registry 移除对象并使 generation 失效；
2. 向 Script 系统发出对象失效通知；
3. C# 从 wrapper cache 移除该 handle，已有 wrapper 变为 invalid；
4. 所有属性访问先做有效性检查并抛出一致的 ObjectDisposedException 或返回约定 fallback。
C# cache 可以继续确保“同一有效 handle 对应同一个 wrapper”，但必须在销毁时删除。使用 WeakReference<Object> 可降低无外部引用 wrapper 的滞留；无论强弱缓存，都必须有 native 销毁失效机制。
5. C# 桥接设计
5.1 统一装箱工厂
替换 Object.CreateManagedWrapper 中的手写 switch。native 注册可公开的 Object 类型及其 stable type key，C# 在启动时注册 factory：
Object.RegisterFactory("Texture2D", handle => new Texture(handle));
Object.RegisterFactory("TextureCubemap", handle => new TextureCubemap(handle));
Object.RegisterFactory("Sprite", handle => new Sprite(handle));
第一阶段可保留 switch 作为 fallback，但新类型必须通过注册表接入，不能继续增加 Resources.Load 的 if (typeof(T) == ...) 分支。
5.2 Resource API
目标 API：
public static T? Load<T>(string path) where T : Object;
public static T? Load<T>(AssetId assetId) where T : Object;
public static T? Find<T>(AssetId assetId) where T : Object; // 不触发加载
- path 仅在边界处解析成 AssetId；C# 不把绝对路径作为对象身份或场景序列化值。
- 为 TextureCubemap 添加 C# wrapper；后续 Object 子类按同一机制注册。
- 第一批至少暴露通用只读数据：Name、AssetId、IsValid、实际类型；具体资源的属性和变更接口按类型新增。
5.3 组件生成代码
保持 PPtr<T> ↔ T : Object 的生成规则，但生成器需：
1. 从 PPtr<T> 生成实际对应的 C# 类型；
2. 在 native setter 做对象类型校验；
3. 删除 PPtr<Texture> 特判，改为基于 resolve() 的通用实现；
4. 对 null、未加载、失效 handle 分别生成正确行为；
5. 生成的 C# 返回类型允许 null，不能用 null-forgiving operator 掩盖资源未加载。
6. 分阶段实施
Phase 0：盘点和保护网
- 列出所有 Object 派生类、PPtr<T> 使用点、Asset 类型和 C# wrapper。
- 为现有 Texture/Sprite load、scene serialization、C# wrapper identity 补回归测试。
- 增加日志：资源加载来源、path 到 FileID/AssetId 解析、PPtr 未解析原因。
完成条件： 可以复现并测试当前 SpriteRendererComponent.sprite 的 C# 赋值、保存、重载场景和重新读取。
Phase 1：稳定资产身份
- 在 importer 生成/维护稳定 AssetId，不再让路径哈希充当主身份。
- Asset registry 维护 AssetId <-> 当前路径；支持 rename/move 后查找。
- Serializer 同时支持旧 FileID 格式和新 AssetId 格式。
完成条件： 移动纹理文件后，引用它的场景无需手工修复且可加载。
Phase 2：AssetManager 产出运行时 Object
- 先落地 TextureAsset → Texture2D 和 SpriteAsset → Sprite。
- 将 TextureManager / SpriteLoader 的对外入口改为接收资产解析结果；旧 path API 标记 deprecated，仅为兼容层服务。
- 修复 TextureAsset::m_gpu_texture 未赋值的问题。
完成条件： C++、C#、场景加载三条路径加载同一资源时拿到同一个 native InstanceID。
Phase 3：Object handle 生命周期
- 引入 ObjectHandle(id, generation)、Native_ObjectIsAlive 和销毁通知。
- 迁移 C# Object cache；添加 ObjectDisposed 行为测试。
- 明确 shutdown 顺序：先通知脚本对象失效，再销毁资源缓存和 Object registry。
完成条件： 卸载资源或 shutdown 后，任何旧 C# wrapper 的 IsValid == false，不会继续命中已释放 native 指针。
Phase 4：PPtr 与代码生成器
- 为 PPtr<T> 增加 getLoaded / resolve / isAssigned。
- 迁移 SpriteRenderer 和其他场景引用到 resolve()；删除特殊按路径补载。
- 修改脚本绑定生成器，消除 PPtr<Texture> 特判并生成类型校验。
完成条件： 任意 Object 子类可作为组件字段类型接入，无需为该类型新增手写 glue 分支。
Phase 5：扩展与清理
- 接入 TextureCubemap，再按需求接入 Material、Mesh、Shader、AnimationClip、AudioClip。
- 删除废弃的直接路径加载 API 和冗余缓存。
- 完成性能测试和资产热重载策略。
完成条件： 新增一个 Object 资产类型只需要声明 runtime 类、Asset loader 和 C# factory/生成元数据；不需要修改 Resources 的类型分支或组件桥接核心。
7. 重点改动文件
目的
主要文件
Object ID、generation、失效事件
runtime/core/object/object.h/.cpp
弱引用与资源解析
runtime/core/object/pptr.h
旧/新引用序列化兼容
runtime/core/meta/serializer/serializer.h
资产注册和 runtime object load
runtime/resource/asset/*、runtime/resource/resource_manager.*
Texture/Sprite 运行时实例
runtime/function/render/texture/texture.*、runtime/service/sprite/sprite_loader.cpp
C# Object cache、factory、Resources API
scriptcore/Source/Object.cs、Resource/Resources.cs、Render/*
C#/C++ glue
runtime/function/script/script_glue.cpp、metaparser/parser/generator/script_binding_generator.cpp
组件引用恢复
runtime/function/world/systems/sprite_renderer_system.cpp
8. 验收测试
必须至少覆盖以下场景：
1. Resources.Load<Texture>(path)、Resources.Load<Sprite>(path) 和 C++ AssetManager 对同一资产返回同一 runtime Object。
2. C# 多次按 InstanceID 查找同一活对象，得到同一 wrapper；销毁后 wrapper 失效且 cache 条目删除。
3. C# 给 SpriteRendererComponent.sprite 赋值，C++ 获得类型正确的 PPtr<Sprite>；错误对象类型被拒绝。
4. 场景保存后，PPtr 使用 AssetId；读取旧路径格式的场景仍可运行并在下次保存时迁移。
5. 资源未加载时 getLoaded() 返回 null，resolve() 能加载并返回正确对象。
6. 移动/改名资产文件后，已保存场景仍能解析其引用。
7. Texture、Sprite、TextureCubemap 的 native 类型到 C# wrapper 映射完整；未知类型返回可诊断错误而不是静默裸 Object。
8. 引擎 shutdown、资源卸载和脚本 reload 不发生悬空 native 指针访问。
9. 风险与兼容策略
- 序列化兼容： 先双读、后单写；保留旧 FileID/path 解析至少一个发布周期。
- 性能： resolve() 只能在明确的非热路径触发 I/O；渲染循环坚持 getLoaded() 并预加载。
- 脚本兼容： 保留 InstanceID 只读属性和现有 Texture.Load / Sprite.Load 作为过渡 API；将其实现改为 AssetManager 路径。
- 渐进迁移： 首批仅 Texture/Sprite，确认所有权、生命周期和序列化稳定后再扩展其他资源类型。
- 回退： 每阶段独立提交；新序列化读取失败时可切回 legacy resolver，不能在运行时悄悄创建以路径哈希为身份的替代资产。
10. 推荐的第一批提交顺序
1. 补测试和加载/引用日志。
2. 引入稳定 AssetId 与旧序列化兼容读取。
3. 让 TextureAsset、SpriteAsset 生成并缓存对应 runtime Object。
4. 将 C# Resources.Load 切至 AssetManager。
5. 完成 Object generation、C# cache 失效和 native destroy 通知。
6. 重构 PPtr<T> / binding generator，删除 Texture 特判与 SpriteRenderer 补载特例。
7. 接入 TextureCubemap 和下一批 Object 资产。
完成后，Object 体系的职责会非常明确：它不是 ECS 或所有 C# 类型的基类，而是 C++/C# 共享运行时资源的安全句柄层；AssetManager 则成为所有可持久化资产身份、导入与加载的唯一入口。