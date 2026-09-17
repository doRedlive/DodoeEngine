// do@Redlive

#pragma once

#include "dopch.h"

#include "mesh_pass_type.h"

namespace dodoe {

    struct MeshPhaseDesc {
        String name{};
        MeshPassType pass_type{MeshPassType::Opaque};
    };

    class MeshPhaseRegistry {
        DynamicArray<MeshPhaseDesc> m_phases{};
        Bool m_builtin_registered{false};

        MeshPhaseRegistry() = default;

    public:
        static MeshPhaseRegistry& Self();

        void registerBuiltinPhases();
        void registerPhase(const String& name, MeshPassType pass_type);

        [[nodiscard]] Bool find(const String& name, MeshPassType& out_pass_type) const;
        [[nodiscard]] Bool find(const char* name, MeshPassType& out_pass_type) const;
        [[nodiscard]] const DynamicArray<MeshPhaseDesc>& getPhases() const { return m_phases; }
    };

} // dodoe
