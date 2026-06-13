// do@Redlive

#include "GenericComponentEditor.h"
#include "runtime/core/meta/component_db.h"
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/object/pptr.h"
#include "runtime/core/project/project.h"
#include "runtime/core/utils/common.h"
#include "runtime/core/utils/util.h"
#include "runtime/function/render/framework/camera.h"
#include "runtime/function/render/framework/texture.h"
#include "runtime/function/world/entity.h"
#include "widgets/DragSpinBox.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <cmath>
#include <filesystem>
#include <functional>

namespace cakery {

using namespace dodoe;
namespace fs = std::filesystem;

namespace {

static DragSpinBox* createSpinBox(QWidget* parent,
                                  double min,
                                  double max,
                                  double value,
                                  int decimals,
                                  double step)
{
    auto* sb = new DragSpinBox(parent);
    sb->setRange(min, max);
    sb->setValue(value);
    sb->setDecimals(decimals);
    sb->setSingleStep(step);
    return sb;
}

static QWidget* createLabeledAxis(QWidget* parent, const QString& label, DragSpinBox* spinBox)
{
    auto* wrapper = new QWidget(parent);
    auto* layout = new QVBoxLayout(wrapper);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto* axisLabel = new QLabel(label, wrapper);
    axisLabel->setAlignment(Qt::AlignHCenter);
    axisLabel->setStyleSheet("color: #6272A4; font-size: 10px;");
    layout->addWidget(axisLabel);
    layout->addWidget(spinBox);
    return wrapper;
}

static void setColorButtonStyle(QPushButton* button, const Color& color)
{
    const int r = static_cast<int>(std::round(color.r * 255.0f));
    const int g = static_cast<int>(std::round(color.g * 255.0f));
    const int b = static_cast<int>(std::round(color.b * 255.0f));
    const int a = static_cast<int>(std::round(color.a * 255.0f));

    button->setToolTip(QStringLiteral("RGBA(%1, %2, %3, %4)").arg(color.r, 0, 'f', 3)
                       .arg(color.g, 0, 'f', 3)
                       .arg(color.b, 0, 'f', 3)
                       .arg(color.a, 0, 'f', 3));
    button->setStyleSheet(QString(
        "QPushButton {"
        "  min-width: 72px;"
        "  min-height: 22px;"
        "  border: 1px solid #44475A;"
        "  border-radius: 3px;"
        "  background-color: rgba(%1, %2, %3, %4);"
        "}"
        "QPushButton:hover { border-color: #6272A4; }"
    ).arg(r).arg(g).arg(b).arg(a));
}

static void markComponentDirty(const Entity& entity, const std::string& componentName)
{
    auto ent = entity;
    ComponentDB::self().markComponentDirty(ent, componentName);
}

static QWidget* createVector2Editor(void* instance, FieldAccessor field, std::function<void()> markDirty)
{
    auto* container = new QWidget();
    auto* hLayout = new QHBoxLayout(container);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(4);

    auto* value = static_cast<Vector2f*>(field.get(instance));
    auto fieldCopy = field;

    auto* x = createSpinBox(container, -99999, 99999, value->x, 3, 0.1);
    auto* y = createSpinBox(container, -99999, 99999, value->y, 3, 0.1);

    QObject::connect(x, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        [instance, fieldCopy, markDirty](double v) mutable {
            auto* vec = static_cast<Vector2f*>(fieldCopy.get(instance));
            vec->x = static_cast<float>(v);
            markDirty();
        });
    QObject::connect(y, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        [instance, fieldCopy, markDirty](double v) mutable {
            auto* vec = static_cast<Vector2f*>(fieldCopy.get(instance));
            vec->y = static_cast<float>(v);
            markDirty();
        });

    hLayout->addWidget(createLabeledAxis(container, "X", x));
    hLayout->addWidget(createLabeledAxis(container, "Y", y));
    return container;
}

static QWidget* createVector3Editor(void* instance, FieldAccessor field, std::function<void()> markDirty)
{
    auto* container = new QWidget();
    auto* hLayout = new QHBoxLayout(container);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(4);

    auto* value = static_cast<Vector3f*>(field.get(instance));
    auto fieldCopy = field;

    auto* x = createSpinBox(container, -99999, 99999, value->x, 3, 0.1);
    auto* y = createSpinBox(container, -99999, 99999, value->y, 3, 0.1);
    auto* z = createSpinBox(container, -99999, 99999, value->z, 3, 0.1);

    QObject::connect(x, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        [instance, fieldCopy, markDirty](double v) mutable {
            auto* vec = static_cast<Vector3f*>(fieldCopy.get(instance));
            vec->x = static_cast<float>(v);
            markDirty();
        });
    QObject::connect(y, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        [instance, fieldCopy, markDirty](double v) mutable {
            auto* vec = static_cast<Vector3f*>(fieldCopy.get(instance));
            vec->y = static_cast<float>(v);
            markDirty();
        });
    QObject::connect(z, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        [instance, fieldCopy, markDirty](double v) mutable {
            auto* vec = static_cast<Vector3f*>(fieldCopy.get(instance));
            vec->z = static_cast<float>(v);
            markDirty();
        });

    hLayout->addWidget(createLabeledAxis(container, "X", x));
    hLayout->addWidget(createLabeledAxis(container, "Y", y));
    hLayout->addWidget(createLabeledAxis(container, "Z", z));
    return container;
}

static QWidget* createVector4Editor(void* instance, FieldAccessor field, std::function<void()> markDirty)
{
    auto* container = new QWidget();
    auto* hLayout = new QHBoxLayout(container);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(4);

    auto* value = static_cast<Vector4f*>(field.get(instance));
    auto fieldCopy = field;

    auto* x = createSpinBox(container, -99999, 99999, value->x, 3, 0.1);
    auto* y = createSpinBox(container, -99999, 99999, value->y, 3, 0.1);
    auto* z = createSpinBox(container, -99999, 99999, value->z, 3, 0.1);
    auto* w = createSpinBox(container, -99999, 99999, value->w, 3, 0.1);

    QObject::connect(x, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        [instance, fieldCopy, markDirty](double v) mutable {
            auto* vec = static_cast<Vector4f*>(fieldCopy.get(instance));
            vec->x = static_cast<float>(v);
            markDirty();
        });
    QObject::connect(y, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        [instance, fieldCopy, markDirty](double v) mutable {
            auto* vec = static_cast<Vector4f*>(fieldCopy.get(instance));
            vec->y = static_cast<float>(v);
            markDirty();
        });
    QObject::connect(z, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        [instance, fieldCopy, markDirty](double v) mutable {
            auto* vec = static_cast<Vector4f*>(fieldCopy.get(instance));
            vec->z = static_cast<float>(v);
            markDirty();
        });
    QObject::connect(w, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        [instance, fieldCopy, markDirty](double v) mutable {
            auto* vec = static_cast<Vector4f*>(fieldCopy.get(instance));
            vec->w = static_cast<float>(v);
            markDirty();
        });

    hLayout->addWidget(createLabeledAxis(container, "X", x));
    hLayout->addWidget(createLabeledAxis(container, "Y", y));
    hLayout->addWidget(createLabeledAxis(container, "Z", z));
    hLayout->addWidget(createLabeledAxis(container, "W", w));
    return container;
}

static QWidget* createEnumEditor(void* instance, FieldAccessor field, std::function<void()> markDirty)
{
    const QString typeName = QString::fromLatin1(field.getFieldTypeName());
    if (typeName != "CameraType" && typeName != "dodoe::CameraType") {
        return nullptr;
    }

    auto* combo = new QComboBox();
    combo->addItems({"None", "Perspective", "Orthographic"});

    auto* value = static_cast<CameraType*>(field.get(instance));
    combo->setCurrentIndex(static_cast<int>(*value));

    auto fieldCopy = field;
    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [instance, fieldCopy, markDirty](int index) mutable {
            auto value = static_cast<CameraType>(index);
            fieldCopy.set(instance, &value);
            markDirty();
        });

