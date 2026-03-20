//
// Created by GreenMuffin on 2026/2/3.
//

#ifndef DODOE_OBJECT_POOL_H
#define DODOE_OBJECT_POOL_H

#include "dopch.h"

namespace dodoe {

    template <typename T>
    class ObjectPool {
    public:
        explicit ObjectPool(size_t initial_capacity = 2) {
            expand_(initial_capacity);
        }

        ~ObjectPool() {
            for (T* obj : all_objects_) {
                delete obj;
            }
        }

        ObjectPool(const ObjectPool&) = delete;
        ObjectPool& operator=(const ObjectPool&) = delete;

        T* acquire() {
            if (available_.empty()) {
                expand_(all_objects_.empty() ? 1 : all_objects_.size());
            }
            T* obj = available_.back();
            available_.pop_back();
            return obj;
        }

        void release(T* obj) {
            if (!owns_(obj)) {
                return;
            }
            available_.push_back(obj);
        }

    private:
        std::vector<T*> all_objects_;
        std::vector<T*> available_;

        void expand_(size_t count) {
            for (size_t i = 0; i < count; ++i) {
                T* obj = new T();
                all_objects_.push_back(obj);
                available_.push_back(obj);
            }
        }

        bool owns_(T* obj) const {
            return std::find(all_objects_.begin(), all_objects_.end(), obj) != all_objects_.end();
        }
    };


} // dodoe

#endif // !DODOE_OBJECT_POOL_H
