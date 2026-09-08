// do@Redlive

#include "runtime/core/layer/layer_stack.h"
#include "runtime/core/layer/layer.h"

namespace dodoe {

    LayerStack::~LayerStack() {
        clearLayers();
    }

    void LayerStack::attach() {
        DO_PROFILE_SCOPE_CATEGORY("LayerStack::attach", "startup");
        for (auto& layer : m_layers) {
            layer->attach();
        }
    }

    void LayerStack::detach() {
        DO_PROFILE_SCOPE_CATEGORY("LayerStack::detach", "shutdown");
        for (auto& layer : m_layers) {
            layer->detach();
        }
    }

    void LayerStack::clearLayers() {
        for (auto* layer : m_layers) {
            if (layer) {
                layer->~Layer();
                Memory::DeallocatePersistent(layer, sizeof(Layer), AllocTag::Object);
            }
        }
        m_layers.clear();
        m_layer_insert_index = 0;
    }

    void LayerStack::pushLayer(Layer *layer) {
        m_layers.emplace(m_layers.begin() + m_layer_insert_index, layer);
        m_layer_insert_index++;
    }

    void LayerStack::pushOverLayer(Layer *layer) {
        m_layers.emplace_back(layer);
    }

    void LayerStack::popLayer(Layer *layer) {
        if (const auto it = std::find(m_layers.begin(), m_layers.begin() + m_layer_insert_index, layer);
            it != m_layers.begin() + m_layer_insert_index) {
                if (*it) {
                    (*it)->~Layer();
                    Memory::DeallocatePersistent(*it, sizeof(Layer), AllocTag::Object);
                }
                m_layers.erase(it);
                m_layer_insert_index--;
        }
    }

    void LayerStack::popOverLayer(Layer *layer) {
        if (const auto it = std::find(m_layers.begin() + m_layer_insert_index, m_layers.end(), layer);
            it != m_layers.end()) {
                if (*it) {
                    (*it)->~Layer();
                    Memory::DeallocatePersistent(*it, sizeof(Layer), AllocTag::Object);
                }
                m_layers.erase(it);
        }
    }

    Layer* LayerStack::getLayer(const String& name) {
        for (auto layer : m_layers) {
            if (layer->getName() == name) {
                return layer;
            }
        }
        return nullptr;
    }

} // dodoe
