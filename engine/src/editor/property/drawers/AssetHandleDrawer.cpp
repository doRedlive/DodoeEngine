// do@Redlive

#include "AssetHandleDrawer.h"
#include "framework/EditorContext.h"
#include "framework/command/FieldValueUtils.h"
#include "framework/command/CommandStack.h"
#include "framework/command/commands/SetAssetFieldValueCommand.h"
#include "framework/asset/AssetDatabase.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/meta/serializer/serializer.h"
#include "runtime/core/object/object_id.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>

#include <cstring>

namespace cakery {

namespace {

std::string ResolveTargetAssetType(dodoe::FieldAccessor& field)
{
    const char* typeName = field.getFieldTypeName();
    if (typeName) {
        const char* open = std::strchr(typeName, '<');
        const char* close = typeName ? std::strrchr(typeName, '>') : nullptr;
        if (open && close && close > open) {
            std::string inner(open + 1, close);
            if (inner.size() > 5 && inner.compare(inner.size() - 5, 5, "Asset") == 0) {
                inner.resize(inner.size() - 5);
            }
            if (!inner.empty()) return inner;
        }
    }
    const char* attr = field.attribute("AssetType");
    return attr ? attr : std::string();
}

dodoe::Json ObjectIdToJson(const dodoe::ObjectID& id)
{
    return dodoe::Json{{"asset_id", dodoe::Serializer::write(id.asset_id)},
                       {"sub_object_id", static_cast<uint32_t>(id.local_id)}};
}

} // namespace

QWidget* AssetHandleDrawer::build(const PropertyContext& pc)
{
    auto* container = new QWidget();
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* label = new QLabel(QString::fromUtf8(pc.field->getFieldName()));
    label->setStyleSheet("color:#8C8C8C;font-size:11px;min-width:30px;");

    auto* combo = new QComboBox();
    combo->addItem("None");

    std::string targetType = ResolveTargetAssetType(*pc.field);
    auto assets = pc.ctx->assets().list(targetType);
    for (const auto& a : assets) {
        combo->addItem(QString::fromStdString(a.path));
    }

    auto field = *pc.field;
    void* ptr = field.get(pc.componentPtr);
    if (ptr) {
        auto* id = reinterpret_cast<dodoe::ObjectID*>(ptr);
        if (id->isValid()) {
            if (auto info = pc.ctx->assets().findByGuid(id->asset_id)) {
                combo->setCurrentText(QString::fromStdString(info->path));
            }
        }
    }

    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [pc, field, combo](int idx) mutable {
            auto* oldIdPtr = reinterpret_cast<dodoe::ObjectID*>(field.get(pc.componentPtr));
            dodoe::Json oldJson = oldIdPtr ? ObjectIdToJson(*oldIdPtr) : dodoe::Json();

            dodoe::ObjectID newId;
            if (idx > 0) {
                for (const auto& a : pc.ctx->assets().list()) {
                    if (a.path == combo->itemText(idx).toStdString()) {
                        newId = dodoe::ObjectID{a.uuid, 0};
                        break;
                    }
                }
            }

            auto cmd = std::make_unique<SetAssetFieldValueCommand>(
                pc.entity, field.getFieldName(), std::move(oldJson), ObjectIdToJson(newId));
            pc.ctx->commands().execute(std::move(cmd));
        });

    layout->addWidget(label);
    layout->addWidget(combo, 1);
    return container;
}

void AssetHandleDrawer::updateValue(const PropertyContext&)
{
}

} // namespace cakery
