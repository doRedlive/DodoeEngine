# dodoe UI 系统架构设计方案

> 参考 Unity UGUI 架构，适配 dodoe RenderCommand 管线。不参考当前旧 UI 代码。

---

## 1. 设计目标

| 目标 | 说明 |
|------|------|
| **不破坏渲染管线** | UI 通过 `RenderCommandQueue` 提交绘制数据，走 RenderScene → RenderPass 路径 |
| **布局与渲染分离** | 类似 UGUI 的 `RectTransform`（布局）和 `CanvasRenderer`（渲染）解耦 |
| **批次合并** | 所有 UI 元素合并为一次 instanced draw（与现有 SpritePass 同理） |
| **声明式布局文件** | JSON 描述 UI 结构，支持未来可视化编辑器 |
| **C# 脚本绑定** | 按 element id 查找并绑定回调，与现有 ScriptGlue 集成 |

---

## 2. 设计决策：为什么不使用组件模型

### Unity 的 UI 组件模型

Unity 中一切皆 `GameObject` + `Component`，UI 也不例外。每个 UI 元素是一个 GameObject，上面挂载多个组件：

```
GameObject "PlayButton"
  ├── RectTransform      ← 替代普通 Transform
  ├── CanvasRenderer     ← 持有 mesh/材质数据
  ├── Image              ← 绘制按钮背景图
  └── Button             ← 处理点击事件
```

### dodoe 不走组件模型的原因

| 考量 | 组件模型 | dodoe 类继承模型 |
|------|---------|-----------------|
| **场景混入** | UI 实体混在场景 Entity 树，物理/碰撞/AI 系统都要过滤 | UI 是独立树，不污染其它系统 |
| **Transform** | 用 `RectTransform` 替换 `Transform`（hack） | `UIElement` 内置 anchor/pivot，无需替换 |
| **坐标系统** | 需区分 `Canvas.RenderMode`（Overlay/Camera/World） | UI 天然屏幕空间，零配置 |
| **组合粒度** | 一个按钮 = 4 个组件，序列化/反序列化开销大 | 1 个 `UIButton` 类，自包含 |
| **引擎一致性** | dodoe 的 RenderSystem、Material、Texture 都是类继承，不是 ECS | UI 保持相同范式 |
| **序列化** | UI 数据在 scene/prefab 文件，和场景数据耦合 | 独立 `.ui` JSON 文件，可单独加载/卸载 |

### UGUI 的组件是无奈之举，不是最佳实践

Unity 的组件模型是引擎整体的约束——`MonoBehaviour` 必须是 Component，UI 没有选择余地。但即便是 Unity，新 UI 系统（UI Toolkit / UI Document）也正在抛弃组件模型，转而采用类似 Web 的声明式树结构（UXML + USS）。

### 如何保证"可组合"

不是组件，但可以通过 **子元素嵌入** 实现组合。例如：

```jsonc
{
    "type": "Panel",          // 背景面板
    "children": [
        {
            "type": "Image",  // 图标（相当于 Image 组件）
            "id": "icon",
            "texture": "..."
        },
        {
            "type": "Label",  // 文字（相当于 Text 组件）
            "id": "label",
            "text": "Play"
        }
    ]
}
// 一个带图标和文字的复杂按钮 = Panel + Image + Label
// 对应 Unity: GameObject(Image+Button) + child(Text)
```

如果需要按钮行为，再包裹一层 `UIButton`（内置状态机 + 回调）。组合通过**树形嵌套**而非**组件列表**完成——这和 Web 的 DOM 树、Flutter 的 Widget tree 一致。




---

## 3. 可视化编辑器支持：元数据驱动的 Inspector

不用组件模型，Inspector 如何自动展示可编辑属性？通过 `offsetof` 元数据注册表。

### Inspector 效果对比

Unity 中一个按钮在 Inspector 里分多个组件面板：
```
RectTransform → Image → Button
```

dodoe 中同一个 `UIButton` 节点把所有属性平铺分组展示：

```
+-- UIButton "play_btn" ------+
| -- 布局 ------------------- |
| Position:  [100] [200]     |
| Size:      [200] [64]      |
| Anchor: Min [0.5] [0.5]   |
|         Max [0.5] [0.5]   |
| -- 外观 ------------------- |
| Preset:    play             |
| Label:     "Play"           |
| Color:     [? 1,1,1,1]     |
| -- 交互 ------------------- |
| Interactable:  ?            |
| Raycast Target: ?           |
| -- 事件 ------------------- |
| OnClick:  > PlayGame()     |
+----------------------------+
```

