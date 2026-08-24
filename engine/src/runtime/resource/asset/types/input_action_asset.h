// do@Redlive

#pragma once

#include "runtime/resource/asset/asset.h"
#include "runtime/function/input/input_types.h"

namespace dodoe {

    struct InputActionAssetAction {
        String name{};
        InputActionValueType value_type{InputActionValueType::Button};
        DynamicArray<InputBinding> bindings{};
    };

    struct InputActionAssetMap {
        String name{};
        Int priority{0};
        Bool enabled{true};
        Bool consume_input{false};
        DynamicArray<InputActionAssetAction> actions{};
    };

    class InputActionAsset final : public Asset {
        String m_guid{};
        Int m_version{1};
        DynamicArray<InputActionAssetMap> m_maps{};

    public:
        static constexpr AssetType kStaticType = AssetType::InputAction;

        InputActionAsset() { m_meta.type = AssetType::InputAction; }

        [[nodiscard]] Bool loadFromSource(const String& absolute_source_path) override;
        void unloadRuntime() override;
        [[nodiscard]] Bool isReadOnly() const override { return false; }
        [[nodiscard]] Bool saveToSource(const String& absolute_path) const override;
        [[nodiscard]] const DynamicArray<InputActionAssetMap>& getMaps() const { return m_maps; }
        [[nodiscard]] const String& getGuid() const { return m_guid; }
        [[nodiscard]] Int getVersion() const { return m_version; }
    };

} // namespace dodoe
