// do@Redlive
#pragma once
#include "dopch.h"

#include "core/Signal.h"
#include "core/application/DomainEventBus.h"
#include "presentation/ViewModel.h"

namespace cakery {

class AssetController;
class DocumentController;
class SelectionController;

struct HierarchyNode {
    UInt64 uuid = 0;
    String name;
    DynamicArray<HierarchyNode> children;
};

struct HierarchySnapshot {
    DynamicArray<HierarchyNode> roots;
};

class HierarchyViewModel : public ViewModel {
public:
    HierarchyViewModel(DocumentController& documents, SelectionController& selection,
                       AssetController& assets, DomainEventBus& events);
    ~HierarchyViewModel() override;

    const HierarchySnapshot& snapshot() const { return m_snapshot; }

    bool isSelected(UInt64 uuid) const;
    const DynamicArray<UInt64>& selection() const;

    void select(const DynamicArray<UInt64>& uuids);
    void clearSelection();

    UInt64 createEntity(const String& name);
    UInt64 createChildEntity(const String& name, UInt64 parentUuid);
    void deleteEntities(const DynamicArray<UInt64>& uuids);
    void renameEntity(UInt64 uuid, const String& name);
    void reparentEntity(UInt64 uuid, UInt64 parentUuid);
    void moveToRoot(const DynamicArray<UInt64>& uuids);
    void copyEntities(const DynamicArray<UInt64>& uuids);
    void cutEntities(const DynamicArray<UInt64>& uuids);
    void pasteEntities();
    void duplicateEntities(const DynamicArray<UInt64>& uuids);
    bool hasClipboard() const;
    FsPath assetRoot() const;
    bool exportPrefab(UInt64 uuid, const String& path);

    Signal<> selectionChanged;

private:
    void rebuildSnapshot();

    DocumentController& m_documents;
    SelectionController& m_selection;
    AssetController& m_assets;
    DomainEventBus& m_events;
    HierarchySnapshot m_snapshot;
    ScopedConnection m_documentSubscription;
    ScopedConnection m_selectionSubscription;
};

} // namespace cakery
