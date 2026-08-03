// do@Redlive

#include "PPtrDrawer.h"
#include "framework/EditorContext.h"
#include "framework/command/commands/SetFieldValueCommand.h"
#include "framework/command/CommandStack.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/meta/serializer/serializer.h"
#include "runtime/core/object/pptr.h"
#include "runtime/function/render/texture/sprite.h"
#include "runtime/resource/file/file_id.h"

#include <QLineEdit>
#include <QHBoxLayout>
#include <QLabel>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

#include <functional>
#include <utility>

namespace cakery {

namespace {

class PPtrDropLineEdit : public QLineEdit {
public:
    explicit PPtrDropLineEdit(std::function<void(const QString&)> on_drop, QWidget* parent = nullptr)
        : QLineEdit(parent), m_on_drop(std::move(on_drop))
    {
        setAcceptDrops(true);
        setReadOnly(true);
        setStyleSheet(
            "QLineEdit{border:1px solid #3C3C3C;border-radius:2px;padding:2px 6px;"
            "background:#2D2D2D;color:#C8C8C8;font-size:12px;}"
            "QLineEdit:focus{border-color:#0078D4;}");
        setPlaceholderText("None");
    }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override
    {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        }
    }

    void dropEvent(QDropEvent* event) override
    {
        const auto& urls = event->mimeData()->urls();
        if (!urls.isEmpty()) {
            const QString path = urls.first().toLocalFile();
            setText(path);
            if (m_on_drop) {
                m_on_drop(path);
            }
            event->acceptProposedAction();
        }
    }

private:
    std::function<void(const QString&)> m_on_drop;
};

} // namespace

QWidget* PPtrDrawer::build(const PropertyContext& pc)
{
    auto* container = new QWidget();
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* label = new QLabel(QString::fromUtf8(pc.field->getFieldName()));
    label->setStyleSheet("color:#8C8C8C;font-size:11px;min-width:30px;");

    auto* pptr = static_cast<dodoe::PPtr<dodoe::Sprite>*>(pc.field->get(pc.componentPtr));
    auto field = *pc.field;

    auto* edit = new PPtrDropLineEdit([pc, field](const QString& path) mutable {
        dodoe::PPtr<dodoe::Sprite> sprite(dodoe::FileID(path.toStdString()), dodoe::UUID());
        dodoe::Json old_val = dodoe::Serializer::write(
            *static_cast<dodoe::PPtr<dodoe::Sprite>*>(field.get(pc.componentPtr)));
        dodoe::Json new_val = dodoe::Serializer::write(sprite);
        field.set(pc.componentPtr, &sprite);
        auto cmd = std::make_unique<SetFieldValueCommand>(
            pc.entity, pc.componentName, field.getFieldName(), old_val, new_val);
        pc.ctx->commands().execute(std::move(cmd));
    }, container);

    if (pptr && pptr->getFileID().isValid()) {
        edit->setText(QString::fromStdString(pptr->getFileID().getPath()));
    }

    layout->addWidget(label);
    layout->addWidget(edit, 1);
    return container;
}

void PPtrDrawer::updateValue(const PropertyContext& pc)
{
    (void)pc;
}

} // namespace cakery