用户体验完全一致——属性按类别分组，而非按"组件"分组。

### UIMetaRegistry

```cpp
// do@Redlive -- ui_meta.h

enum class UIPropertyType : ui8 {
    Float,
    Vector2f,
    Color,
    String,
    Resource,   // 纹理/预设 ID（可视化编辑器弹选择器）
    Bool,
    Enum,
    Callback,   // 事件绑定（不通过 offsetof）
};

struct UIPropertyMeta {
    String name;                // Inspector 显示名
    UIPropertyType type;        // 控件类型
    Size_t offset;              // 成员变量在类中的偏移（Callback 类型时为 0）
    String category;            // Inspector 分组名："Layout", "Appearance", ...
    Optional<Float> rangeMin;   // Float 类型的滑块范围
    Optional<Float> rangeMax;
    DynamicArray<String> enumValues;  // Enum 类型的选项列表
};

class UIMetaRegistry {
public:
    static UIMetaRegistry& Self();

    template<typename T>
    void registerType(StringView typeName, DynamicArray<UIPropertyMeta> properties);

    const DynamicArray<UIPropertyMeta>& getProperties(StringView typeName) const;
    DynamicArray<StringView> getRegisteredTypes() const;

private:
    UnorderedMap<String, DynamicArray<UIPropertyMeta>> m_registry;
};
```

### 注册示例

每个控件类型的 `.cpp` 文件中注册自己的属性：

```cpp
// do@Redlive -- ui_button.cpp

static struct UIButtonMetaInit {
    UIButtonMetaInit() {
        UIMetaRegistry::Self().registerType<UIButton>("Button", {
            // === Layout ===
            {"Position",    UIPropertyType::Vector2f, offsetof(UIElement, m_position), "Layout"},
            {"Size",        UIPropertyType::Vector2f, offsetof(UIElement, m_size),     "Layout"},
            {"Anchor Min",  UIPropertyType::Vector2f, offsetof(UIElement, m_anchorMin),"Layout"},
            {"Anchor Max",  UIPropertyType::Vector2f, offsetof(UIElement, m_anchorMax),"Layout"},
            // === Appearance ===
            {"Preset",      UIPropertyType::Resource, offsetof(UIButton, m_presetId), "Appearance"},
            {"Color",       UIPropertyType::Color,    offsetof(UIWidget, m_color),    "Appearance"},
            // === Interaction ===
            {"Interactable",   UIPropertyType::Bool, offsetof(UIInteractive, m_interactable),  "Interaction"},
            {"Raycast Target", UIPropertyType::Bool, offsetof(UIInteractive, m_raycastTarget), "Interaction"},
            // === Events ===
            {"OnClick",     UIPropertyType::Callback, 0, "Events"},
        });
    }
} s_uiButtonMetaInit;

// do@Redlive -- ui_label.cpp
static struct UILabelMetaInit {
    UILabelMetaInit() {
        UIMetaRegistry::Self().registerType<UILabel>("Label", {
            {"Position",    UIPropertyType::Vector2f, offsetof(UIElement, m_position), "Layout"},
            {"Size",        UIPropertyType::Vector2f, offsetof(UIElement, m_size),     "Layout"},
            {"Text",        UIPropertyType::String,   offsetof(UILabel, m_text),       "Appearance"},
            {"Font Size",   UIPropertyType::Float,    offsetof(UILabel, m_fontSize),   "Appearance",
             .rangeMin = 8, .rangeMax = 128},
            {"Color",       UIPropertyType::Color,    offsetof(UIWidget, m_color),     "Appearance"},
            {"Alignment",   UIPropertyType::Enum,     offsetof(UILabel, m_alignment),  "Appearance",
             .enumValues = {"Left", "Center", "Right", "UpperLeft", /*...*/}},
        });
    }
} s_uiLabelMetaInit;
```

### 编辑器如何使用

