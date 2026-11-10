// do@Redlive

#include "dopch.h"

#include "AssetFileOperations.h"

#include <system_error>

namespace cakery {

namespace {

FsPath MetaFor(const FsPath& path)
{
    FsPath meta = path;
    meta += ".meta";
    return meta;
}

} // namespace

AssetFileResult AssetFileOperations::copyFile(const FsPath& source,
                                              const FsPath& destination)
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(source, ec)) {
        return AssetFileResult::SourceMissing;
    }
    if (std::filesystem::exists(destination, ec)) {
        return AssetFileResult::DestinationExists;
    }
    if (!std::filesystem::copy_file(source, destination,
                                    std::filesystem::copy_options::none, ec) || ec) {
        return AssetFileResult::Failed;
    }
    return AssetFileResult::Ok;
}

AssetFileResult AssetFileOperations::moveFile(const FsPath& source,
                                              const FsPath& destination)
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(source, ec)) {
        return AssetFileResult::SourceMissing;
    }
    if (std::filesystem::exists(destination, ec)) {
        return AssetFileResult::DestinationExists;
    }
    const FsPath sourceMeta = MetaFor(source);
    const FsPath destinationMeta = MetaFor(destination);
    const bool hasMeta = std::filesystem::is_regular_file(sourceMeta, ec);
    if (hasMeta && std::filesystem::exists(destinationMeta, ec)) {
        return AssetFileResult::DestinationExists;
    }
    std::filesystem::rename(source, destination, ec);
    if (ec) {
        return AssetFileResult::Failed;
    }
    if (hasMeta) {
        std::error_code metaError;
        std::filesystem::rename(sourceMeta, destinationMeta, metaError);
    }
    return AssetFileResult::Ok;
}

bool AssetFileOperations::removeWithMeta(const FsPath& path)
{
    std::error_code ec;
    if (!std::filesystem::remove(path, ec) || ec) {
        return false;
    }
    const FsPath meta = MetaFor(path);
    if (std::filesystem::exists(meta, ec)) {
        std::error_code metaError;
        std::filesystem::remove(meta, metaError);
    }
    return true;
}

FsPath AssetFileOperations::metaPath(const FsPath& path)
{
    return MetaFor(path);
}

} // namespace cakery
