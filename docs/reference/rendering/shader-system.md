# Shader 体系

本文覆盖 Dodoe 的着色器管理与参数绑定体系:shader 源码与清单、ShaderLibrary 加载与反射、参数 Set/Binding 约定、声明式参数结构宏,以及 Global/Material 两级 shader map。

代码位置:`engine/src/runtime/function/render/shader/`,shader 源文件与清单在 `engine/res/shaders/`。

## 1. 总览

```text
shader_manifest.json + *.vert/*.frag/*.geom/*.comp / bin/*.spv|*.dxil
        │
        ▼
ShaderLibrary(manifest 驱动加载 + SPIRV 反射缓存)
        │  findShader / getReflection
        ├──────────────────────────────┐
        ▼                              ▼
GlobalShaderMap                MaterialShaderMap        ShaderReflector
(程序名 → ShaderProgram)      (程序名 → 变体 → Program) (字节码 → ReflectionData)
                                       │
                                       ▼
        ShaderParameter 宏 + ShaderBindingReflector(声明式参数结构 → BindingLayout/BindingSet)
                                       │
                                       ▼
        ShaderParameterBinder::bind(按 6 个 Set 绑定到 GraphicsState)
```

## 2. 资源约定(res/shaders/)

| 内容 | 说明 |
|---|---|
| `shader_manifest.json` | 全部 shader 的清单:name / source(不含扩展名)/ entry_point / stage / platforms |
| `*_pass.vert/.frag/.geom/.comp` | GLSL 源码(OpenGL 后端直接使用) |
| `bin/*.spv` `bin/*.dxil` | 离线编译产物(Vulkan/D3D12 使用;`.spv` 同时作为所有后端的反射字节码来源) |
| `shader_parameter_sets.glsl` | set/binding 宏约定(见 §4) |

`ShaderManifest`(shader_manifest.h):`loadFromFile` 解析清单,按 name 建索引;`StageToExtension` 把 stage 映射为 `.vert/.frag/.geom/.comp` 扩展名。`ReadShaderFile` 以引擎 res 目录为根读二进制文件。

## 3. ShaderLibrary(shader_library.h/.cpp)

`Managed<ShaderLibrary, ShaderLibraryCreateInfo>` 单例式管理器,`initialize` 流程:

```text
m_manifest.loadFromFile("shaders/shader_manifest.json")
  → 按后端选平台:d3d12→".dxil" / vulkan→".spv" / opengl→GLSL 源码
  → 逐条目:
      platforms 过滤(当前后端不在列表则跳过)
      非 bindless 模式跳过 GBufferPS / SpritePS / UIPS / ForwardLitPS
      OpenGL:读源码并 InlineShaderIncludes(内联 shader_parameter_sets.glsl,
              正则把 #define DOE_XXX n 展开为字面量)
      其他后端:读 shaders/bin/<source><ext><backend_ext>
      GDrawCommandList.createShader → m_shaders[name]
      反射:优先读 bin/<source><ext>.spv 字节码 ReflectBytecode(SPIRV),
            无则运行时 Reflect;结果缓存到 m_reflections[name]
```

- `findShader(name)` / `getReflection(name)`:按清单名(如 `"LitVS"`)查询。
- 命名 getter:`getLitVertexShader`、`getGBufferPixelShader`(内部按 `RenderSettings::IsBindlessActive()` 在 `GBufferPS` / `GBufferNoBindlessPS` 间切换,`ForwardLitPS`、`UIPS` 同理)等,覆盖管线全部 pass(含 Shadow / Skybox / ToneMapping / FXAA / Pick / Gizmo / Sprite / ImGui / GpuCulling 等 compute)。
- `reset()` / `shutdown()`:释放全部句柄并 `ClearStaticBindingLayoutCaches()`(配合 §6 的静态布局缓存)。

## 4. Set/Binding 约定

`ShaderParameterSet`(shader_parameter.h)与 GLSL 宏(shader_parameter_sets.glsl)一一对应:

| Set | 枚举 | GLSL 宏 | 内容 |
|---|---|---|---|
| 0 | Global | `DOE_SET_GLOBAL` | 时间等全局常量(binding 0) |
| 1 | View | `DOE_SET_VIEW` | ViewConstants(0)、Transforms(1) |
| 2 | Pass | `DOE_SET_PASS` | PassConstants(0)、Input0~7(1–8)、Sampler(9) |
| 3 | Material | `DOE_SET_MATERIAL` | Constants(0)、Sampler(1)、BaseColor(2)、MetallicRough(3)、Normal(4)、Emissive(5) |
| 4 | Primitive | `DOE_SET_PRIMITIVE` | DrawData / MaterialData(0) |
| 5 | Bindless | `DOE_SET_BINDLESS` | `Texture2D u_Textures[1024]`(0) |

C++ 侧 `shader_bindings` 命名空间(shader_parameter.h)持有同一套槽位常量;GLSL 侧 shader 通过 `#include "shader_parameter_sets.glsl"` 使用宏(见 gbuffer_pass.frag)。所有布局以 `setRegisterSpaceIsDescriptorSet(true)` 把 register space 直接当 descriptor set。