```
可视化编辑器启动
  |
  +-- 从 UIMetaRegistry 遍历所有注册类型
  |     |
  |     +-- "Panel"  → [Position, Size, Color, ClipChildren, ...]
  |     +-- "Label"  → [Text, FontSize, Color, Alignment, ...]
  |     +-- "Button" → [Preset, Label, Color, OnClick, ...]
  |     +-- "Image"  → [TextureId, UV, Color, PreserveAspect, ...]
  |     +-- "StackLayout" → [Direction, Spacing, ChildAlignment, ...]
  |     +-- "GridLayout"  → [Columns, Spacing, CellSize, ...]
  |
  +-- Hierarchy 面板：UI 树 → 树形控件展示
  |     |
  |     +-- 点击节点 → 按 typeName 查 UIMetaRegistry
  |           |
  |           +-- Inspector 面板动态生成：
  |                 for each PropertyMeta in properties:
  |                   switch(type):
  |                     Float   → 输入框/滑块（依据 rangeMin/rangeMax）
  |                     Color   → 颜色选择器
  |                     String  → 文本框
  |                     Bool    → 复选框
  |                     Enum    → 下拉菜单
  |                     Resource→ 资源选择器弹窗
  |                     Callback→ "添加绑定" 按钮
  |                   |
  |                   +-- 读取：*(T*)(node_ptr + meta.offset)
  |                   +-- 写入：*(T*)(node_ptr + meta.offset) = newValue
```

### 新增控件类型的成本

新增 `UISlider`？只需：
1. 继承 `UIInteractive`，写 `onLayout()` + `onCollectRenderData()`
2. 在 `.cpp` 里注册属性：

```cpp
static struct UISliderMetaInit {
    UISliderMetaInit() {
        UIMetaRegistry::Self().registerType<UISlider>("Slider", {
            {"Min Value",  UIPropertyType::Float, offsetof(UISlider, m_minValue), "Value",
             .rangeMin = 0, .rangeMax = 1},
            {"Max Value",  UIPropertyType::Float, offsetof(UISlider, m_maxValue), "Value",
             .rangeMin = 0, .rangeMax = 1},
            {"Value",      UIPropertyType::Float, offsetof(UISlider, m_value),    "Value",
             .rangeMin = 0, .rangeMax = 1},
            {"Direction",  UIPropertyType::Enum,  offsetof(UISlider, m_direction),"Appearance",
             .enumValues = {"LeftToRight", "RightToLeft", "BottomToTop", "TopToBottom"}},
            {"OnValueChanged", UIPropertyType::Callback, 0, "Events"},
        });
    }
} s_uiSliderMetaInit;
```

**编辑器零改动**，Inspector 自动生成 Slider 面板。

### Hierarchy 面板

```
+-- Hierarchy --------------------+
| > root                          |
|   > logo         [Image]       |
|   > title        [Label]       |
|   v menu_buttons [StackLayout] |
|       play_btn     [Button]   |
|       settings_btn [Button]   |
|       quit_btn     [Button]   |
+---------------------------------+
```

与 Unity Hierarchy 完全一致：显示元素 id + 类型标记；支持拖拽重排、右键增删；选中联动 Inspector 属性面板。区别只是 Unity 显示 GameObject 名，dodoe 显示 `UIElement::m_id`。


---

## 4. 架构总览

```
┌──────────────────────────────────────────────────────────────────┐
│  C# Script Layer                                                 │
│  UIService.LoadLayout("main_menu.ui")                            │
│  UIService.Find<Button>("play_btn").OnClick += () => {...}       │
└───────────────────────────┬──────────────────────────────────────┘
                            │ ScriptGlue (native bindings)
┌───────────────────────────▼──────────────────────────────────────┐
│  C++ UIManager (SystemContext 持有)                               │
│  ┌─────────────┐  ┌──────────────┐  ┌─────────────────────────┐ │
│  │ UIElement   │  │ UILayoutLoader│  │ UIInputRouter          │ │
│  │ 树 (布局)    │  │ (JSON→树)     │  │ (hit-test + 事件分发)    │ │
│  └─────────────┘  └──────────────┘  └─────────────────────────┘ │
│        │                                                         │
│        │ 每帧收集所有可见元素                                       │
│        ▼                                                         │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │ UIRenderBatch (构建 UI 绘制数据)                              │ │
│  │ → DynamicArray<UIInstance> → RenderCommandQueue::SubmitUI() │ │
│  └─────────────────────────────────────────────────────────────┘ │
└───────────────────────────┬──────────────────────────────────────┘
                            │ SpscQueue<RenderCommand>
┌───────────────────────────▼──────────────────────────────────────┐
│  RenderSystem (渲染线程)                                          │
│  RenderScene → UIPass (RenderPhase::UI)                         │
│  → instanced draw (1 quad mesh × N instances)                   │
└──────────────────────────────────────────────────────────────────┘
```

