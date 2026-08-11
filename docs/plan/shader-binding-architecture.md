# Shader 绑定架构（最终方案）

> 状态：目标架构
>
> 本文定义 Dodoe 的唯一 Shader、参数与资源绑定模型。它取代此前按资源类型分 set 的方案和旧的 GLSL / HLSL 双源构建方案。

## 1. 目标

绑定系统应接近 Unreal Engine 的 Shader Parameter Struct 与参数集合模型：Shader 描述自己需要的参数，渲染系统按参数的所有权、更新频率和生命周期提供数据，RHI 负责把同一逻辑模型映射到 DX12、Vulkan 与 OpenGL。

系统必须满足：

- GLSL 是唯一的 Shader 作者入口，同时产出 SPIR-V 与 DXIL。
- Shader 参数按资源所有权分组，而不是按 CBV / SRV / UAV / Sampler 类型分组。
- Pass 不直接手写底层 binding layout、register space 或 descriptor set。
- Material、View、Primitive、Pass 和全局资源具有明确的创建、更新和缓存边界。
- 运行时反射只用于验证与资产加载；常规渲染不依赖临时反射拼装 layout。
- 相同的逻辑参数布局在所有后端具有相同语义。

## 2. 基本原则

### 2.1 参数组按所有权划分

`set` / `register space` 的含义是参数组的所有者，而非资源种类。一个参数组可以同时含有常量缓冲、纹理、采样器和 buffer。

```text
Global      set 0 / space 0
View        set 1 / space 1
Pass        set 2 / space 2
Material    set 3 / space 3
Primitive   set 4 / space 4
Bindless    set 5 / space 5
```

### 2.2 每个 set 内 binding 唯一

同一参数组内，所有资源共享一条连续的 binding 编号空间，不能出现同一个 `(set, binding)` 对应多个资源种类的情况。

```text
Material set 3
  binding 0  MaterialConstants       ConstantBuffer
  binding 1  BaseColorTexture        TextureSRV
  binding 2  MaterialSampler         Sampler
  binding 3  NormalTexture           TextureSRV
```

这保证 Vulkan descriptor set 合法；DX12 可以映射为 `b0, t1, s2, t3, space3`；OpenGL 仍按各自资源 namespace 绑定低编号 slot。

### 2.3 资源类型不是参数组

禁止以下模型：

```text
set 0 = 所有 ConstantBuffer
set 1 = 所有 Sampler
set 2 = 所有 SRV
set 4 = 所有 UAV
```

它会把 RHI 实现细节暴露给 Shader 和 Pass，无法表达资源的更新频率、缓存方式与所有权，也会让一个 Pass 被迫手工组合多个无语义的 binding set。

### 2.4 参数组是显式契约

Shader 中使用的参数必须属于已定义的参数组。参数组的名称、set 编号、成员顺序、资源类型、数组长度和可见 Shader stage 都是 ABI 的一部分。

## 3. 参数组定义

| 参数组 | Set / Space | 所有者 | 更新频率 | 典型内容 |
|---|---:|---|---|---|
| Global | 0 | `RenderFrameContext` | 每帧 | 时间、环境、全局常量、全局只读资源 |
| View | 1 | `RenderView` | 每 View | 相机、矩阵、视口、曝光、裁剪参数 |
| Pass | 2 | `IRenderPass` | 每次执行 Pass | GBuffer、光照输入、后处理输入输出 |
| Material | 3 | `MaterialInstance` | 材质变更时 | 材质常量、纹理、采样器、材质 buffer |
| Primitive | 4 | `MeshDrawCommand` / instance data | 每 Draw 或每批次 | transform、对象 ID、实例数据、骨骼数据 |
| Bindless | 5 | `DescriptorTableManager` | 全局增量更新 | bindless Texture / Sampler / Buffer 数组 |

### 3.1 Global

Global 参数只能由渲染帧上下文写入。它不应包含具体视图、材质或对象数据。跨整个 frame 复用的环境贴图、蓝噪声、时间和全局配置属于此组。

### 3.2 View

每个 `RenderView` 拥有独立的 View 参数集。编辑器多视口、分屏与立体渲染不得共享同一 View binding set。

### 3.3 Pass

Pass 参数描述一次 RenderGraph pass 的输入与输出资源。RenderGraph 负责解析资源句柄并在执行时创建对应 binding set。Pass 参数不保存跨帧的材质或 Primitive 状态。

### 3.4 Material

