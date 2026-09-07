# Parser、Generated、Sandbox

## Parser

`engine/src/metaparser/`

| 子目录 | 职责 |
|---|---|
| `parser/cursor/` | Clang cursor 与类型解析 |
| `parser/language_types/` | class、enum、field、method、type information 模型 |
| `parser/meta/` | metadata config、metadata collection、utility |
| `parser/generator/` | reflection、serializer、script binding generator |
| `parser/template_manager/` | 生成模板管理 |

Parser 输入带 reflection/metadata 标记的 C++ 头，输出 runtime serializer/reflection 与 script bridge 所需代码。

## Generated

`engine/src/_generated/` 包含 reflection、serializer、script native calls、bindings 和 glue 生成输出。生成文件由 parser 与生成脚本维护，不作为手写实现入口。

`engine/src/precompile/` 聚合 parser 输入头并调用生成步骤。

## Sandbox

`engine/src/sandbox/`

`SandboxApp` 继承 `Application` 并挂载 `SandboxLayer`。`SandboxLayer` 提供 `attach`、`detach`、`updateTick`、`renderTick` 实验入口。`main.cpp` 加载工程后创建并运行 Application。