**与 UGUI 的对照关系：**

| UGUI | dodoe UI |
|------|----------|
| `RectTransform` (布局) | `UIElement` (anchor/pivot/size 计算) |
| `CanvasRenderer` (持有 mesh) | `UIInstance` (绘制时生成的 GPU 数据) |
| `Canvas.BuildBatch()` | `UIRenderBatch::collect()` → `RenderCommandQueue::SubmitUI()` |
| `Graphic.OnPopulateMesh()` | `UIElement::onCollectRenderData()` |
| `EventSystem` + `GraphicRaycaster` | `UIInputRouter` + `UIElement::hitTest()` |
| `CanvasUpdateRegistry` | `UIManager` 帧循环（布局 → 输入 → 收集 → 提交） |

---

## 5. 帧循环流程

```
UIManager::update(deltaTime)
  │
  ├─ Phase 1: Layout（脏标记驱动）
  │   └─ UIElement::ensureLayout() → onLayout() 递归
  │
  ├─ Phase 2: Input（鼠标/键盘 → hit-test → 事件分发）
  │   └─ UIInputRouter::process(mousePos, keyEvents)
  │       ├─ hitTest(root, pos) → 最顶层 UIInteractive
  │       └─ dispatchEnter/Exit/Press/Release/Click
  │
  └─ Phase 3: Render Collect（收集绘制数据）
      └─ UIRenderBatch::collect(root) → 递归遍历
          └─ 每个可见元素调用 onCollectRenderData(batch)
              → 生成 UIInstance {pos, size, uv, color, atlas_index, depth}
      └─ RenderCommandQueue::SubmitUI(std::move(batch))
```

**关键：UI 不直接持有 GPU 资源，只持有绘制描述数据。** 渲染线程收到 `SubmitUI` 命令后才创建 instanced buffer 并绘制。

---

## 6. 核心类设计

### 7.1 UIElement（排版基类）

```cpp
// do@Redlive
class UIElement {
    // ======================== 成员变量 ========================
    identifier m_id{entt::null};
    UIElement* m_parent{nullptr};
    DynamicArray<Scope<UIElement>> m_children;

    Bool m_visible{true};
    Vector2f m_anchorMin{0, 0};
    Vector2f m_anchorMax{0, 0};
    Vector2f m_pivot{0, 0};
    Vector2f m_position{0, 0};
    Vector2f m_size{100, 100};
    Thickness m_padding{};
    Thickness m_margin{};

    mutable Bool m_layoutDirty{true};
    mutable Vector2f m_cachedScreenPos{0, 0};
    mutable Vector2f m_cachedLayoutSize{0, 0};

public:
    // ======================== 禁用拷贝 ========================
    UIElement(const UIElement&) = delete;
    UIElement& operator=(const UIElement&) = delete;

    // ======================== 树结构 ========================
    [[nodiscard]] UIElement* getParent() const { return m_parent; }
    void addChild(Scope<UIElement> child);
    Scope<UIElement> removeChild(UIElement* child);
    [[nodiscard]] UIElement* findChildById(identifier id) const;

    // ======================== 可见性 ========================
    [[nodiscard]] Bool isVisible() const { return m_visible; }
    void setVisible(Bool visible) { m_visible = visible; }

    // ======================== 布局属性 ========================
    void setAnchor(Vector2f min, Vector2f max);
    void setPivot(Vector2f pivot);
    void setSize(Vector2f size);
    void setPosition(Vector2f position);
    void setPadding(const Thickness& padding);
    void setMargin(const Thickness& margin);

    [[nodiscard]] Vector2f getScreenPosition() const;
    [[nodiscard]] Vector2f getLayoutSize() const;
    [[nodiscard]] Rect getScreenRect() const;

    // ======================== 虚方法（子类重写）======================
    virtual void onLayout() {}
    virtual void onCollectRenderData(class UIRenderBatch& batch) {}
    [[nodiscard]] virtual Bool hitTest(Vector2f localPos) const;

protected:
    // ======================== 内部方法 ========================
    void invalidateLayout(Bool propagate = true);
    void ensureLayout() const;
};

### 7.2 UIInteractive（交互基类）

```cpp
// do@Redlive
class UIInteractive : public UIElement {
    // ======================== 成员变量 ========================
    Bool m_interactable{true};
    Bool m_raycastTarget{true};
    Bool m_isHovered{false};
    Bool m_isPressed{false};

public:
    // ======================== 回调 ========================
    std::function<void()> onClick;
    std::function<void()> onHoverEnter;
    std::function<void()> onHoverLeave;
    std::function<void(Bool)> onPressChanged;

