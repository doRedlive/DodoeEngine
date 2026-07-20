// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/resource/file/file_system.h"

#include <fstream>

namespace dodoe {

    struct ShaderManifestEntry {
        String name;
        String source;
        String entry_point;
        GfxShaderType stage;
        DynamicArray<String> platforms;
    };

    class ShaderManifest {
        DynamicArray<ShaderManifestEntry> m_entries{};
        UnorderedMap<String, Size_t> m_name_index{};

    public:
        Bool loadFromFile(const String& manifest_path);

        const ShaderManifestEntry* find(const String& name) const;
        const DynamicArray<ShaderManifestEntry>& getEntries() const { return m_entries; }
        Size_t getCount() const { return m_entries.size(); }

        static const char* StageToExtension(GfxShaderType stage);

    private:
        static GfxShaderType ParseStage(const String& stage_str);
    };

    inline DynamicArray<char> ReadShaderFile(const String& relative_path) {
        auto full_path = FileSystem::GetEngineResPath() / relative_path;
        std::ifstream in(full_path, std::ios::binary | std::ios::ate);
        if (!in.is_open()) {
            DO_ERROR("Open shader file {} failed!", full_path.string());
            return {};
        }

        const auto size = static_cast<Size_t>(in.tellg());
        in.seekg(0, std::ios::beg);

        DynamicArray<Char> content(size);
        in.read(content.data(), static_cast<std::streamsize>(size));
        return content;
    }

} // namespace dodoe