    return combo;
}

static QWidget* createTextureEditor(void* instance, FieldAccessor field)
{
    auto* edit = new QLineEdit();
    edit->setReadOnly(true);
    edit->setPlaceholderText("None");

    auto* value = static_cast<PPtr<Texture>*>(field.get(instance));
    const std::string currentPath = value->getFileID().getPath();
    edit->setText(QString::fromStdString(currentPath));
    edit->setToolTip(QString::fromStdString(currentPath));
    return edit;
}

static QWidget* createColorEditor(void* instance, FieldAccessor field, std::function<void()> markDirty)
{
    auto* container = new QWidget();
    auto* hLayout = new QHBoxLayout(container);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(4);

    auto* value = static_cast<Color*>(field.get(instance));
    auto fieldCopy = field;

    auto* colorButton = new QPushButton(container);
    colorButton->setCursor(Qt::PointingHandCursor);
    colorButton->setToolTip("Choose Color");

    auto* r = createSpinBox(container, 0, 1, value->r, 3, 0.05);
    auto* g = createSpinBox(container, 0, 1, value->g, 3, 0.05);
    auto* b = createSpinBox(container, 0, 1, value->b, 3, 0.05);
    auto* a = createSpinBox(container, 0, 1, value->a, 3, 0.05);

    auto updateColor = [colorButton, r, g, b, a]() {
        Color c{
            static_cast<float>(r->value()),
            static_cast<float>(g->value()),
            static_cast<float>(b->value()),
            static_cast<float>(a->value())
        };
        setColorButtonStyle(colorButton, c);
    };

    auto applyColor = [instance, fieldCopy, markDirty, colorButton, r, g, b, a]() mutable {
        auto* color = static_cast<Color*>(fieldCopy.get(instance));
        color->r = static_cast<float>(r->value());
        color->g = static_cast<float>(g->value());
        color->b = static_cast<float>(b->value());
        color->a = static_cast<float>(a->value());
        setColorButtonStyle(colorButton, *color);
        markDirty();
    };

    QObject::connect(r, QOverload<double>::of(&QDoubleSpinBox::valueChanged), colorButton, applyColor);
    QObject::connect(g, QOverload<double>::of(&QDoubleSpinBox::valueChanged), colorButton, applyColor);
    QObject::connect(b, QOverload<double>::of(&QDoubleSpinBox::valueChanged), colorButton, applyColor);
    QObject::connect(a, QOverload<double>::of(&QDoubleSpinBox::valueChanged), colorButton, applyColor);

    QObject::connect(colorButton, &QPushButton::clicked, container, [container, r, g, b, a, applyColor]() mutable {
        const QColor current = QColor::fromRgbF(r->value(), g->value(), b->value(), a->value());
        const QColor chosen = QColorDialog::getColor(current, container, QObject::tr("Choose Color"), QColorDialog::ShowAlphaChannel);
        if (!chosen.isValid()) {
            return;
        }

        const QSignalBlocker br(r);
        const QSignalBlocker bg(g);
        const QSignalBlocker bb(b);
        const QSignalBlocker ba(a);
        r->setValue(chosen.redF());
        g->setValue(chosen.greenF());
        b->setValue(chosen.blueF());
        a->setValue(chosen.alphaF());
        applyColor();
    });

    hLayout->addWidget(colorButton);
    hLayout->addWidget(createLabeledAxis(container, "R", r));
    hLayout->addWidget(createLabeledAxis(container, "G", g));
    hLayout->addWidget(createLabeledAxis(container, "B", b));
    hLayout->addWidget(createLabeledAxis(container, "A", a));

    updateColor();
    return container;
}

static QStringList inferredLabels(const QString& typeName, FieldAccessor* subFields, int subCount)
{
    if (typeName == "Vector2f") return {"X", "Y"};
    if (typeName == "Vector3f") return {"X", "Y", "Z"};
    if (typeName == "Vector4f") return {"X", "Y", "Z", "W"};

    QStringList labels;
    labels.reserve(subCount);
    for (int i = 0; i < subCount; ++i) {
        labels << QString::fromLatin1(subFields[i].getFieldName()).toUpper();
    }
    return labels;
}

static QWidget* createReflectedCompositeEditor(void* instance,
                                               FieldAccessor& field,
                                               const QString& typeName,
                                               std::function<void()> markDirty)
{
    TypeMeta subMeta;
    if (!field.get_type_meta(subMeta) || !subMeta.isValid()) {
        return nullptr;
    }

    FieldAccessor* subFields = nullptr;
    const int subCount = subMeta.get_field_list(subFields);
    if (subCount <= 0) {
        delete[] subFields;
        return nullptr;
    }

    auto* container = new QWidget();
    auto* hLayout = new QHBoxLayout(container);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(4);

    const QStringList labels = inferredLabels(typeName, subFields, subCount);
    const bool isColor = (subCount == 4 && labels.size() >= 4 &&
                          labels[0] == "R" && labels[1] == "G" &&
                          labels[2] == "B" && labels[3] == "A");

    if (isColor) {
        // Keep composite RGBA structs editable, but Color gets a dedicated picker below.
    }

    void* subInstance = field.get(instance);
    for (int i = 0; i < subCount && i < labels.size(); ++i) {
        auto* sb = createSpinBox(container, -99999, 99999,
                                 static_cast<double>(*static_cast<float*>(subFields[i].get(subInstance))),
                                 3, 0.1);

        auto subField = subFields[i];
        QObject::connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [subInstance, subField, markDirty](double v) mutable {
                float fv = static_cast<float>(v);
                subField.set(subInstance, &fv);
                markDirty();
            });

        hLayout->addWidget(createLabeledAxis(container, labels[i], sb));
    }