Material 参数是材质 shader interface 的一部分。Material 模板定义布局，MaterialInstance 只保存值和资源引用；实例修改后使自己的 parameter set 失效并按需重建。

材质不拥有 View、Pass 或 Primitive 参数，材质系统也不能从所有反射资源中创建一个混合 layout。

### 3.5 Primitive

Primitive 参数由 Mesh Draw Command 提供。静态对象可缓存，动态对象使用每帧参数分配器或实例 buffer；不应为每个 draw 创建长期存在的 descriptor set。

### 3.6 Bindless

Bindless 是独立、全局、稳定的 descriptor table。Material 常量保存 bindless index，而不是为每个 Material 复制 Texture descriptor。

支持 bindless 的后端使用 set 5 的 descriptor array；不支持时保留相同 Material 参数接口，以常规 Material set 实现回退路径。能力差异必须由 RHI capability 显式表达，不能假定 OpenGL 已支持完整 bindless 或 compute 功能。

## 4. Shader 作者模型

GLSL 是唯一的 Shader 源码。Shader 使用共享的参数组常量和声明宏；业务 Shader 不直接填写裸数字。

```glsl
#include "shader_parameter_sets.glsl"

layout(set = DOE_SET_VIEW, binding = DOE_VIEW_BINDING_CONSTANTS)
uniform ViewParameters {
    mat4 u_ViewProjection;
    vec4 u_CameraPosition;
};

layout(set = DOE_SET_MATERIAL, binding = DOE_MATERIAL_BINDING_CONSTANTS)
uniform MaterialParameters {
    vec4 u_BaseColor;
    float u_Roughness;
};

layout(set = DOE_SET_MATERIAL, binding = DOE_MATERIAL_BINDING_BASE_COLOR)
uniform texture2D u_BaseColorTexture;

layout(set = DOE_SET_MATERIAL, binding = DOE_MATERIAL_BINDING_SAMPLER)
uniform sampler u_MaterialSampler;

layout(set = DOE_SET_PRIMITIVE, binding = DOE_PRIMITIVE_BINDING_CONSTANTS)
uniform PrimitiveParameters {
    mat4 u_LocalToWorld;
};
```

每个具体 Shader 的 binding 名称和编号由其参数布局定义生成。成员新增、重排、修改类型或数组长度都视为 ABI 变更，并导致相关 shader、layout、material 和 PSO 缓存失效。

## 5. 引擎参数模型

引擎以类似 Shader Parameter Struct 的类型描述参数布局，而不是让 Pass 直接构造 `GfxBindingLayoutItem`。

```cpp
BEGIN_SHADER_PARAMETER_STRUCT(MaterialParameters, ShaderParameterSet::Material)
    SHADER_PARAMETER(ConstantBuffer, 0, MaterialConstants)
    SHADER_PARAMETER(TextureSRV,     1, BaseColorTexture)
    SHADER_PARAMETER(Sampler,        2, MaterialSampler)
    SHADER_PARAMETER(TextureSRV,     3, NormalTexture)
END_SHADER_PARAMETER_STRUCT()
```

参数系统负责生成：

- `ShaderParameterLayout`：参数组的静态 ABI 描述；
- `GfxBindingLayoutDesc`：某一 set 的 RHI layout；
- `ShaderParameterSet`：实际资源与常量的绑定值；
- `ShaderParameterSetCache`：可缓存参数组的 binding set；
- `ShaderParameterBinder`：在 draw / dispatch 前按固定 set 顺序绑定。

`ShaderParameterLayout` 至少包含：

```text
set、binding、resource kind、array size、byte size、stage visibility、name、update frequency
```

缓存键必须包含上述 ABI 属性以及设备标识；不能只按 slot 或资源对象地址缓存。

## 6. Pipeline Layout 与绑定顺序

每个 Pipeline 的 layout 由 Shader 变体声明的参数组集合生成，按 set 从小到大排序。未使用的参数组可以缺失；Vulkan descriptor set 的空洞由 RHI 统一处理，Pass 不关心。

```text
Example mesh pipeline
  set 0  Global
  set 1  View
  set 3  Material
  set 4  Primitive
  set 5  Bindless (optional)
```

Pipeline layout 的创建、根签名创建、layout cache 与 PSO cache 必须以完整 layout signature 为键。`c_MaxBindingLayouts` 是 RHI 实现限制，不应改变参数组语义；当前六个标准 set 位于限制之内。

## 7. Material 与 Mesh Draw

