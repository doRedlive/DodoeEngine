// do@Redlive
#pragma once
#include "dopch.h"

namespace cakery {

class EditorResourceLocator;

class ThemeController {
public:
    explicit ThemeController(EditorResourceLocator& resources);

    void apply(const String& themeName);

private:
    EditorResourceLocator& m_resources;
};

} // namespace cakery
