# 引擎核心

| 文档 | 内容 |
|---|---|
| [core.md](core.md) | Runtime Core 总览:Application/SystemContext、事件、线程、反射、序列化、Project、生命周期所有权 |
| [configuration.md](configuration.md) | 配置系统:编译期 CMake 选项、运行期分层配置(ConfigSystem)、Debug 开关注册表、app/editor 配置文件 |
| [memory.md](memory.md) | 内存体系:Tier×Tag 分层、Linear/Pool/mimalloc 分配器、线程分配器与帧 epoch、STL 适配器、延迟删除 |
| [resource.md](resource.md) | 资源管理:ResourceManager、Asset/AssetManager/AssetDatabase/AssetHandle、importer、asset types |
| [tooling.md](tooling.md) | Parser、Generated、Sandbox:metaparser 与生成代码 |
| [code-style.md](code-style.md) | C++ 类型、命名、类布局、render pass 模式等代码规范 |