MaterialTemplate 定义 Material 参数布局与 shader variant；MaterialInstance 填充该布局；MeshPassProcessor 只组合已有参数组：

```text
Frame Context       -> Global parameter set
RenderView          -> View parameter set
RenderGraph Pass    -> Pass parameter set
MaterialInstance    -> Material parameter set
MeshDrawCommand     -> Primitive parameter set
DescriptorTable     -> Bindless parameter set
```

Mesh Draw Command 保存 Pipeline、参数组引用、几何信息和排序键。它不保存分散的 `binding_sets` 向量，也不依赖“第 N 个 binding set 是 sampler”这类位置约定。

## 8. 反射与验证

SPIR-V 是运行时反射的唯一来源。反射必须读取：

- `DecorationDescriptorSet`；
- `DecorationBinding`；
- 资源种类及 SRV / UAV 可写性；
- 数组长度；
- constant / push constant 的大小和成员偏移；
- Shader stage visibility。

反射输出使用完整资源键：

```text
(set, binding, kind, array_size)
```

反射的职责是验证与资产加载：

1. 同一 Pipeline 的各 Shader stage 对共享资源必须声明相同 ABI；
2. 每个反射资源必须在对应参数组 layout 中存在；
3. 同一 `(set, binding)` 不允许资源类型、数组长度或大小冲突；
4. layout 中多出的资源必须显式标记为 optional，不能静默忽略；
5. 诊断信息必须包含 shader、参数组、set、binding、资源名称和 stage。

运行时不应通过反射临时推导 Material 或 Pass 的 ownership。ownership 来自参数结构和 shader metadata，反射只检查两者一致。

## 9. 后端映射

### DX12

逻辑 set 映射为 HLSL register space。一个参数组在同一个 space 中可包含 CBV、SRV、Sampler 和 UAV；其统一 binding 编号映射为对应 register type 的 register index。

```text
set 3 / binding 0 ConstantBuffer -> b0, space3
set 3 / binding 1 TextureSRV      -> t1, space3
set 3 / binding 2 Sampler         -> s2, space3
```

根签名由完整参数组 layout 自动生成。root constants、root descriptor 与 descriptor table 的选择属于 RHI policy，不暴露给 Shader 或 Pass。

### Vulkan

逻辑 set 直接映射 descriptor set，binding 直接映射 descriptor binding。`registerSpaceIsDescriptorSet` 必须对同一 Pipeline 的全部 regular layout 一致。不得使用 `VulkanBindingOffsets` 或任何将编号写入 SPIR-V 后再在引擎侧修正的偏移机制。

### OpenGL

OpenGL 后端忽略 set，但保留资源类型和 binding slot；UBO、texture unit、sampler、image unit 与 buffer binding 按各自 namespace 绑定。后端应验证实际 capability，并显式选择 bindless 或常规资源绑定实现。

## 10. Shader 构建与资产

构建链固定为：

```text
GLSL source
  -> glslangValidator
  -> SPIR-V (Vulkan 字节码 + 反射源)
  -> spirv-cross HLSL
  -> dxc
  -> DXIL
```

图形与 compute Shader 使用同一条链路。中间 HLSL 仅存在于 build 目录；源树不维护独立的 legacy HLSL Shader。Shader manifest 记录源文件、stage、变体、产物、反射 layout signature 与目标后端能力。

## 11. 约束与禁止项

- 禁止使用 `256 / 128 / 384` 等资源类型偏移编号。
- 禁止用资源类型决定 set / space。
- 禁止 Pass 手写 layout 顺序并依赖 binding set 的数组下标。
- 禁止 MaterialSystem 接管 View、Pass 或 Primitive 资源。
- 禁止反射合并时仅按 `(slot, kind)` 去重。
- 禁止同一 Pipeline 混用 `registerSpaceIsDescriptorSet` 配置。
- 禁止把不支持 bindless 的后端伪装为完全兼容。
- 禁止 Shader 源、反射、RHI layout 之间存在编号转换补丁。

## 12. 验收标准

一个 Shader 资源从 GLSL 声明到 DX12、Vulkan、OpenGL 的最终绑定，始终保有相同的逻辑身份：

```text
ParameterGroup + set + binding + resource kind + array size
```

新增一个材质参数或一个 Pass 输入时，开发者只修改参数结构与 Shader 声明；layout、binding set、反射验证、PSO signature 和三后端映射由统一参数系统处理。任何 shader/layout 不匹配都在构建期或 Pipeline 创建期给出可定位的错误。
