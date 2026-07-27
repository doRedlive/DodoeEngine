// do@Redlive

#include "ui_manager.h"

#include "ui_layout_loader.h"
#include "ui_render_batch.h"
#include "ui_panel.h"
#include "ui_label.h"
#include "ui_image.h"
#include "ui_button.h"
#include "ui_stack_layout.h"
#include "ui_grid_layout.h"
#include "runtime/function/window/window.h"
#include "runtime/function/window/window_manager.h"

namespace dodoe {

    Bool UIManager::initialize(const UIManagerCreateInfo& info) {
        m_window_manager = info.window_manager;
        m_input_router = create_scope<UIInputRouter>();
        m_render_batch = create_scope<UIRenderBatch>();
        m_preset_manager = create_scope<UIPresetManager>();
        return true;
    }

    void UIManager::shutdown() {
        m_root.reset();
        m_input_router.reset();
        m_render_batch.reset();
        m_preset_manager.reset();
    }

    void UIManager::update(Float deltaTime) {
        (void)deltaTime;
        if (!m_root) return;

        m_root->ensureLayout();

        m_render_batch->clear();

        m_root->onCollectRenderData(*m_render_batch);

        m_render_batch->submit();
    }

    void UIManager::setRoot(Scope<UIElement> root) {
        m_root = std::move(root);
        if (m_input_router) {
            m_input_router->setRoot(m_root.get());
        }
        if (m_root) {
            m_root->invalidateLayout();
        }
    }

    void UIManager::onMouseMove(Vector2f pos) {
        if (m_input_router) m_input_router->processMouseMove(pos);
    }

    void UIManager::onMouseDown(Vector2f pos) {
        if (m_input_router) m_input_router->processMouseDown(pos);
    }

    void UIManager::onMouseUp(Vector2f pos) {
        if (m_input_router) m_input_router->processMouseUp(pos);
    }

    void UIManager::onScroll(Vector2f pos, Float delta) {
        if (m_input_router) m_input_router->processScroll(pos, delta);
    }

    Bool UIManager::loadLayout(const String& filePath) {
        auto root = UILayoutLoader::LoadFromFile(filePath, m_preset_manager.get());
        if (!root) {
            LOG_ERROR("UIManager: Failed to load layout from {}", filePath);
            return false;
        }
        setRoot(std::move(root));
        return true;
    }

    void UIManager::unloadLayout() {
        m_root.reset();
        if (m_input_router) {
            m_input_router->setRoot(nullptr);
        }
    }

    void UIManager::clearAll() {
        unloadLayout();
    }

    UIElement* UIManager::findElementById(const String& id) const {
        if (!m_root) return nullptr;

        const identifier hashed_id = static_cast<identifier>(std::hash<String>{}(id));

        DynamicArray<UIElement*> queue;
        queue.push_back(m_root.get());

        while (!queue.empty()) {
            auto* current = queue.front();
            queue.erase(queue.begin());

            if (current->getId() == hashed_id) {
                return current;
            }

            for (const auto& child : current->getChildren()) {
                queue.push_back(child.get());
            }
        }

        return nullptr;
    }

    UIElement* UIManager::createElement(const String& type, const String& id, const String& parentId) {
        if (!m_root) return nullptr;

        Scope<UIElement> element;
        if (type == "Panel")       element = create_scope<UIPanel>();
        else if (type == "Label")  element = create_scope<UILabel>();
        else if (type == "Image")  element = create_scope<UIImage>();
        else if (type == "Button") element = create_scope<UIButton>();
        else if (type == "StackLayout") element = create_scope<UIStackLayout>();
        else if (type == "GridLayout")  element = create_scope<UIGridLayout>();
        else return nullptr;

        element->setId(static_cast<identifier>(std::hash<String>{}(id)));

        UIElement* parent = parentId.empty() ? m_root.get() : findElementById(parentId);
        if (!parent) return nullptr;

        auto* raw_ptr = element.get();
        parent->addChild(std::move(element));
        parent->invalidateLayout();
        return raw_ptr;
    }

    Bool UIManager::removeElement(const String& id) {
        auto* elem = findElementById(id);
        if (!elem) return false;

        auto* parent = elem->getParent();
        if (!parent) {
            m_root.reset();
            if (m_input_router) m_input_router->setRoot(nullptr);
            return true;
        }

        parent->removeChild(elem);
        parent->invalidateLayout();
        return true;
    }

} // namespace dodoe