## 5. 反射:ShaderReflector(shader_reflection.h/.cpp)

核心数据 `ShaderReflectionData`:

- `constant_buffers`:`ShaderCBReflection{name, set, slot, size, variables[]}`,变量含 name/offset/size(MaterialSystem 据此填充 CB,见 material-system.md §5);
- `textures`:`ShaderTextureReflection{name, set, slot, kind, dimension, array_size}`;
- `samplers` / `vertex_inputs`(semantic + format + location);
- `uses_push_constants` / `push_constant_size`。

`ShaderResourceKind` 枚举覆盖 TextureSRV/UAV、Typed/Structured/Raw Buffer、ConstantBuffer(含 Volatile)、Sampler、RayTracingAccelStruct、PushConstants、SamplerFeedbackTextureUAV;`ShaderResourceKindToBindingItem` 把 kind + slot + array_size 转为 `GfxBindingLayoutItem`。

`ShaderReflector` 静态接口:

| 接口 | 用途 |
|---|---|
| `Reflect(shader, name)` | 对已创建的 GPU shader 运行时反射 |
| `ReflectBytecode(bytecode, stage, name)` | 对 SPIRV 字节码离线式反射(优先路径) |
| `ValidateAgainstLayout(reflection, layout, out_error)` | 校验反射结果与 BindingLayout 一致 |

## 6. 声明式参数结构(shader_parameter.h)

仿 UE 风格的参数声明宏,把"结构体成员"同时变成布局描述与绑定描述:

```cpp
BEGIN_SHADER_PARAMETER_STRUCT(ViewParams, View)
    SHADER_PARAMETER(ConstantBuffer, kViewBindingConstants, constants)
    SHADER_PARAMETER(TextureSRV, kPassBindingInput0, scene_color)
    SHADER_PARAMETER_PUSH_CONSTANTS(0, MyPush, push)
END_SHADER_PARAMETER_STRUCT(forEachMember(constants); forEachMember(scene_color); ...)
```

- `ShaderParameter<Type, Set, Binding, ValueT>` 模板特化:`TextureSRV`(值可为 `RenderGraphTextureHandle` 或裸 `GfxTextureHandle`)、`Sampler`、`ConstantBuffer`(RenderGraph volatile / 裸 buffer)、`PushConstants`;每个特化提供 `makeLayoutItem()` 与 `addToBindingSet()`。
- `ShaderBindingReflector<Struct>::getOrCreateLayouts()`:遍历成员聚合各 set 的 `GfxBindingLayoutDesc`,惰性创建并缓存(函数级 static),同时注册到全局静态缓存表(`RegisterStaticBindingLayoutCache`),供 `ShaderLibrary::reset` 时统一清空,避免句柄悬挂。
- `createBindingSets(command_list, layouts, params, resolveTex, resolveBuf)`:遍历成员填充 `GfxBindingSetDesc`;RenderGraph 句柄经回调解析为 RHI 句柄,PushConstants 跳过。
- `ShaderParameterBinder::bind`:按 `kShaderParameterSetCount = 6` 个 Set 顺序 `addBindingSet`(跳过未就绪的 set)。

## 7. GlobalShader 与 MaterialShader

`ShaderProgram`(shader_program.h):一个可用程序的各 stage 句柄(VS/HS/DS/GS/PS/CS)+ 各 stage 的 `ShaderReflectionData*`;`isGraphics()` = VS+PS 齐备,`isCompute()` = CS 存在。`ShaderDomain` 区分 Global / Material。

- `GlobalShaderMap`(global_shader.h/.cpp):程序名 → `GlobalShader`(单 ShaderProgram)。`registerStage(program, stage, handle, reflection)` 逐 stage 注册,`findProgram(program)` 取用。
- `MaterialShaderMap`(material_shader.h/.cpp):程序名 → `MaterialShader` → 变体名 → `ShaderProgram`,支持一个材质 shader 的多变体(permutation);`registerStage(program, variant, stage, ...)` 注册,`finalize()` 收尾,`findProgram(program, variant)` 查询。

## 8. 与其他模块的关系

- **MaterialSystem**(material-system.md):`resolveTemplate` 按"`shader_name + "VS" / "PS"(回退 "NoBindlessPS")`"命名约定从 ShaderLibrary 取句柄,并用 VS/PS 反射合并生成材质 binding layout;`buildConstantBufferData` 依赖 CB 反射的变量偏移。
- **RenderPipeline / Pass**:经 ShaderLibrary 命名 getter 或 GlobalShaderMap 取 shader;Pass 参数结构用 §6 宏声明,经 ShaderBindingReflector 生成布局与 binding set,ShaderParameterBinder 完成绑定。
- **RenderSettings**:bindless 开关决定加载哪组 PS 变体;后端 API 类型决定读编译产物还是 GLSL 源码。
