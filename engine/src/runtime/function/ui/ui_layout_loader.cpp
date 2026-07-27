// do@Redlive

#include "ui_layout_loader.h"

#include "ui_panel.h"
#include "ui_label.h"
#include "ui_image.h"
#include "ui_button.h"
#include "ui_preset_manager.h"
#include "ui_stack_layout.h"
#include "ui_grid_layout.h"

#include "runtime/core/utils/util.h"

#include <fstream>
#include <sstream>

namespace dodoe {

    Scope<UIElement> UILayoutLoader::LoadFromJson(const Json& json) {
        return LoadFromJson(json, nullptr);
    }

    Scope<UIElement> UILayoutLoader::LoadFromJson(const Json& json, UIPresetManager* preset_manager) {
        if (!json.contains("root")) return nullptr;

        return ParseElement(json["root"], preset_manager);
    }

    Scope<UIElement> UILayoutLoader::LoadFromFile(const String& filePath) {
        return LoadFromFile(filePath, nullptr);
    }

    Scope<UIElement> UILayoutLoader::LoadFromFile(const String& filePath, UIPresetManager* preset_manager) {
        std::ifstream file(filePath.c_str());
        if (!file.is_open()) return nullptr;

        std::stringstream buffer;
        buffer << file.rdbuf();

        Json json = Json::parse(buffer.str(), nullptr, false);
        if (json.is_discarded()) return nullptr;

        return LoadFromJson(json, preset_manager);
    }

