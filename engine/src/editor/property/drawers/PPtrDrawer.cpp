// do@Redlive

#include "PPtrDrawer.h"
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/object/pptr.h"
#include "runtime/resource/file/file_id.h"

#include <QLineEdit>
#include <QHBoxLayout>
#include <QLabel>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

namespace cakery {

namespace {

class PPtrDropLineEdit : public QLineEdit {
public:
    explicit PPtrDropLineEdit(QWidget* parent = nullptr)
        : QLineEdit(parent)
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
            setText(urls.first().toLocalFile());
            event->acceptProposedAction();
        }
    }
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

    auto* edit = new PPtrDropLineEdit(container);

    void* ptr = pc.field->get(pc.componentPtr);
    if (ptr) {
        auto* fileId = static_cast<dodoe::FileID*>(ptr);
        if (fileId->isValid()) {
            edit->setText(QString::fromStdString(fileId->getPath()));
        }
    }

    layout->addWidget(label);
    layout->addWidget(edit, 1);
    return container;
}

void PPtrDrawer::updateValue(const PropertyContext&)
{
}

} // namespace cakery