    // ======================== 交互状态 ========================
    [[nodiscard]] Bool isHovered() const { return m_isHovered; }
    [[nodiscard]] Bool isPressed() const { return m_isPressed; }
    [[nodiscard]] Bool isInteractable() const { return m_interactable; }
    void setInteractable(Bool interactable) { m_interactable = interactable; }

    // ======================== raycastTarget ========================
    [[nodiscard]] Bool isRaycastTarget() const { return m_raycastTarget; }
    void setRaycastTarget(Bool enabled) { m_raycastTarget = enabled; }

protected:
    // ======================== 由 UIInputRouter 调用 ========================
    virtual void onMouseEnter();
    virtual void onMouseExit();
    virtual void onMouseDown();
    virtual void onMouseUp(Bool isInside);
};
```

### 7.3 UIWidget（可绘制基类）—— 对应 UGUI 的 Graphic

```cpp
// do@Redlive
class UIWidget : public UIInteractive {
    // ======================== 成员变量 ========================
    Color m_color{1, 1, 1, 1};
    Float m_alphaThreshold{0};

public:
    // ======================== 颜色 ========================
    void setColor(Color color) { m_color = color; }
    [[nodiscard]] Color getColor() const { return m_color; }
    [[nodiscard]] Float getAlpha() const { return m_color.a; }
    void setAlpha(Float alpha) { m_color.a = alpha; }

    // ======================== Alpha HitTest ========================
    void setAlphaHitTestThreshold(Float threshold) { m_alphaThreshold = threshold; }
};
```

### 7.4 具体控件

```cpp
// do@Redlive

// === UIImage ===
class UIImage final : public UIWidget {
    identifier m_textureId{entt::null};
    Rect m_uvRect{0, 0, 1, 1};
    Bool m_flipH{false}, m_flipV{false};
    FillMethod m_fillMethod{FillMethod::None};
    Float m_fillAmount{1};
    Bool m_preserveAspect{false};

public:
    void setTexture(identifier textureId);
    void setTexture(String texturePath);
    void setUVRect(const Rect& uv);
    void setFlipped(Bool horizontal, Bool vertical);
    void setFillMethod(FillMethod method, Float fillAmount);
    void setPreserveAspect(Bool preserve);

protected:
    void onCollectRenderData(UIRenderBatch& batch) override;
};

// === UILabel ===
class UILabel final : public UIWidget {
    String m_text;
    Int m_fontSize{16};
    identifier m_fontId{entt::null};
    TextAnchor m_alignment{TextAnchor::MiddleCenter};
    Float m_lineSpacing{1};

public:
    void setText(String text);
    void setFontSize(Int size);
    void setFont(identifier fontId);
    void setTextAlignment(TextAnchor alignment);
    void setLineSpacing(Float spacing);

protected:
    void onCollectRenderData(UIRenderBatch& batch) override;
    void onLayout() override;
};

// === UIButton ===
class UIButton final : public UIInteractive {
    enum class ButtonState { Normal, Hovered, Pressed, Disabled };

    ButtonState m_state{ButtonState::Normal};
    UIImage* m_icon{nullptr};
    UILabel* m_label{nullptr};
    identifier m_presetId{entt::null};

public:
    void setPreset(identifier presetId);
    void setLabel(String text);
    void setStateImage(ButtonState state, identifier textureId, Rect uv);
    void setStateColor(ButtonState state, Color color);

protected:
    void onCollectRenderData(UIRenderBatch& batch) override;
    void onMouseEnter() override;
    void onMouseExit() override;
    void onMouseDown() override;
    void onMouseUp(Bool isInside) override;

private:
    void transitionTo(ButtonState state);
};

