// do@Redlive

#pragma once

#include <string>
#include <functional>

#include "runtime/core/utils/json.h"

namespace cakery {

class EditorConfig {
public:
    static EditorConfig& self();

    bool load(const std::string& builtinDir, const std::string& projectDir = "", const std::string& userDir = "");

    const dodoe::Json& editorJson() const { return m_editor; }
    const dodoe::Json& menusJson() const { return m_menus; }
    const dodoe::Json& panelsJson() const { return m_panels; }
    const dodoe::Json& inspectorsJson() const { return m_inspectors; }

    dodoe::Json layoutJson(const std::string& name) const;

    std::string themeName() const;
    std::string defaultLayoutName() const;
    std::string themePath() const;
    std::string builtinDir() const { return m_builtinDir; }

    std::string shortcut(const std::string& action) const;

    void reload();

private:
    EditorConfig() = default;

    dodoe::Json loadJsonFile(const std::string& path) const;
    void mergeOverride(dodoe::Json& base, const std::string& overrideDir, const std::string& filename);

    std::string m_builtinDir;
    std::string m_projectDir;
    std::string m_userDir;

    dodoe::Json m_editor;
    dodoe::Json m_menus;
    dodoe::Json m_panels;
    dodoe::Json m_inspectors;
};

} // namespace cakery
