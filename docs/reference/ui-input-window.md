# UI、Input、Window

## 目录

| 目录 | 关键类型 |
|---|---|
| `engine/src/runtime/function/ui/` | `UIManager`、UIElement、Widget、Panel、Image、Label、Button、layout、render batch |
| `engine/src/runtime/function/ui/imgui/` | `ImGuiBuilder`、ImGui style、draw packet |
| `engine/src/runtime/function/input/` | `InputManager`、`Input`、KeyCode、MouseCode、ActionState |
| `engine/src/runtime/function/window/` | `Window`、`WindowManager`、window properties/types |

## Runtime UI

`UIManager` 拥有 UI root 与 UI update/render 数据。`UIElement` 提供树、transform、visibility、depth 与 layout 基础；`UIWidget`、`UIButton`、`UILabel`、`UIImage`、`UIPanel` 扩展具体控件。`UIRenderBatch` 将 UI 数据提供给 Render 模块。

`UILayoutLoader` 与 `UIPresetManager` 读取 UI document/preset。`UIInputRouter` 将输入事件分发给 interactive UI element。

## Debug ImGui

`ImGuiBuilder::SetupImGui` 创建 ImGui context 与 GLFW platform backend。`PrepareImGui` 开始一帧；`RenderImGui` 调用 `ImGui::Render` 并将 `ImDrawData` 序列化为 `ImGuiRenderPacket`。

`ImGuiPass` 位于 Render 模块，将 packet 上传到 transient vertex/index buffer，建立 graphics pipeline，并按 draw command 的 clip rect 与 texture 记录绘制命令。

## Input 与 Window

`WindowManager` 创建和持有 `Window`。`Window` 保存 native window、尺寸、标题、backend API 和窗口事件入口。`InputManager` 在每帧更新键盘、鼠标和 action state；`Input` 提供静态查询 facade。

window 尺寸、logical size、pixel size 和 render viewport 分别由 Window 与 RenderViewTarget/RenderViewport 表示。
