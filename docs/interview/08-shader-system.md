# 08 Shader 体系与资源绑定（SPIRV-Cross / SRG / MeshBatch / DrawList）

对应面试题：项目 Shader 体系；ShaderResourceGroup 概念；MeshBatch、DrawList 概念。

## 一句话

> 着色器只用一份 Vulkan 风格 GLSL 编写，构建期经 glslangValidator → SPIR-V → **SPIRV-Cross → HLSL** → DXC → DXIL 生成 D3D12 产物，Vulkan 直接用 SPIR-V，OpenGL 运行时读 GLSL 源码（后端内重写 set/binding）；**SPIR-V 同时是所有后端统一的反射字节码来源**。绑定模型仿 UE：六个 ShaderParameterSet 声明式分组，等价于 UE 的 ShaderResourceGroup。

## 一、着色器资产与构建管线（"屏蔽三后端差异"的完整答案）

### 1. 编写：一份源码

- `engine/res/shaders/*.vert/.frag/.geom/.comp`：全部是 **Vulkan 风格 GLSL**（`layout(set=N, binding=M)`、push_constant 语法）；
- `shader_parameter_sets.glsl`：set/binding 宏约定（`DOE_SET_GLOBAL/VIEW/PASS/MATERIAL/PRIMITIVE/BINDLESS`），shader `#include` 使用；
- `shader_manifest.json`：清单驱动——name / source / entry_point / stage / platforms。

### 2. 构建：一条流水线（`engine/res/CMakeLists.txt`，全链可指认）

```text
GLSL 源码
  → glslangValidator -V  →  bin/<name>.spv          （Vulkan 直接使用）
  → spirv-cross --hlsl --shader-model 60            （SPIR-V → HLSL，中间产物）
  → dxc -T ps_6_0/vs_6_0/...  →  bin/<name>.dxil    （D3D12 使用）
```

### 3. 运行时分发（`ShaderLibrary::initialize`，shader_library.h/.cpp）

| 后端 | 加载内容 | 差异吸收点 |
|---|---|---|
| D3D12 | `.dxil` | 构建期 SPIRV-Cross 生成 HLSL 时已按 SPIR-V 的 decoration 保留矩阵布局/绑定语义 |
| Vulkan | `.spv` | 原生 |
| OpenGL | GLSL 源码 | 运行时 `InlineShaderIncludes` 内联宏文件；cutie GL 后端 GLSL 重写器把 `set=N,binding=M` 改写为 `binding=N*6+M`、push_constant→隐藏 UBO、分离采样器折叠（见 06） |

### 4. 反射：SPIR-V 是唯一权威

- 反射**优先读 `.spv` 字节码**（`ShaderReflector::ReflectBytecode`），运行时才有 `Reflect` 兜底；
- 实现在 `shader_reflection.cpp:86`：`spirv_cross::CompilerReflection` 解析出 `constant_buffers`（含每变量 name/offset/size）、`textures`、`samplers`、`vertex_inputs`、push constants；
- **三个后端共用同一份反射结果**——材质常量缓冲按反射偏移填充，绕开手工对齐（也是数学差异题的答案之一，见 10）。

### 5. 收尾表述

> "所以 GL 与 Vulkan/D3D12 的差异被压在两层：构建期用 SPIRV-Cross 把统一 IR 落成 HLSL，运行期用 GLSL 重写器吸收 GL 的绑定模型差异；中间的 SPIR-V 反射让三个后端共享同一套绑定布局与常量偏移。着色器作者只面对一种方言。"

---

## 二、ShaderResourceGroup 概念题

**先讲 UE 里的概念**：

> UE 的 FShaderResourceGroup（SRG）把一个 pass/材质用到的着色器参数（CBV/SRV/UAV/Sampler）按**更新频率与作用域**分组——典型分 Global / View / Pass / Material / Primitive / Bindless 组。价值有三：① PSO 只描述管线状态不包含具体资源，资源换组重绑不需要重建 PSO；② 按 set 粒度部分重绑，只有变化的组需要重新提交；③ 绘制排序时同一组的资源一致，命令可以合并复用。

