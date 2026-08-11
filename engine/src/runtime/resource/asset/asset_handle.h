// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/object/object_id.h"

namespace dodoe {

    class Asset;
    class AssetManager;
    class ResourceManager;

    template<typename T>
    class AssetHandle {
        static_assert(std::is_base_of_v<Asset, T>, "T must derive from Asset");

        ObjectID m_id{};

    public:
        AssetHandle() = default;
        explicit AssetHandle(const ObjectID& id) : m_id(id) {}
        AssetHandle(const AssetHandle&) = default;
        AssetHandle& operator=(const AssetHandle&) = default;
        AssetHandle(AssetHandle&&) noexcept = default;
        AssetHandle& operator=(AssetHandle&&) noexcept = default;

        [[nodiscard]] T* get() const;
        T* operator->() const;
        T& operator*() const;

        [[nodiscard]] const ObjectID& getObjectID() const { return m_id; }
        [[nodiscard]] Bool isValid() const { return m_id.isValid(); }
        [[nodiscard]] explicit operator Bool() const { return isValid(); }

        [[nodiscard]] Bool isLoaded() const;

        Bool operator==(const AssetHandle& other) const {
            return m_id == other.m_id;
        }
        Bool operator!=(const AssetHandle& other) const {
            return !(m_id == other.m_id);
        }

        template<typename U, typename = std::enable_if_t<std::is_base_of_v<T, U>>>
        operator AssetHandle<U>() const {
            return AssetHandle<U>(m_id);
        }

        void setObjectID(const ObjectID& id) { m_id = id; }
    };

} // dodoe

namespace std {
    template<typename T>
    struct hash<dodoe::AssetHandle<T>> {
        dodoe::Size_t operator()(const dodoe::AssetHandle<T>& handle) const noexcept {
            return static_cast<dodoe::Size_t>(
                static_cast<std::uint64_t>(handle.getObjectID().asset_id)
                ^ (static_cast<std::uint64_t>(handle.getObjectID().local_id) << 32));
        }
    };
} // namespace std
