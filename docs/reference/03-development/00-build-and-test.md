# 构建与测试说明

## 构建方式

本项目使用 CMake 构建，仓库中包含多个构建预设。常用命令：

- 编辑器调试：
  cmake --build --preset msvc-editor-debug

- 沙盒调试：
  cmake --build --preset msvc-sandbox-debug

## 目录与产物
- `build/`：CMake 生成目录
- `bin/`：构建产物输出目录

## 推荐开发流程
1. 确认构建预设
2. 完整构建
3. 运行测试或沙盒工程
4. 修改公共模块时优先检查测试
