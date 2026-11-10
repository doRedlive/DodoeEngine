// do@Redlive
#pragma once
#include "dopch.h"

#include "core/Signal.h"
#include "core/application/DomainEventBus.h"
#include "core/bridge/EditorTypes.h"

#include <functional>
#include <utility>

namespace cakery {

class EditorSession;

class AssetController {
public:
    AssetController(EditorSession& session, DomainEventBus& events);
    ~AssetController();

    FsPath projectRoot() const;
    bool listAssets(DynamicArray<AssetBrowserEntry>& out) const;
    bool queryThumbnail(const String& path, int size, Json& out) const;
    bool getImportSettings(const String& path, AssetImportSettings& out) const;
    bool updateImportSettings(const String& path, const Json& settings);
    UInt64 guidForPath(const String& path) const;
    DynamicArray<String> referenceHolders(UInt64 guid) const;
    bool refreshPending() const;
    void refreshProgress(Size_t& done, Size_t& total) const;

    void refresh();
    bool importAsset(const String& path);
    bool reimportAsset(const String& path);
    bool createScene(const FsPath& directory, const String& name);

    bool copyFile(const String& source, const FsPath& target) const;
    bool moveFile(const String& source, const FsPath& target) const;
    bool removeFile(const String& path) const;
    bool createDirectory(const FsPath& directory) const;
    bool writeTextFile(const FsPath& path, const String& content) const;
    bool copyExternalFile(const String& source, const FsPath& target, bool overwrite) const;

    String uniqueDestination(const FsPath& directory, const String& fileName) const;
    static String normalizePath(const FsPath& path);

    ScopedConnection subscribeChanged(std::function<void()> onChange);

    Signal<Size_t> missingAssetReferences;

private:
    bool report(bool ok, const char* action);

    EditorSession& m_session;
    DomainEventBus& m_events;
    ScopedConnection m_missingSubscription;
};

} // namespace cakery
