// do@Redlive

#include "PPtrDrawer.h"
#include "framework/EditorContext.h"
#include "framework/command/commands/SetFieldValueCommand.h"
#include "framework/command/CommandStack.h"
#include "framework/asset/AssetDatabase.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/object_id.h"
#include "runtime/core/object/pptr.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>

namespace cakery {

QWidget* PPtrDrawer::build(const PropertyContext& pc)
{
    auto* container = new QWidget();
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* label = new QLabel(QString::fromUtf8(pc.field->getFieldName()));
    label->setStyleSheet("color:#8C8C8C;font-size:11px;min-width:30px;");

    auto* combo = new QComboBox();
    combo->addItem("None");

    const char* assetType = pc.field->attribute("AssetType");
    auto assets = pc.ctx->assets().list(assetType && assetType[0] ? assetType : "");
    for (const auto& a : assets) {
        combo->addItem(QString::fromStdString(a.path));
    }

    auto field = *pc.field;
    void* ptr = field.get(pc.componentPtr);
    if (ptr) {
        auto* pptr = reinterpret_cast<dodoe::PPtr<void>*>(ptr);
        QString cur;
        const auto& id = pptr->getObjectID();
        if (id.isValid()) {
            if (auto info = pc.ctx->assets().findByGuid(id.asset_id)) {
                cur = QString::fromStdString(info->path);
            }
        }
        if (cur.isEmpty() && !pptr->getLegacyPath().empty()) {
            cur = QString::fromUtf8(pptr->getLegacyPath().c_str());
        }
        if (!cur.isEmpty()) {
            int idx = combo->findText(cur);
            if (idx < 0) {
                combo->addItem(cur);
                idx = combo->count() - 1;
            }
            combo->setCurrentIndex(idx);
        }
    }

    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [pc, field, combo](int idx) mutable {
            auto* pptr = reinterpret_cast<dodoe::PPtr<void>*>(field.get(pc.componentPtr));
            const dodoe::ObjectID oldId = pptr ? pptr->getObjectID() : dodoe::ObjectID{};
            std::string oldPath;
            if (oldId.isValid()) {
                if (auto info = pc.ctx->assets().findByGuid(oldId.asset_id)) {
                    oldPath = info->path;
                }
            }
            else if (pptr && !pptr->getLegacyPath().empty()) {
                oldPath = pptr->getLegacyPath().c_str();
            }

            std::string newPath = (idx <= 0) ? "" : combo->itemText(idx).toStdString();
            dodoe::ObjectID newId;
            if (!newPath.empty()) {
                for (const auto& a : pc.ctx->assets().list()) {
                    if (a.path == newPath) {
                        newId = dodoe::ObjectID{a.guid, 0};
                        break;
                    }
                }
            }
            if (pptr) {
                pptr->setIdentity(newId, newId.isValid() ? dodoe::Object::FindInstanceID(newId) : 0);
            }
            auto cmd = std::make_unique<SetFieldValueCommand>(
                pc.entity, pc.componentName, field.getFieldName(),
                dodoe::Json(oldPath), dodoe::Json(newPath));
            pc.ctx->commands().execute(std::move(cmd));
        });

    layout->addWidget(label);
    layout->addWidget(combo, 1);
    return container;
}

void PPtrDrawer::updateValue(const PropertyContext&)
{
}

} // namespace cakery
