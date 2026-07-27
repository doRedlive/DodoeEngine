// do@Redlive

#ifdef DODOE_PYTHON_ENABLED

#include "py_bindings_ui.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/ui/ui_manager.h"
#include "runtime/function/ui/ui_button.h"
#include "runtime/function/ui/ui_label.h"
#include "runtime/function/ui/ui_image.h"
#include "runtime/function/ui/ui_panel.h"
#include "runtime/function/ui/ui_stack_layout.h"
#include "runtime/function/ui/ui_grid_layout.h"
#include "runtime/function/ui/ui_widget.h"
#include "runtime/function/ui/ui_interactive.h"
#include "runtime/function/ui/ui_types.h"

namespace dodoe::py_bindings {
namespace {

    UIManager* GetUI() {
        return GetUIManager();
    }

    UIElement* FindElement(identifier id) {
        auto* ui = GetUI();
        if (!ui || !ui->getRoot()) return nullptr;

        DynamicArray<UIElement*> queue;
        queue.push_back(ui->getRoot());
        while (!queue.empty()) {
            auto* cur = queue.front();
            queue.erase(queue.begin());
            if (cur->getId() == id) return cur;
            for (const auto& child : cur->getChildren()) {
                queue.push_back(child.get());
            }
        }
        return nullptr;
    }

} // anonymous namespace

void RegisterUI(py::module_& m) {
    py::class_<Thickness>(m, "Thickness")
        .def(py::init<>())
        .def(py::init<Float>())
        .def(py::init<Float, Float, Float, Float>())
        .def_readwrite("left", &Thickness::left)
        .def_readwrite("top", &Thickness::top)
        .def_readwrite("right", &Thickness::right)
        .def_readwrite("bottom", &Thickness::bottom);

    py::enum_<TextAnchor>(m, "TextAnchor")
        .value("UpperLeft", TextAnchor::UpperLeft)
        .value("UpperCenter", TextAnchor::UpperCenter)
        .value("UpperRight", TextAnchor::UpperRight)
        .value("MiddleLeft", TextAnchor::MiddleLeft)
        .value("MiddleCenter", TextAnchor::MiddleCenter)
        .value("MiddleRight", TextAnchor::MiddleRight)
        .value("LowerLeft", TextAnchor::LowerLeft)
        .value("LowerCenter", TextAnchor::LowerCenter)
        .value("LowerRight", TextAnchor::LowerRight)
        .export_values();

    py::enum_<FillMethod>(m, "FillMethod")
        .value("None", FillMethod::None)
        .value("Horizontal", FillMethod::Horizontal)
        .value("Vertical", FillMethod::Vertical)
        .value("Radial90", FillMethod::Radial90)
        .value("Radial180", FillMethod::Radial180)
        .value("Radial360", FillMethod::Radial360)
        .export_values();

    py::enum_<LayoutDirection>(m, "LayoutDirection")
        .value("Horizontal", LayoutDirection::Horizontal)
        .value("Vertical", LayoutDirection::Vertical)
        .export_values();

    py::class_<UIElement>(m, "UIElement")
        .def_property("id",
            [](UIElement& e) { return e.getId(); },
            [](UIElement& e, identifier id) { e.setId(id); })
        .def_property("visible",
            [](UIElement& e) { return e.isVisible(); },
            [](UIElement& e, Bool v) { e.setVisible(v); })
        .def_property("depth",
            [](UIElement& e) { return e.getDepth(); },
            [](UIElement& e, Float d) { e.setDepth(d); })
        .def_property("position",
            [](UIElement& e) { return e.getPosition(); },
            [](UIElement& e, const Vector2f& v) { e.setPosition(v); })
        .def_property("size",
            [](UIElement& e) { return e.getSize(); },
            [](UIElement& e, const Vector2f& v) { e.setSize(v); })
        .def_property("anchor_min",
            [](UIElement& e) { return e.getAnchorMin(); },
            [](UIElement& e, const Vector2f& v) { e.setAnchor(v, e.getAnchorMax()); })
        .def_property("anchor_max",
            [](UIElement& e) { return e.getAnchorMax(); },
            [](UIElement& e, const Vector2f& v) { e.setAnchor(e.getAnchorMin(), v); })
        .def_property("pivot",
            [](UIElement& e) { return e.getPivot(); },
            [](UIElement& e, const Vector2f& v) { e.setPivot(v); })
        .def_property("padding",
            [](UIElement& e) -> const Thickness& { return e.getPadding(); },
            [](UIElement& e, const Thickness& v) { e.setPadding(v); })
        .def_property("margin",
            [](UIElement& e) -> const Thickness& { return e.getMargin(); },
            [](UIElement& e, const Thickness& v) { e.setMargin(v); })
        .def("get_screen_position", &UIElement::getScreenPosition)
        .def("get_layout_size", &UIElement::getLayoutSize)
        .def("get_screen_rect", &UIElement::getScreenRect);

    py::class_<UIInteractive, UIElement>(m, "UIInteractive")
        .def_property("interactable",
            [](UIInteractive& e) { return e.isInteractable(); },
            [](UIInteractive& e, Bool v) { e.setInteractable(v); })
        .def_property("raycast_target",
            [](UIInteractive& e) { return e.isRaycastTarget(); },
            [](UIInteractive& e, Bool v) { e.setRaycastTarget(v); })
        .def_property_readonly("is_hovered", &UIInteractive::isHovered)
        .def_property_readonly("is_pressed", &UIInteractive::isPressed)
        .def_readwrite("on_click", &UIInteractive::on_click)
        .def_readwrite("on_hover_enter", &UIInteractive::on_hover_enter)
        .def_readwrite("on_hover_leave", &UIInteractive::on_hover_leave);

    py::class_<UIWidget, UIInteractive>(m, "UIWidget")
        .def_property("color",
            [](UIWidget& e) { return e.getColor(); },
            [](UIWidget& e, const Color& v) { e.setColor(v); })
        .def_property("alpha",
            [](UIWidget& e) { return e.getAlpha(); },
            [](UIWidget& e, Float v) { e.setAlpha(v); });

    py::class_<UILabel, UIWidget>(m, "UILabel")
        .def("set_text", &UILabel::setText)
        .def_property("text",
            [](UILabel& l) { return l.getText(); },
            [](UILabel& l, const String& v) { l.setText(v); })
        .def("set_font_size", &UILabel::setFontSize)
        .def_property("font_size",
            [](UILabel& l) { return l.getFontSize(); },
            [](UILabel& l, Int v) { l.setFontSize(v); })
        .def("set_text_alignment", &UILabel::setTextAlignment)
        .def("set_line_spacing", &UILabel::setLineSpacing);

    py::class_<UIImage, UIWidget>(m, "UIImage")
        .def("set_preserve_aspect", &UIImage::setPreserveAspect)
        .def("set_flipped", &UIImage::setFlipped)
        .def_property("flip_h",
            [](UIImage& i) { return i.isFlippedH(); },
            [](UIImage& i, Bool v) { i.setFlippedH(v); })
        .def_property("flip_v",
            [](UIImage& i) { return i.isFlippedV(); },
            [](UIImage& i, Bool v) { i.setFlippedV(v); })
        .def_property("preserve_aspect",
            [](UIImage& i) { return i.isPreserveAspect(); },
            [](UIImage& i, Bool v) { i.setPreserveAspect(v); })
        .def("set_fill_method", &UIImage::setFillMethod)
        .def("set_uv_rect", &UIImage::setUVRect);

    py::class_<UIButton, UIInteractive>(m, "UIButton")
        .def("set_label", &UIButton::setLabel)
        .def_property("label",
            [](UIButton& b) { return b.getLabel(); },
            [](UIButton& b, const String& v) { b.setLabel(v); })
        .def("set_preset", &UIButton::setPreset);

    py::class_<UIPanel, UIElement>(m, "UIPanel")
        .def("set_background_color", &UIPanel::setBackgroundColor)
        .def_property("background_color",
            [](UIPanel& p) { return p.getBackgroundColor(); },
            [](UIPanel& p, const Color& v) { p.setBackgroundColor(v); })
        .def("set_background_image", &UIPanel::setBackgroundImage)
        .def("set_nine_slice", &UIPanel::setNineSlice)
        .def("set_clip_children", &UIPanel::setClipChildren)
        .def_property("clip_children",
            [](UIPanel& p) { return p.isClipChildrenEnabled(); },
            [](UIPanel& p, Bool v) { p.setClipChildren(v); });

    py::class_<UIStackLayout, UIElement>(m, "UIStackLayout")
        .def_property("direction",
            nullptr,
            [](UIStackLayout& l, LayoutDirection v) { l.setDirection(v); })
        .def("set_spacing", &UIStackLayout::setSpacing)
        .def("set_child_alignment", &UIStackLayout::setChildAlignment);

    py::class_<UIGridLayout, UIElement>(m, "UIGridLayout")
        .def("set_columns", &UIGridLayout::setColumns)
        .def("set_spacing", &UIGridLayout::setSpacing)
        .def("set_cell_size", &UIGridLayout::setCellSize);

    m.def("ui_load_layout", [](const String& filePath) -> Bool {
            auto* ui = GetUI();
            return ui ? ui->loadLayout(filePath) : false;
        },
        py::arg("file_path"),
        "Load a .ui layout file");

    m.def("ui_clear_all", []() {
            auto* ui = GetUI();
            if (ui) ui->clearAll();
        },
        "Clear all UI");

    m.def("ui_find_button", [](const String& id) -> UIButton* {
            auto* ui = GetUI();
            if (!ui) return nullptr;
            auto* elem = ui->findElementById(id);
            return dynamic_cast<UIButton*>(elem);
        },
        py::arg("id"),
        py::return_value_policy::reference,
        "Find a button by id");

    m.def("ui_find_label", [](const String& id) -> UILabel* {
            auto* ui = GetUI();
            if (!ui) return nullptr;
            auto* elem = ui->findElementById(id);
            return dynamic_cast<UILabel*>(elem);
        },
        py::arg("id"),
        py::return_value_policy::reference,
        "Find a label by id");

    m.def("ui_find_image", [](const String& id) -> UIImage* {
            auto* ui = GetUI();
            if (!ui) return nullptr;
            auto* elem = ui->findElementById(id);
            return dynamic_cast<UIImage*>(elem);
        },
        py::arg("id"),
        py::return_value_policy::reference,
        "Find an image by id");

    m.def("ui_find_panel", [](const String& id) -> UIPanel* {
            auto* ui = GetUI();
            if (!ui) return nullptr;
            auto* elem = ui->findElementById(id);
            return dynamic_cast<UIPanel*>(elem);
        },
        py::arg("id"),
        py::return_value_policy::reference,
        "Find a panel by id");

    m.def("ui_find_element", [](const String& id) -> UIElement* {
            auto* ui = GetUI();
            return ui ? ui->findElementById(id) : nullptr;
        },
        py::arg("id"),
        py::return_value_policy::reference,
        "Find a UI element by id");

    m.def("ui_create_element", [](const String& type, const String& id, const String& parent_id) -> UIElement* {
            auto* ui = GetUI();
            return ui ? ui->createElement(type, id, parent_id) : nullptr;
        },
        py::arg("type"),
        py::arg("id"),
        py::arg("parent_id") = "",
        py::return_value_policy::reference,
        "Create a UI element and add to parent");
}

} // namespace dodoe::py_bindings

#endif // DODOE_PYTHON_ENABLED