// === UIPanel ===
class UIPanel final : public UIElement {
    Color m_bgColor{0, 0, 0, 0};
    identifier m_bgTextureId{entt::null};
    Rect m_bgUVRect{0, 0, 1, 1};
    Optional<NineSliceMargins> m_nineSlice{};
    Bool m_clipChildren{false};

public:
    void setBackgroundColor(Color color);
    void setBackgroundImage(identifier textureId, Rect uv = {0, 0, 1, 1});
    void setNineSlice(NineSliceMargins margins);
    void setClipChildren(Bool clip);

protected:
    void onCollectRenderData(UIRenderBatch& batch) override;
};

// === UIStackLayout ===
class UIStackLayout : public UIElement {
    LayoutDirection m_direction{LayoutDirection::Vertical};
    Float m_spacing{0};
    Alignment m_childAlignment{Alignment::Start};

public:
    void setDirection(LayoutDirection dir);
    void setSpacing(Float spacing);
    void setChildAlignment(Alignment align);

protected:
    void onLayout() override;
};

// === UIGridLayout ===
class UIGridLayout : public UIElement {
    Int m_columns{3};
    Vector2f m_spacing{0, 0};
    Vector2f m_cellSize{100, 100};

public:
    void setColumns(Int count);
    void setSpacing(Vector2f spacing);
    void setCellSize(Vector2f size);

protected:
    void onLayout() override;
};
```

---

## 7. 与渲染管线的集成

### 7.1 新增 RenderCommand 类型

```cpp
// do@Redlive — 在 render_command.h 中新增
enum class RenderCommandType : ui8 {
    // ... 已有命令 ...
    SubmitUIBatch,     // 每帧提交一次，包含所有 UI 绘制数据
};
```

### 7.2 UIRenderBatch（UI 层构建，游戏线程）

```cpp
// do@Redlive
struct alignas(16) UIInstance {
    Vector2f position;
    Vector2f size;
    Vector2f uvMin;
    Vector2f uvMax;
    UInt32 color;
    UInt32 atlasIndex;
    Float depth;
    UInt32 flags;
    Rect clipRect;
};
static_assert(sizeof(UIInstance) == 64, "UIInstance must be 64 bytes");

class UIRenderBatch {
    DynamicArray<UIInstance> m_instances;

public:
    void addQuad(const UIInstance& instance);
    void addNineSlice(const UIInstance& instance, const NineSliceMargins& margins);
    void clear();

    void submit();  // → RenderCommandQueue::SubmitUI(batch)
};
```

### 7.3 RenderCommandQueue 新增接口

```cpp
// do@Redlive — render_command_queue.h 新增
class RenderCommandQueue {
public:
    // ... 已有方法 ...
    static void SubmitUI(DynamicArray<UIInstance> instances);
};
```

内部实现：构造 `RenderCommand{SubmitUIBatch, .ui_instances = std::move(instances)}` 放入 SPSC 队列。

### 7.4 RenderScene 存储 UI 数据

```cpp
// do@Redlive — render_scene 新增 ui_scene_info.h
class UISceneInfo {
    DynamicArray<UIInstance> m_instances;
    Bool m_dirty{false};

public:
    void upload(GfxCommandList cmdList, GpuScene& gpuScene);
};

// RenderScene 中新增
class RenderScene {
    // ... 已有 ...
    UISceneInfo m_uiSceneInfo;
};
```

### 7.5 新增 UIPass（渲染线程）

```cpp
// do@Redlive — render_pipeline 新增 ui_pass.h/cpp
class UIPass {
public:
    void record(GfxCommandList cmdList,
                const UISceneInfo& uiInfo,
                const GpuScene& gpuScene,
                const RenderView& view);

private:
    void renderInstances(GfxCommandList cmdList,
                         const DynamicArray<UIInstance>& instances,
                         Size_t count);
};
```

**渲染方式（与 SpritePass 完全一致）：**
1. 创建 transient instance buffer，大小 = `count × sizeof(UIInstance)`
2. 上传到 GPU
3. 使用与 SpritePass 相同的 quad mesh、相同的 bindless texture array
4. 调用 `drawIndexed(6, count)` 一次 instanced draw

**与 SpritePass 的区别：**
- SpritePass 做 frustum culling（相机视锥体裁剪）；UIPass 不做（UI 始终全屏可见）
- SpritePass 每个实例有自己的 world transform；UI 实例直接是屏幕坐标
- UIPass 使用正交投影矩阵（屏幕空间），SpritePass 使用相机矩阵

### 7.6 渲染阶段

```cpp
// render_phase.h 中新增
enum class RenderPhase {
    // ... 已有 ...
    UI,           // UI 渲染 — 在所有场景内容之后、ImGui 之前
    DebugUI,      // ImGui 调试 UI
    Present,
};
```

---

## 8. UIManager 集成到 SystemContext

```cpp
// do@Redlive — system_context.h 变更
class SystemContext {
    // ... 已有成员 ...
    Scope<UIManager> m_uiManager{nullptr};

public:
    UIManager* getUIManager() { return m_uiManager.get(); }