**再映射到我的引擎**（两者同构，直接给代码证据）：

| UE 概念 | 我的实现 |
|---|---|
| SRG 分组 | `ShaderParameterSet` 六个 set：Global / View / Pass / Material / Primitive / Bindless（shader_parameter.h + shader_parameter_sets.glsl 一一对应） |
| set 编号 | `setRegisterSpaceIsDescriptorSet(true)`，register space 直接当 descriptor set |
| 参数声明宏 | `BEGIN_SHADER_PARAMETER_STRUCT / SHADER_PARAMETER / SHADER_PARAMETER_PUSH_CONSTANTS`（仿 UE 语法） |
| 布局生成 | `ShaderBindingReflector<Struct>::getOrCreateLayouts()` 遍历成员聚合各 set 的 BindingLayoutDesc，函数级 static 惰性缓存 + 全局静态缓存表 |
| 绑定生成 | `createBindingSets(cmd, layouts, params, resolveTex, resolveBuf)` 填充 GfxBindingSetDesc，RenderGraph 句柄经回调解析 |
| 提交 | `ShaderParameterBinder::bind` 按 6 个 set 顺序 `addBindingSet`，跳过未就绪 set |
| 消费 | `MeshDrawCommand` 持有固定 `StaticArray<GfxBindingSetHandle, kShaderParameterSetCount>`——绑定集是指令的一部分，可缓存可排序 |

具体到网格绘制：`LitMeshProcessor` 用四套 layout（Global/View/Primitive/Sampler），`ShadowMeshProcessor` 只用 Global/View——**组越少，缓存命中率越高**，这正是分组设计的收益。

---

## 三、MeshBatch / DrawList 概念题

**MeshBatch**（深度讲解见 03）：

> 一次提交前的"可绘制单元"：一个 mesh section + 一个材质实例，带各 pass 的相关性掩码（`MeshBatchPassMask`：Opaque/Shadow/Transparent）、绘制范围（index/instance range）、自定义包围盒。剔除和编命令都以 MeshBatch 为粒度，section 拆分让多材质模型每个 section 命中自己的状态。

**DrawList**：

> UE 里 MeshDrawList 是"一个 pass 的全部绘制命令的容器"，负责把 MeshBatch 经 MeshPassProcessor 加工成 MeshDrawCommand 并排序提交。我的实现叫 `MeshDrawList`：持有 sources（工作集）/ instances / shader data / commands 的并行数组，分 **cached（静态物体，命令常驻缓存）** 与 **dynamic（每帧重建）** 两条路径，另含供 GPU driven 用的 `gpu_buckets`（排序后同状态相邻分桶）与合批统计。排序键是 `pipeline→material→binding_set→VB→IB→绘制范围→depth_bucket` 字典序，排序后不透明做相邻实例/索引范围合并。

## 高频追问

- **为什么 GL 不走 SPIR-V？** → GL 后端设计上只支持 GLSL（4.5 无 SPIR-V 加载，且 SPIR-V 路线与统一源码方言冲突），用运行时重写器吸收差异（见 06 面试题 C）。
- **bindless 与 shader 变体的关系？** → manifest 层的变体：非 bindless 模式跳过 bindless 版 PS，命名 getter 按 `IsBindlessActive()` 在 `GBufferPS/GBufferNoBindlessPS` 间切换；没有完整 permutation 系统（对比 UE 的差异，见 11）。
- **材质 CB 怎么填充？** → 依赖 CB 反射的变量偏移（`MaterialSystem::buildConstantBufferData`），不是手写偏移——跨后端安全。
- **SRG 和 Bindless 什么关系？** → 正交：SRG 是"怎么分组"，Bindless 是第五组（Bindless set）的绑定方式（纹理走全局描述符表索引，不占传统槽位）。
