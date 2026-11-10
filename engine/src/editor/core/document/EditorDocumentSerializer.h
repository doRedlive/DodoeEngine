// do@Redlive
#pragma once
#include "dopch.h"

#include "EditorDocument.h"

#include <filesystem>


namespace cakery {

class EditorDocumentSerializer {
public:
    static Json toJson(const EditorDocument& document);
    static bool fromJson(const Json& json, EditorDocument& outDocument);
    static bool load(const FsPath& path, EditorDocument& outDocument);
    static bool save(const EditorDocument& document, const FsPath& path);
};

} // namespace cakery
