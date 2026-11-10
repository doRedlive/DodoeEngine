// do@Redlive

#include "dopch.h"

#include "HierarchyViewModel.h"

#include "core/application/AssetController.h"
#include "core/application/DocumentController.h"
#include "core/application/SelectionController.h"
#include "core/document/EditorDocumentModel.h"

#include <filesystem>
#include <system_error>

namespace cakery {

HierarchyViewModel::HierarchyViewModel(DocumentController& documents,
                                       SelectionController& selection,
                                       AssetController& assets)
    : m_documents(documents), m_selection(selection), m_assets(assets)
{
    m_documentSubscription = m_documents.documentModel().subscribe(
        [this]() { rebuildSnapshot(); });
    m_selectionSubscription = m_selection.subscribe(
        [this]() { selectionChanged.fire(); });
    rebuildSnapshot();
}

HierarchyViewModel::~HierarchyViewModel() = default;

bool HierarchyViewModel::isSelected(UInt64 uuid) const
{
    return m_selection.isSelected(uuid);
}

const DynamicArray<UInt64>& HierarchyViewModel::selection() const
{
    return m_selection.selectedAll();
}

void HierarchyViewModel::select(const DynamicArray<UInt64>& uuids)
{
    m_selection.selectMany(uuids);
}

void HierarchyViewModel::clearSelection()
{
    m_selection.clear();
}

UInt64 HierarchyViewModel::createEntity(const String& name)
{
    return m_documents.createEntity(name);
}

UInt64 HierarchyViewModel::createChildEntity(const String& name, UInt64 parentUuid)
{
    const UInt64 childUuid = m_documents.createEntity(name);
    if (childUuid != 0 && parentUuid != 0) {
        m_documents.reparentEntity(childUuid, parentUuid);
    }
    return childUuid;
}

void HierarchyViewModel::deleteEntities(const DynamicArray<UInt64>& uuids)
{
    m_documents.deleteEntities(uuids);
}

void HierarchyViewModel::renameEntity(UInt64 uuid, const String& name)
{
    m_documents.renameEntity(uuid, name);
}

void HierarchyViewModel::reparentEntity(UInt64 uuid, UInt64 parentUuid)
{
    m_documents.reparentEntity(uuid, parentUuid);
}

void HierarchyViewModel::moveToRoot(const DynamicArray<UInt64>& uuids)
{
    for (UInt64 uuid : uuids) {
        m_documents.reparentEntity(uuid, 0);
    }
}

void HierarchyViewModel::copyEntities(const DynamicArray<UInt64>& uuids)
{
    m_documents.copyEntities(uuids);
}

void HierarchyViewModel::cutEntities(const DynamicArray<UInt64>& uuids)
{
    m_documents.cutEntities(uuids);
}

void HierarchyViewModel::pasteEntities()
{
    m_documents.pasteEntities();
}

void HierarchyViewModel::duplicateEntities(const DynamicArray<UInt64>& uuids)
{
    m_documents.duplicateEntities(uuids);
}

bool HierarchyViewModel::hasClipboard() const
{
    return m_documents.hasEntityClipboard();
}

FsPath HierarchyViewModel::assetRoot() const
{
    return m_assets.projectRoot();
}

bool HierarchyViewModel::exportPrefab(UInt64 uuid, const String& path)
{
    const FsPath assetRoot = m_assets.projectRoot();
    String target = path;
    if (!assetRoot.empty()) {
        std::error_code ec;
        const FsPath relative = std::filesystem::relative(FsPath(path.c_str()), assetRoot, ec);
        if (!ec && !relative.empty() && !relative.generic_string().starts_with("..")) {
            target = relative.generic_string();
        }
    }
    return m_documents.exportPrefab(uuid, target);
}

void HierarchyViewModel::rebuildSnapshot()
{
    const EditorDocumentModel& model = m_documents.documentModel();
    const auto& entities = model.entities();

    UnorderedMap<UInt64, DynamicArray<const EditorEntity*>> children;
    DynamicArray<const EditorEntity*> roots;
    for (const EditorEntity& entity : entities) {
        if (entity.parent != 0 && model.findEntity(entity.parent)) {
            children[entity.parent].push_back(&entity);
        } else {
            roots.push_back(&entity);
        }
    }

    const auto build = [&children](auto&& self, const EditorEntity& entity) -> HierarchyNode {
        HierarchyNode node;
        node.uuid = entity.uuid;
        node.name = entity.name;
        const auto it = children.find(entity.uuid);
        if (it != children.end()) {
            node.children.reserve(it->second.size());
            for (const EditorEntity* child : it->second) {
                node.children.push_back(self(self, *child));
            }
        }
        return node;
    };

    m_snapshot.roots.clear();
    m_snapshot.roots.reserve(roots.size());
    for (const EditorEntity* root : roots) {
        m_snapshot.roots.push_back(build(build, *root));
    }

    const DynamicArray<UInt64> selected = m_selection.selectedAll();
    for (UInt64 uuid : selected) {
        if (!model.findEntity(uuid)) {
            m_selection.remove(uuid);
        }
    }

    notifyChanged();
}

} // namespace cakery
