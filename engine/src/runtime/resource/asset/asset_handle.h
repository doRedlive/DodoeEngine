// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/utils/uuid.h"

namespace dodoe {

    class Asset;
    class AssetManager;
    class ResourceManager;

    template<typename T>
    class AssetHandle {
        static_assert(std::is_base_of_v<Asset, T>, "T must derive from Asset");

        UUID m_uuid{};

    public:
        AssetHandle() = default;
        explicit AssetHandle(const UUID& uuid) : m_uuid(uuid) {}
        AssetHandle(const AssetHandle&) = default;
        AssetHandle& operator=(const AssetHandle&) = default;
        AssetHandle(AssetHandle&&) noexcept = default;
        AssetHandle& operator=(AssetHandle&&) noexcept = default;

        [[nodiscard]] T* get() const;
        T* operator->() const;
        T& operator*() const;

        [[nodiscard]] const UUID& getUUID() const { return m_uuid; }
        [[nodiscard]] Bool isValid() const { return m_uuid.isValid(); }
        [[nodiscard]] explicit operator Bool() const { return isValid(); }

        [[nodiscard]] Bool isLoaded() const;

        Bool operator==(const AssetHandle& other) const {
            return m_uuid == other.m_uuid;
        }
        Bool operator!=(const AssetHandle& other) const {
            return !(m_uuid == other.m_uuid);
        }

        template<typename U, typename = std::enable_if_t<std::is_base_of_v<T, U>>>
        operator AssetHandle<U>() const {
            return AssetHandle<U>(m_uuid);
        }

        void setUUID(const UUID& uuid) { m_uuid = uuid; }
    };

} // dodoe

namespace std {
    template<typename T>
    struct hash<dodoe::AssetHandle<T>> {
        dodoe::Size_t operator()(const dodoe::AssetHandle<T>& handle) const noexcept {
            return static_cast<dodoe::Size_t>(static_cast<UInt64>(handle.getUUID()));
        }
    };
} // namespace std
