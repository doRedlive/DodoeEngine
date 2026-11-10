// do@Redlive
#pragma once
#include "dopch.h"

#include <filesystem>
#include <string>

namespace cakery {

class EditorResourceLocator {
public:
    explicit EditorResourceLocator(FsPath packagedRoot);

    void setProjectRoot(FsPath projectRoot);
    FsPath resolve(const String& resourceId) const;
    const FsPath& packagedRoot() const;

private:
    FsPath m_packagedRoot;
    FsPath m_projectOverrideRoot;
};

} // namespace cakery