    delete[] subFields;
    return container;
}

static QWidget* createFieldEditor(void* instance,
                                  FieldAccessor& field,
                                  const std::string& componentName,
                                  const Entity& entity)
{
    const QString typeName = QString::fromLatin1(field.getFieldTypeName());
    auto markDirty = [entity, componentName]() mutable {
        markComponentDirty(entity, componentName);
    };

    if (typeName == "float") {
        auto* sb = createSpinBox(nullptr, -99999, 99999,
                                 static_cast<double>(*static_cast<float*>(field.get(instance))),
                                 3, 0.1);
        auto fieldCopy = field;
        QObject::connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [instance, fieldCopy, markDirty](double v) mutable {
                float fv = static_cast<float>(v);
                fieldCopy.set(instance, &fv);
                markDirty();
            });
        return sb;
    }

    if (typeName == "double") {
        auto* sb = createSpinBox(nullptr, -99999, 99999,
                                 *static_cast<double*>(field.get(instance)),
                                 3, 0.1);
        auto fieldCopy = field;
        QObject::connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [instance, fieldCopy, markDirty](double v) mutable {
                fieldCopy.set(instance, &v);
                markDirty();
            });
        return sb;
    }

    if (typeName == "int" || typeName == "int32_t" || typeName == "int32") {
        auto* sb = createSpinBox(nullptr, -99999, 99999,
                                 *static_cast<int*>(field.get(instance)),
                                 0, 1.0);
        auto fieldCopy = field;
        QObject::connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [instance, fieldCopy, markDirty](double v) mutable {
                int iv = static_cast<int>(std::round(v));
                fieldCopy.set(instance, &iv);
                markDirty();
            });
        return sb;
    }

    if (typeName == "uint32_t" || typeName == "unsigned int") {
        auto* sb = createSpinBox(nullptr, 0, 99999,
                                 static_cast<double>(*static_cast<unsigned int*>(field.get(instance))),
                                 0, 1.0);
        auto fieldCopy = field;
        QObject::connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [instance, fieldCopy, markDirty](double v) mutable {
                unsigned int iv = static_cast<unsigned int>(std::round(v));
                fieldCopy.set(instance, &iv);
                markDirty();
            });
        return sb;
    }

    if (typeName == "bool") {
        auto* cb = new QCheckBox();
        cb->setChecked(*static_cast<bool*>(field.get(instance)));
        auto fieldCopy = field;
        QObject::connect(cb, &QCheckBox::toggled,
            [instance, fieldCopy, markDirty](bool v) mutable {
                fieldCopy.set(instance, &v);
                markDirty();
            });
        return cb;
    }

    if (typeName == "std::string" || typeName == "string") {
        auto* le = new QLineEdit();
        le->setText(QString::fromStdString(*static_cast<std::string*>(field.get(instance))));
        auto fieldCopy = field;
        QObject::connect(le, &QLineEdit::editingFinished,
            [le, instance, fieldCopy, markDirty]() mutable {
                std::string s = le->text().toStdString();
                fieldCopy.set(instance, &s);
                markDirty();
            });
        return le;
    }

    if (typeName == "CameraType" || typeName == "dodoe::CameraType") {
        return createEnumEditor(instance, field, markDirty);
    }

    if (typeName.startsWith("PPtr<") && typeName.contains("Texture")) {
        return createTextureEditor(instance, field);
    }

    if (typeName == "Vector2f") return createVector2Editor(instance, field, markDirty);
    if (typeName == "Vector3f") return createVector3Editor(instance, field, markDirty);
    if (typeName == "Vector4f") return createVector4Editor(instance, field, markDirty);
    if (typeName == "Color") return createColorEditor(instance, field, markDirty);

    if (QWidget* composite = createReflectedCompositeEditor(instance, field, typeName, markDirty)) {
        return composite;
    }

    auto* label = new QLabel(QString("(%1)").arg(typeName));
    label->setStyleSheet("color: #888; font-style: italic;");
    return label;
}

} // namespace

