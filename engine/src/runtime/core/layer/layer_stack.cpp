//
// Created by GreenMuffin on 2025/11/15.
//

#include "runtime/core/layer/layer_stack.h"
#include "runtime/core/layer/layer.h"

namespace dodoe {

    LayerStack::~LayerStack() {
        clear_layers();
    }

    void LayerStack::attach() {
        for (auto& layer : layers_) {
            layer->attach();
        }
    }

    void LayerStack::detach() {
        for (auto& layer : layers_) {
            layer->detach();
        }
    }

    void LayerStack::clear_layers() {
        for (auto* layer : layers_) {
            delete layer;
        }
        layers_.clear();
        layer_insert_index_ = 0;
    }

    void LayerStack::push_layer(Layer *layer) {
        layers_.emplace(layers_.begin() + layer_insert_index_, layer);
        layer_insert_index_++;
    }

    void LayerStack::push_over_layer(Layer *layer) {
        layers_.emplace_back(layer);
    }

    void LayerStack::pop_layer(Layer *layer) {
        if (const auto it = std::find(layers_.begin(), layers_.begin() + layer_insert_index_, layer);
            it != layers_.begin() + layer_insert_index_) {
                delete *it;
                layers_.erase(it);
                layer_insert_index_--;
        }
    }

    void LayerStack::pop_over_layer(Layer *layer) {
        if (const auto it = std::find(layers_.begin() + layer_insert_index_, layers_.end(), layer);
            it != layers_.end()) {
                delete *it;
                layers_.erase(it);
        }
    }

} // dodoe
