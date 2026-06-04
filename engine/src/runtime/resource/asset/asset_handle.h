// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/file/file_id.h"

namespace dodoe {

    class Asset;
    class AssetManager;
    class ResourceManager;

    template<typename T>
    class AssetHandle {
        static_assert(std::is_base_of_v<Asset, T>, "T must derive from Asset");

        FileID m_file_id{};

    public:
        AssetHandle() = default;
        explicit AssetHandle(const FileID& file_id) : m_file_id(file_id) {}
        AssetHandle(const AssetHandle&) = default;
        AssetHandle& operator=(const AssetHandle&) = default;
        AssetHandle(AssetHandle&&) noexcept = default;
        AssetHandle& operator=(AssetHandle&&) noexcept = default;

        [[nodiscard]] T* get() const;
        T* operator->() const;
        T& operator*() const;

        [[nodiscard]] const FileID& getFileID() const { return m_file_id; }
        [[nodiscard]] Bool isValid() const { return m_file_id.isValid(); }
        [[nodiscard]] explicit operator Bool() const { return isValid(); }

        [[nodiscard]] Bool isLoaded() const;

        Bool operator==(const AssetHandle& other) const {
            return m_file_id == other.m_file_id;
        }
        Bool operator!=(const AssetHandle& other) const {
            return !(m_file_id == other.m_file_id);
        }

        template<typename U, typename = std::enable_if_t<std::is_base_of_v<T, U>>>
        operator AssetHandle<U>() const {
            return AssetHandle<U>(m_file_id);
        }

        void setFileID(const FileID& file_id) { m_file_id = file_id; }
    };

} // dodoe

namespace std {
    template<typename T>
    struct hash<dodoe::AssetHandle<T>> {
        dodoe::Size_t operator()(const dodoe::AssetHandle<T>& handle) const noexcept {
            return static_cast<dodoe::Size_t>(handle.getFileID().getID());
        }
    };
} // namespace std