GenericComponentEditor::GenericComponentEditor(const QString& typeName, dodoe::Entity ent,
                                               bool canRemove, QWidget* parent)
    : ComponentEditor(typeName, ent, canRemove, parent)
{
    std::string tn = typeName.toStdString();

    auto mutEnt = ent;
    void* componentPtr = ComponentDB::self().getComponentPtr(mutEnt, tn);
    if (!componentPtr) {
        auto* layout = new QVBoxLayout(this);
        auto* label = new QLabel("(Component instance not found)", this);
        label->setStyleSheet("color: #888; font-style: italic;");
        layout->addWidget(label);
        return;
    }

    TypeMeta meta = TypeMeta::newMetaFromName(tn);
    if (!meta.isValid()) {
        auto* layout = new QVBoxLayout(this);
        auto* label = new QLabel("(No reflection data)", this);
        label->setStyleSheet("color: #888; font-style: italic;");
        layout->addWidget(label);
        return;
    }

    FieldAccessor* fields = nullptr;
    int fieldCount = meta.get_field_list(fields);
    if (fieldCount <= 0) {
        auto* layout = new QVBoxLayout(this);
        auto* label = new QLabel("(No serializable fields)", this);
        label->setStyleSheet("color: #888; font-style: italic;");
        layout->addWidget(label);
        delete[] fields;
        return;
    }

    auto* formLayout = new QFormLayout(this);
    formLayout->setContentsMargins(0, 4, 0, 4);
    formLayout->setSpacing(4);

    for (int i = 0; i < fieldCount; ++i) {
        const char* fieldName = fields[i].getFieldName();
        QString label = QString::fromLatin1(fieldName);
        if (!label.isEmpty()) label[0] = label[0].toUpper();

        QWidget* w = createFieldEditor(componentPtr, fields[i], tn, mutEnt);
        if (w) {
            formLayout->addRow(label + ":", w);
        }
    }

    delete[] fields;
}

void GenericComponentEditor::refresh()
{
    QLayout* oldLayout = this->layout();
    if (oldLayout) {
        QLayoutItem* item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        delete oldLayout;
    }

    std::string tn = typeName().toStdString();
    auto mutEnt = entity();
    void* componentPtr = ComponentDB::self().getComponentPtr(mutEnt, tn);
    if (!componentPtr) return;

    TypeMeta meta = TypeMeta::newMetaFromName(tn);
    if (!meta.isValid()) return;

    FieldAccessor* fields = nullptr;
    int fieldCount = meta.get_field_list(fields);
    if (fieldCount <= 0) {
        delete[] fields;
        return;
    }

    auto* formLayout = new QFormLayout(this);
    formLayout->setContentsMargins(0, 4, 0, 4);
    formLayout->setSpacing(4);

    for (int i = 0; i < fieldCount; ++i) {
        const char* fieldName = fields[i].getFieldName();
        QString label = QString::fromLatin1(fieldName);
        if (!label.isEmpty()) label[0] = label[0].toUpper();

        QWidget* w = createFieldEditor(componentPtr, fields[i], tn, mutEnt);
        if (w) {
            formLayout->addRow(label + ":", w);
        }
    }

    delete[] fields;
}

}