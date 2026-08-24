# UI、Input、Window

## 目录

| 目录 | 关键类型 |
|---|---|
| `engine/src/runtime/function/ui/` | `UIManager`、UIElement、Widget、Panel、Image、Label、Button、layout、render batch |
| `engine/src/runtime/function/ui/imgui/` | `ImGuiBuilder`、ImGui style、draw packet |
| `engine/src/runtime/function/input/` | `InputManager`、`Input`、Action Map、InputAction、KeyCode、MouseCode |
| `engine/src/runtime/function/window/` | `Window`、`WindowManager`、window properties/types |

## Runtime UI

`UIManager` 拥有 UI root 与 UI update/render 数据。`UIElement` 提供树、transform、visibility、depth 与 layout 基础；`UIWidget`、`UIButton`、`UILabel`、`UIImage`、`UIPanel` 扩展具体控件。`UIRenderBatch` 将 UI 数据提供给 Render 模块。

`UILayoutLoader` 与 `UIPresetManager` 读取 UI document/preset。`UIInputRouter` 将输入事件分发给 interactive UI element。

## Debug ImGui

`ImGuiBuilder::SetupImGui` 创建 ImGui context 与 GLFW platform backend。`PrepareImGui` 开始一帧；`RenderImGui` 调用 `ImGui::Render` 并将 `ImDrawData` 序列化为 `ImGuiRenderPacket`。

`ImGuiPass` 位于 Render 模块，将 packet 上传到 transient vertex/index buffer，建立 graphics pipeline，并按 draw command 的 clip rect 与 texture 记录绘制命令。

## Input 与 Window

`WindowManager` 创建和持有 `Window`。`Window` 保存 native window、尺寸、标题、backend API 和窗口事件入口。`InputManager` 在每帧采样设备状态并评估启用的 Action Map；`Input` 提供 Action 查询 facade。Action 支持 `Started`、`Performed`、`Canceled` 三个事件阶段，以及 `IsActionDown`、`WasActionPressed`、`WasActionReleased` 和轴值查询。

Action 通过 Map 注册和绑定设备输入，查询可以使用 `Map/Action` 全名，也可以使用 Action 名称并由启用 Map 的优先级决定来源：

```cpp
Input::RegisterActionMap("Gameplay", 0);
Input::RegisterAction("Gameplay", "Jump", InputActionValueType::Button);
Input::BindKey("Gameplay", "Jump", KeyCode::Space);

if (Input::WasActionPressed("Gameplay/Jump")) {
    // handle jump
}

const auto subscription = Input::Subscribe(
    "Gameplay/Jump", InputActionPhase::Performed,
    [](const InputActionEvent& event) {
        // event.value contains the action value for this frame
    });
```

每个 Action 注册后获得稳定的 `InputActionId`。业务代码可以先通过 `FindActionId("Gameplay/Jump")`（或 `FindActionId("Gameplay", "Jump")`）解析出 ID，再用 ID 版查询/订阅，避免散落字符串：

```cpp
const auto jump_id = Input::FindActionId("Gameplay/Jump");
if (Input::WasActionPressed(jump_id)) { /* ... */ }

const auto sub = Input::Subscribe(jump_id, InputActionPhase::Performed,
    [](const InputActionEvent& event) { /* event.action_id 指向该 Action */ });
```

Map 也可以通过 `.doinput` 资源加载。资源支持 `priority`、`consume`、`press`、`hold` 和 `tap`：

```json
{
  "maps": [
    {
      "name": "Gameplay",
      "priority": 0,
      "consume": false,
      "actions": [
        {
          "name": "Jump",
          "type": "Button",
          "bindings": [
            { "type": "key", "key": 32, "interaction": "press" }
          ]
        }
      ]
    }
  ]
}
```

Context 是一个真正的栈，**只有被推入栈的 Map 才会被评估**，栈顶优先级最高：

- `PushInputContext(map)` 把 Map 移到栈顶（已在栈中则先移除再置顶）。
- `PopInputContext(map)` 把 Map 从栈中移除，恢复其下的栈顺序；被移除的 Map 正在进行的 Action 会被 `Canceled`。
- `enabled` 是独立的开关：栈中的 Map 若 `enabled=false` 会被跳过，适合"暂停某 Context 而不打乱栈顺序"。
- 加载 `.doinput` 时，启用的 Map 会按 `priority` 升序自动入栈（priority 最高的在栈顶）。
- Map 的 `consume` 为 true 时，会阻止低优先级 Map 读取**同一帧实际活动的具体控制**（键/鼠标按钮/指针），而不是整张 Map 的所有绑定；每帧只消费按下的控制，同帧按下又释放的控制也会被消费。

window 尺寸、logical size、pixel size 和 render viewport 分别由 Window 与 RenderViewTarget/RenderViewport 表示。