    // initializeModules() 中新增:
    void initializeModules() {
        // ... 已有初始化 ...
        auto windowSize = m_windowManager->getWindowSize();
        m_uiManager = create_scope<UIManager>(windowSize);
    }

    // 每帧 tick 中:
    void updateTick() {
        // ... 已有逻辑 ...
        if (m_uiManager) {
            m_uiManager->update(deltaTime);
        }
    }
};

// 全局访问便利函数
inline UIManager* GetUIManager() {
    return Application::Self().context().getUIManager();
}
```

---

## 9. UI 布局文件格式

```jsonc
// assets/ui/main_menu.ui
{
    "version": 1,
    "root": {
        "type": "Panel",
        "id": "root",
        "anchor_min": [0, 0],
        "anchor_max": [1, 1],
        "background_color": [0.1, 0.1, 0.15, 1.0],
        "children": [
            {
                "type": "Image",
                "id": "logo",
                "position": [760, 60],
                "size": [400, 150],
                "texture_id": 12345,
                "uv_rect": [0, 0, 1, 1],
                "preserve_aspect": true
            },
            {
                "type": "Label",
                "id": "title",
                "position": [860, 250],
                "size": [200, 60],
                "text": "My Game",
                "font_size": 48,
                "color": [1, 1, 1, 1],
                "alignment": "MiddleCenter"
            },
            {
                "type": "StackLayout",
                "id": "menu_buttons",
                "position": [810, 400],
                "size": [300, 300],
                "spacing": 16,
                "direction": "Vertical",
                "child_alignment": "Center",
                "children": [
                    {
                        "type": "Button",
                        "id": "play_btn",
                        "size": [200, 64],
                        "preset_id": 1001,
                        "label": "Play"
                    },
                    {
                        "type": "Button",
                        "id": "settings_btn",
                        "size": [200, 64],
                        "preset_id": 1001,
                        "label": "Settings"
                    },
                    {
                        "type": "Button",
                        "id": "quit_btn",
                        "size": [200, 64],
                        "preset_id": 1002,
                        "label": "Quit"
                    }
                ]
            }
        ]
    }
}
```

---

## 10. C# 脚本 API

```csharp
// do@Redlive — UIService.cs
namespace Dodoe.UI
{
    public static class UIService
    {
        // 加载/卸载布局文件
        public static void LoadLayout(string filePath);
        public static void UnloadLayout(string layoutId);
        public static void ClearAll();

        // 按 id 查找元素
        public static T Find<T>(string elementId) where T : UIElement;
        public static UIElement Find(string elementId);

        // 动态创建元素
        public static Panel CreatePanel(string id, Vector2 position, Vector2 size);
        public static Label CreateLabel(string id, string text, Vector2 position, int fontSize = 16);
        public static Button CreateButton(string id, string presetKey, Vector2 position, Vector2 size);
        public static Image CreateImage(string id, string texturePath, Vector2 position, Vector2 size);
    }

    public class Button
    {
        public string Label { get; set; }
        public bool Interactable { get; set; }
        public bool Visible { get; set; }
        public event Action OnClick;
    }

    public class Label
    {
        public string Text { get; set; }
        public Color Color { get; set; }
        public int FontSize { get; set; }
    }

    public class Image
    {
        public string TexturePath { get; set; }
        public Color Tint { get; set; }
        public bool PreserveAspect { get; set; }
    }

