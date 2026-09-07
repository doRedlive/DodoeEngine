# Material 体系

本文覆盖 Dodoe 的材质体系:资产层(`Material` / `MaterialAsset`)与渲染层(`MaterialSystem`)的职责划分、模板/实例模型、解析(resolve)与失效(invalidate)机制。

代码位置:`engine/src/runtime/function/render/material/`(渲染层)、`engine/src/runtime/resource/asset/types/material_asset.h`(资产层)。

## 1. 总览

```text
.mesh 导入(Assimp)                      美术/编辑器
  → 逐 section 生成 MaterialProperties      │
  → 落盘 materials/*.domat(JSON)  ─────────┤ 保存/编辑
                                            ▼
资产层:Material(Object 子类,PPtr 纹理引用,s_material_cache)
       MaterialAsset(反射资产,AssetHandle<TextureAsset>)
                                            │ ResourceManager.loadObject<Material>
                                            ▼
渲染层:MaterialSystem
  MaterialTemplate(shader + 光栅状态 + BindingLayout,经 ShaderLibrary 解析)
  MaterialInstance(参数覆盖 + 纹理解析 + revision)
                                            │ getResolvedMaterial / getTextureBindingSet
                                            ▼
渲染管线(lit_mesh_processor / baseline passes)
```

## 2. 资产层

### 2.1 Material(render/material/material.h/.cpp)

`Object` 子类,纯数据资产,**不持有 GPU 资源**:

- 参数:`m_color`(Vector4f)、`m_emissive`(Vector3f)、`m_metallic`(0)、`m_roughness`(1)+ 4 个 `PPtr<Texture2D>`:base_color / normal / metallic_roughness / emissive;
- 序列化:`loadFromJson` / `saveToJson`(JSON,字段名与成员一一对应),落盘格式即 `.domat`;
- `Material::Create(ref, path)`:相对路径经 AssetManager 补全 → 加载(失败则用默认值并告警)→ 存入静态缓存 `s_material_cache`(InstanceID → Material);`Material::Shutdown()` 统一释放;
- `MaterialProperties`(material.h):轻量 POD 版本(纹理用 `FileID` 而非 PPtr),Mesh 导入时生成并写 `.domat`(见 resources.md §5)。

### 2.2 MaterialAsset(resource/asset/types/material_asset.h/.cpp)

带反射(`REFLECTION_TYPE` / `META(Enable)`)的 `Asset` 子类,`kStaticType = AssetType::Material`:

- 字段与 Material 相同,纹理引用用 `AssetHandle<TextureAsset>`(`META(Enable, AssetHandle)`);
- `loadFromSource` / `saveToSource` 同为 JSON 读写,记录 `m_meta.source_path`;
- 编辑器/序列化体系用,运行时游戏侧主要走 `Material`。

## 3. 渲染层:数据模型(material_system.h)

### 3.1 参数模型

| 类型 | 说明 |
|---|---|
| `MaterialParamType` | Float / Float2 / Float3 / Float4 / Color3 / Color4 / Texture2D / TextureCube / Int / Bool |
| `MaterialParamValue` | union:`Float f[4]` / `Int32 i[4]` / `Texture2D* texture`(16 字节) |
| `MaterialParamDef` | name、display_name(编辑器显示)、type、default/min/max 值 |

### 3.2 模板与实例

`MaterialTemplateDesc`(注册时的描述):

- `name`、`shader_name`(按约定映射为 `<shader_name>VS` / `<shader_name>PS`);
- 渲染状态:`GfxRasterState` / `GfxDepthStencilState` / `GfxBlendState`;
- `param_defs`:参数定义表;`permutation_defaults`:变体默认值;
- `computeHash()`:name + shader_name + 各参数(name, type)+ permutation 参与 hash。

`MaterialTemplate`:`desc` + 解析产物(`vertex_shader` / `pixel_shader` 句柄、`binding_layout`)+ `revision` + `resolved` 标志。

`MaterialInstanceDesc`:`name`、`template_name`、`param_overrides`(按参数名覆盖)、`permutation_overrides`。

`MaterialInstance`:`desc` + 指向模板的指针 + 解析产物(`textures`、`texture_descriptor_indices`(bindless 槽)、`sampler`、`metallic/roughness/ao`)+ `revision` + `resolved`。

`ResolvedMaterial`(最终输出):VS/PS/BindingLayout 句柄 + 三套渲染状态 + 纹理数组 + sampler + `parameter_data` + `revision`(= `tpl.revision ^ inst.revision`);`valid()` 要求三个句柄齐备。

## 4. 生命周期与解析流程(material_system.cpp)