    Scope<UIElement> UILayoutLoader::ParseElement(const Json& node, UIPresetManager* preset_manager) {
        if (!node.contains("type")) return nullptr;

        String type = node["type"].get<String>();

        auto element = CreateElementByType(type);
        if (!element) return nullptr;

        if (node.contains("id")) {
            String id_str = node["id"].get<String>();
            element->setId(static_cast<identifier>(std::hash<String>{}(id_str)));
        }

        if (node.contains("anchor_min") && node["anchor_min"].is_array() && node["anchor_min"].size() == 2) {
            element->setAnchor(
                {node["anchor_min"][0].get<Float>(), node["anchor_min"][1].get<Float>()},
                element->getAnchorMax());
        }
        if (node.contains("anchor_max") && node["anchor_max"].is_array() && node["anchor_max"].size() == 2) {
            element->setAnchor(
                element->getAnchorMin(),
                {node["anchor_max"][0].get<Float>(), node["anchor_max"][1].get<Float>()});
        }
        if (node.contains("position") && node["position"].is_array() && node["position"].size() == 2) {
            element->setPosition({node["position"][0].get<Float>(), node["position"][1].get<Float>()});
        }
        if (node.contains("size") && node["size"].is_array() && node["size"].size() == 2) {
            element->setSize({node["size"][0].get<Float>(), node["size"][1].get<Float>()});
        }
        if (node.contains("pivot") && node["pivot"].is_array() && node["pivot"].size() == 2) {
            element->setPivot({node["pivot"][0].get<Float>(), node["pivot"][1].get<Float>()});
        }

        if (auto* panel = dynamic_cast<UIPanel*>(element.get())) {
            if (node.contains("background_color") && node["background_color"].is_array() && node["background_color"].size() >= 4) {
                panel->setBackgroundColor(Color(
                    node["background_color"][0].get<Float>(),
                    node["background_color"][1].get<Float>(),
                    node["background_color"][2].get<Float>(),
                    node["background_color"][3].get<Float>()));
            }
            if (node.contains("clip_children")) {
                panel->setClipChildren(node["clip_children"].get<Bool>());
            }
        }

        if (auto* label = dynamic_cast<UILabel*>(element.get())) {
            if (node.contains("text")) {
                label->setText(node["text"].get<String>());
            }
            if (node.contains("font_size")) {
                label->setFontSize(node["font_size"].get<Int>());
            }
            if (node.contains("color") && node["color"].is_array() && node["color"].size() >= 4) {
                label->setColor(Color(
                    node["color"][0].get<Float>(),
                    node["color"][1].get<Float>(),
                    node["color"][2].get<Float>(),
                    node["color"][3].get<Float>()));
            }
            if (node.contains("alignment")) {
                String align = node["alignment"].get<String>();
                if (align == "UpperLeft") label->setTextAlignment(TextAnchor::UpperLeft);
                else if (align == "UpperCenter") label->setTextAlignment(TextAnchor::UpperCenter);
                else if (align == "UpperRight") label->setTextAlignment(TextAnchor::UpperRight);
                else if (align == "MiddleLeft") label->setTextAlignment(TextAnchor::MiddleLeft);
                else if (align == "MiddleCenter") label->setTextAlignment(TextAnchor::MiddleCenter);
                else if (align == "MiddleRight") label->setTextAlignment(TextAnchor::MiddleRight);
                else if (align == "LowerLeft") label->setTextAlignment(TextAnchor::LowerLeft);
                else if (align == "LowerCenter") label->setTextAlignment(TextAnchor::LowerCenter);
                else if (align == "LowerRight") label->setTextAlignment(TextAnchor::LowerRight);
            }
        }

        if (auto* image = dynamic_cast<UIImage*>(element.get())) {
            if (node.contains("uv_rect") && node["uv_rect"].is_array() && node["uv_rect"].size() == 4) {
                image->setUVRect(Rect(
                    node["uv_rect"][0].get<Float>(), node["uv_rect"][1].get<Float>(),
                    node["uv_rect"][2].get<Float>(), node["uv_rect"][3].get<Float>()));
            }
            if (node.contains("preserve_aspect")) {
                image->setPreserveAspect(node["preserve_aspect"].get<Bool>());
            }
        }

        if (auto* button = dynamic_cast<UIButton*>(element.get())) {
            if (node.contains("preset_id") && preset_manager) {
                identifier preset_id = node["preset_id"].get<identifier>();
                if (const auto* preset = preset_manager->findButtonPreset(preset_id)) {
                    button->applyPreset(*preset);
                }
            }
            if (node.contains("label")) {
                button->setLabel(node["label"].get<String>());
            }
        }

        if (auto* stack = dynamic_cast<UIStackLayout*>(element.get())) {
            if (node.contains("spacing")) {
                stack->setSpacing(node["spacing"].get<Float>());
            }
            if (node.contains("direction")) {
                String dir = node["direction"].get<String>();
                if (dir == "Horizontal") stack->setDirection(LayoutDirection::Horizontal);
                else stack->setDirection(LayoutDirection::Vertical);
            }
            if (node.contains("child_alignment")) {
                String align = node["child_alignment"].get<String>();
                if (align == "Start") stack->setChildAlignment(Alignment::Start);
                else if (align == "Center") stack->setChildAlignment(Alignment::Center);
                else if (align == "End") stack->setChildAlignment(Alignment::End);
                else if (align == "Stretch") stack->setChildAlignment(Alignment::Stretch);
            }
        }

        if (auto* grid = dynamic_cast<UIGridLayout*>(element.get())) {
            if (node.contains("columns")) {
                grid->setColumns(node["columns"].get<Int>());
            }
            if (node.contains("cell_size") && node["cell_size"].is_array() && node["cell_size"].size() == 2) {
                grid->setCellSize({node["cell_size"][0].get<Float>(), node["cell_size"][1].get<Float>()});
            }
        }

        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child_node : node["children"]) {
                auto child = ParseElement(child_node, preset_manager);
                if (child) {
                    element->addChild(std::move(child));
                }
            }
        }

        return element;
    }

    Scope<UIElement> UILayoutLoader::CreateElementByType(const String& type) {
        if (type == "Panel") return create_scope<UIPanel>();
        if (type == "Label") return create_scope<UILabel>();
        if (type == "Image") return create_scope<UIImage>();
        if (type == "Button") return create_scope<UIButton>();
        if (type == "StackLayout") return create_scope<UIStackLayout>();
        if (type == "GridLayout") return create_scope<UIGridLayout>();
        return nullptr;
    }

} // namespace dodoe