    public class Panel
    {
        public Color? BackgroundColor { get; set; }
        public bool ClipChildren { get; set; }
    }
}
```

C# 脚本使用示例：

```csharp
public class MainMenuScript : ScriptComponent
{
    protected override void OnStart()
    {
        UIService.LoadLayout("assets/ui/main_menu.ui");

        var playBtn = UIService.Find<Button>("play_btn");
        playBtn.OnClick += () => SceneManager.LoadScene("Level1");

        var quitBtn = UIService.Find<Button>("quit_btn");
        quitBtn.OnClick += () => Application.Quit();

        var title = UIService.Find<Label>("title");
        title.Text = $"Welcome, {SaveData.PlayerName}";
    }
}
```

---

## 11. ScriptGlue 绑定清单

只需 **5 个核心绑定**（而非 20+）：

```cpp
// do@Redlive — script_glue.cpp 新增
NATIVE_METHOD(native_ui_load_layout,        Dodoe.UI.Bindings.LoadLayout)
NATIVE_METHOD(native_ui_unload_layout,      Dodoe.UI.Bindings.UnloadLayout)
NATIVE_METHOD(native_ui_clear_all,          Dodoe.UI.Bindings.ClearAll)
NATIVE_METHOD(native_ui_find_element,       Dodoe.UI.Bindings.FindElement)
NATIVE_METHOD(native_ui_element_set_property, Dodoe.UI.Bindings.SetProperty)

// 其他所有操作（setText, setColor 等）通过 SetProperty 通配完成：
// native_ui_element_set_property(id, "Text", "Hello")
// native_ui_element_set_property(id, "Color", [1,1,1,1])
```

---

## 12. 实施计划

| 阶段 | 内容 | 依赖 |
|------|------|------|
| **Phase 1** | 删除旧 UI 代码 | 无 |
| **Phase 1** | `UIInstance` 数据结构 + `RenderCommandType::SubmitUIBatch` + `RenderCommandQueue::SubmitUI` | 无 |
| **Phase 1** | `RenderScene::UISceneInfo` + `UIPass`（instanced draw） | 上一步 |
| **Phase 2** | `UIElement` + `Thickness` + 布局数学（anchor/pivot） | 无 |
| **Phase 2** | `UIInteractive` + `UIInputRouter`（hit-test + 事件分发） | UIElement |
| **Phase 2** | `UIRenderBatch::collect()` + `UIElement::onCollectRenderData` | UIElement + Render |
| **Phase 3** | `UIWidget` + `UIImage` + `UILabel` + `UIPanel` | Phase 2 |
| **Phase 3** | `UIStackLayout` + `UIGridLayout` | UIElement |
| **Phase 3** | `UIButton` + ButtonState 状态机 | UIInteractive |
| **Phase 4** | `UILayoutLoader`（JSON 解析 + 构建 UI 树） | Phase 3 |
| **Phase 4** | `UIManager` 集成到 `SystemContext` | Phase 3 |
| **Phase 4** | `UIPresetManager`（按钮/图片预设） | Phase 3 |
| **Phase 5** | `ScriptGlue` UI 绑定 | Phase 4 |
| **Phase 5** | C# `UIService` / `Button` / `Label` / `Image` / `Panel` | 上一步 |

---

## 13. 文件清单（预估 ~20 个新文件）

```
engine/src/runtime/function/ui/
  ├── ui_element.h / .cpp            # UIElement 基类
  ├── ui_interactive.h / .cpp        # UIInteractive 交互基类
  ├── ui_widget.h / .cpp             # UIWidget 可绘制基类
  ├── ui_image.h / .cpp              # UIImage
  ├── ui_label.h / .cpp              # UILabel
  ├── ui_button.h / .cpp             # UIButton
  ├── ui_panel.h / .cpp              # UIPanel
  ├── ui_stack_layout.h / .cpp       # UIStackLayout
  ├── ui_grid_layout.h / .cpp        # UIGridLayout
  ├── ui_layout_loader.h / .cpp      # UILayoutLoader (JSON → 树)
  ├── ui_manager.h / .cpp            # UIManager
  ├── ui_input_router.h / .cpp       # UIInputRouter
  ├── ui_render_batch.h / .cpp       # UIRenderBatch
  ├── ui_preset_manager.h / .cpp     # UIPresetManager (保留)
  ├── ui_types.h                     # Thickness, UIInstance, NineSliceMargins 等类型
  └── ui_enums.h                     # ButtonState, FillMethod, TextAnchor 等枚举

engine/src/runtime/function/render/
  ├── render_command.h               # 新增 SubmitUIBatch
  ├── render_command_queue.h/.cpp    # 新增 SubmitUI
  ├── render_scene/ui_scene_info.h/.cpp  # UISceneInfo
  └── render_pipeline/ui_pass.h/.cpp     # UIPass
```