`MaterialSystem` 为 `Managed<MaterialSystem, MaterialSystemCreateInfo>`,依赖四个服务:`ShaderLibrary`、`BindingLayoutCache`、`BindingSetCache`、`TextureManager`。

### 4.1 初始化与注册

```text
initialize → registerBuiltinTemplates
  内置 "GBuffer" 模板:shader_name="GBuffer",
  参数 = base_color / metallic_roughness / normal / emissive 四个 Texture2D
registerTemplate(desc)     重名拒绝,成功后 ++m_global_revision
createInstance(desc)       校验模板存在、实例重名;inst.tpl 指向模板
getOrCreateInstance(name, tpl, overrides)   查缓存 → create → resolve 一条龙
setInstanceParam(...)      写 param_overrides,++instance.revision,++m_global_revision
```

### 4.2 resolveTemplate

```text
shader_name → "<name>VS" / "<name>PS"(PS 缺失时回退 "<name>NoBindlessPS")
  → ShaderLibrary::findShader 取两个句柄
  → 由 VS+PS 的 ShaderReflectionData 合并生成 GfxBindingLayoutDesc
      (VolatileConstantBuffer / 纹理按 kind 转换 / Sampler,槽位去重)
  → BindingLayoutCache::getOrCreate 缓存布局
  → tpl.resolved = true,++tpl.revision
```

### 4.3 resolveInstance

```text
模板未解析则先 resolveTemplate
  → getResolvedParams:默认值 ← 实例 overrides 逐层覆盖
  → 纹理参数:TextureManager 解析;空值用 1×1 白 fallback 兜底,
     记录 descriptor index(bindless)
  → metallic / roughness / ao 标量参数写入实例字段
  → 创建默认 sampler;inst.resolved = true,++inst.revision
resolveAll():模板全部 → 实例全部
```

## 5. 渲染侧消费

- `getResolvedMaterial(instance_name, permutation_overrides, out)`:输出 `ResolvedMaterial`(当前 `parameter_data` 为空,CB 填充走下一条);
- `buildConstantBufferData(instance_name, cb_reflection, out)`:按 CB 反射变量的 name/offset/size,把参数值 memcpy 进对齐好的缓冲区(offset 越界报错跳过);
- `getTextureBindingSet(instance)`:传统(非 bindless)路径,构建 Set=Material 的布局(Sampler@1、BaseColor@2、MetallicRough@3)并经 `BindingSetCache` 缓存 binding set;纹理按 `textures[0]` / `textures[1]` 顺序取。

调用方:

| 调用方 | 用法 |
|---|---|
| `render_scene.cpp`(GpuScene 同步) | `getOrCreateInstance(name, "GBuffer", overrides)`(render_scene.cpp:432) |
| `lit_mesh_processor.cpp` | 每个绘制项 `getTextureBindingSet(mi)`(:173/:253) |
| `baseline_gbuffer_pass.cpp` / `baseline_pick_pass.cpp` | 传统路径纹理 binding set(:360/:194) |

## 6. 失效与 revision 机制

三级 revision 支持上层做增量同步/重缓存判断:

| 接口 | 行为 |
|---|---|
| `invalidateForShader(shader_name)` | 该 shader 的模板清空解析产物(unresolved),实例置 unresolved;各自 ++revision |
| `invalidateForTexture(texture)` | 引用该纹理的实例 ++revision(纹理内容变化) |
| `invalidateAll()` | 全部模板与实例置 unresolved |

`m_global_revision` 随任何注册/解析/失效递增,供全局"材质是否变化"的快速判断;`getTemplates()` / `getInstances()` 暴露只读遍历。

## 7. 设计要点

- **资产与渲染分离**:`Material` 只是参数容器;GPU 表达(shader、布局、纹理解析、binding set)全部在 `MaterialSystem`,资产可安全跨线程持有。
- **惰性解析**:`resolved` 标志 + `resolveAll` / 单项 resolve,shader 重载或纹理变化后按需重建。
- **bindless 优先**:实例只记录 descriptor index,纹理经全局 bindless 表访问;传统路径保留 `getTextureBindingSet` 兜底。
- **命名约定即契约**:模板 → shader 的映射完全靠 `<shader_name>VS/PS` 命名,新增材质模板需保证 ShaderLibrary 中存在同名条目(见 shader-system.md §3)。

## 8. 相关文档

- [shader-system.md](shader-system.md):ShaderLibrary、反射与 Set/Binding 约定。
- [resources.md](resources.md):`Material::Create` 在 ResourceManager 链路中的位置、Mesh 导入生成 `.domat` 的流程。
