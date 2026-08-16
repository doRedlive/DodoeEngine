# Agent 阅读说明

此文档面向 AI Agent / 代码助手，描述仓库结构、关键入口与修改规范。

## 建议阅读顺序
1. `docs/reference/README.md`
2. `docs/reference/01-architecture/00-project-overview.md`
3. `docs/reference/01-architecture/01-system-architecture.md`
4. `docs/reference/02-code-map/00-repository-map.md`
5. `docs/reference/03-development/00-build-and-test.md`

## 关键入口
- `engine/src/`：核心实现
- `engine/scripts/`：脚本与辅助逻辑
- `tests/`：测试与示例
- `scripts/`：构建/辅助脚本
- `docs/reference/`：项目说明与模块解释

## 修改前必做
- 确认改动属于哪个模块
- 查阅是否已有文档说明
- 复用现有接口与测试

## 修改后必做
- 运行构建与相关测试
- 更新 `docs/reference`（若影响接口或模块边界）
