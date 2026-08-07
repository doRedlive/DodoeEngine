// do@Redlive

#include "PPtrDrawer.h"
#include "framework/EditorContext.h"
#include "framework/command/commands/SetFieldValueCommand.h"
#include "framework/command/CommandStack.h"
#include "framework/asset/AssetDatabase.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/object/pptr.h"
#include "runtime/resource/file/file_id.h"

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
        auto* fileId = static_cast<dodoe::FileID*>(ptr);
        if (fileId->isValid()) {
            QString cur = QString::fromUtf8(fileId->getPath().c_str());
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
            auto* fileId = static_cast<dodoe::FileID*>(field.get(pc.componentPtr));
            std::string oldPath = (fileId && fileId->isValid()) ? std::string(fileId->getPath().c_str()) : std::string();
            std::string newPath = (idx <= 0) ? "" : combo->itemText(idx).toStdString();
            if (newPath.empty()) {
                *fileId = dodoe::FileID();
            }
            else {
                *fileId = dodoe::FileID(dodoe::String(newPath.c_str(), newPath.size()));
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
