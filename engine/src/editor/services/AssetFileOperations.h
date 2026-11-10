// do@Redlive
#pragma once
#include "dopch.h"

#include <filesystem>

namespace cakery {

enum class AssetFileResult {
    Ok,
    SourceMissing,
    DestinationExists,
    Failed
};

class AssetFileOperations {
public:
    static AssetFileResult copyFile(const FsPath& source,
                                    const FsPath& destination);
    static AssetFileResult moveFile(const FsPath& source,
                                    const FsPath& destination);
    static bool removeWithMeta(const FsPath& path);

private:
    static FsPath metaPath(const FsPath& path);
};

} // namespace cakery
