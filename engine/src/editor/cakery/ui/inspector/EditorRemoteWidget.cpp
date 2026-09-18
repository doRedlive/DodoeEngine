// do@Redlive

#include "EditorRemoteWidget.h"

#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <cstddef>
#include <vector>

namespace cakery {

namespace {

nlohmann::json* JsonAtPath(nlohmann::json& root, const std::string& path)
{
    nlohmann::json* current = &root;
    std::size_t start = 0;
    while (true) {
        const std::size_t dot = path.find('.', start);
        const std::string key =
            path.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (key.empty() || !current->is_object() || !current->contains(key)) {
            return nullptr;
        }
        current = &(*current)[key];
        if (dot == std::string::npos) {
            return current;
        }
        start = dot + 1;
    }
}

} // namespace

EditorRemoteWidget::EditorRemoteWidget(Provider provider, Dispatcher dispatcher, QWidget* parent)
    : EditorRemoteWidget(std::move(provider), std::move(dispatcher), PropertyContext{}, parent)
{
}

EditorRemoteWidget::EditorRemoteWidget(Provider provider, Dispatcher dispatcher, PropertyContext property,
                                       QWidget* parent)
    : QWidget(parent),
      m_provider(std::move(provider)),
      m_dispatcher(std::move(dispatcher)),
      m_property(std::move(property))
{
    m_root = new QVBoxLayout(this);
    m_root->setContentsMargins(0, 0, 0, 0);
    m_root->setSpacing(6);
    refresh();
}

void EditorRemoteWidget::refresh()
{
    if (!m_provider) {
        return;
    }
    nlohmann::json tree;
    if (!m_provider(tree)) {
        return;
    }
    m_tree = tree;
    m_lastJson = tree.dump();
    rebuildFrom(m_tree);
}

bool EditorRemoteWidget::rebuildIfChanged()
{
    if (!m_provider) {
        return false;
    }
    nlohmann::json tree;
    if (!m_provider(tree)) {
        return false;
    }
    const std::string text = tree.dump();
    if (text == m_lastJson) {
        return false;
    }
    QWidget* focused = QApplication::focusWidget();
    if (focused != nullptr && isAncestorOf(focused)) {
        return false;
    }
    m_tree = tree;
    m_lastJson = text;
    rebuildFrom(m_tree);
    return true;
}

void EditorRemoteWidget::rebuildFrom(const nlohmann::json& tree)
{
    m_building = true;
    QLayoutItem* child = nullptr;
    while ((child = m_root->takeAt(0)) != nullptr) {
        if (child->widget()) {
            delete child->widget();
        }
        delete child;
    }
    buildNodes(m_root, tree.value("nodes", nlohmann::json::array()));
    m_root->addStretch();
    m_building = false;
}

void EditorRemoteWidget::buildNodes(QBoxLayout* layout, const nlohmann::json& nodes)
{
    if (!nodes.is_array()) {
        return;
    }
    for (const nlohmann::json& node : nodes) {
        buildNode(layout, node);
    }
}

void EditorRemoteWidget::buildNode(QBoxLayout* layout, const nlohmann::json& node)
{
    if (!node.is_object()) {
        return;
    }
    const std::string type = node.value("type", std::string());

    if (type == "row" || type == "column") {
        auto* container = new QWidget(this);
        auto* box = type == "row" ? static_cast<QBoxLayout*>(new QHBoxLayout(container))
                                  : static_cast<QBoxLayout*>(new QVBoxLayout(container));
        box->setContentsMargins(0, 0, 0, 0);
        box->setSpacing(6);
        buildNodes(box, node.value("children", nlohmann::json::array()));
        layout->addWidget(container);
        return;
    }

    if (type == "foldout") {
        auto* container = new QWidget(this);
        auto* box = new QVBoxLayout(container);
        box->setContentsMargins(0, 0, 0, 0);
        box->setSpacing(4);

        auto* header = new QToolButton(container);
        header->setText(QString::fromStdString(node.value("text", std::string())));
        header->setCheckable(true);
        header->setChecked(node.value("expanded", true));
        header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        header->setArrowType(header->isChecked() ? Qt::DownArrow : Qt::RightArrow);
        box->addWidget(header);

        auto* body = new QWidget(container);
        auto* bodyLayout = new QVBoxLayout(body);
        bodyLayout->setContentsMargins(12, 0, 0, 0);
        bodyLayout->setSpacing(6);
        body->setVisible(header->isChecked());
        buildNodes(bodyLayout, node.value("children", nlohmann::json::array()));
        box->addWidget(body);

        connect(header, &QToolButton::toggled, this, [header, body](bool checked) {
            header->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
            body->setVisible(checked);
        });
        layout->addWidget(container);
        return;
    }

    if (type == "separator") {
        auto* line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        layout->addWidget(line);
        return;
    }

    if (type == "space") {
        auto* spacer = new QWidget(this);
        spacer->setFixedHeight(static_cast<int>(node.value("size", 8.0f)));
        layout->addWidget(spacer);
        return;
    }

    if (type == "label") {
        auto* label = new QLabel(QString::fromStdString(node.value("text", std::string())), this);
        label->setWordWrap(true);
        layout->addWidget(label);
        return;
    }

    if (type == "help") {
        auto* label = new QLabel(QString::fromStdString(node.value("text", std::string())), this);
        label->setWordWrap(true);
        const std::string level = node.value("level", std::string("info"));
        if (level == "error") {
            label->setStyleSheet(QStringLiteral("color:#E06C75;"));
        } else if (level == "warning") {
            label->setStyleSheet(QStringLiteral("color:#E5C07B;"));
        } else {
            label->setStyleSheet(QStringLiteral("color:#98C379;"));
        }
        layout->addWidget(label);
        return;
    }

    if (type == "property") {
        buildPropertyNode(layout, node);
        return;
    }

    auto* row = new QWidget(this);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(6);

    const std::string id = node.value("id", std::string());
    const std::string text = node.value("text", std::string());
    if (!text.empty()) {
        auto* label = new QLabel(QString::fromStdString(text), row);
        label->setMinimumWidth(90);
        rowLayout->addWidget(label);
    }

    QWidget* control = nullptr;

    if (type == "button") {
        auto* button = new QPushButton(QString::fromStdString(text.empty() ? id : text), row);
        button->setEnabled(node.value("enabled", true));
        connect(button, &QPushButton::clicked, this, [this, id]() {
            emitEvent(id, "click", nlohmann::json(true));
        });
        control = button;
    } else if (type == "toggle") {
        auto* check = new QCheckBox(row);
        check->setChecked(node.value("value", false));
        connect(check, &QCheckBox::toggled, this, [this, id](bool value) {
            emitEvent(id, "change", nlohmann::json(value));
        });
        control = check;
    } else if (type == "float") {
        auto* spin = new QDoubleSpinBox(row);
        const double min = node.value("min", 0.0f);
        const double max = node.value("max", 0.0f);
        if (max > min) {
            spin->setRange(min, max);
        } else {
            spin->setRange(-1.0e9, 1.0e9);
        }
        spin->setDecimals(4);
        spin->setValue(node.value("value", 0.0f));
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, id](double value) {
            emitEvent(id, "change", nlohmann::json(value));
        });
        control = spin;
    } else if (type == "int") {
        auto* spin = new QSpinBox(row);
        const int min = node.value("min", 0);
        const int max = node.value("max", 0);
        if (max > min) {
            spin->setRange(min, max);
        } else {
            spin->setRange(-1000000000, 1000000000);
        }
        spin->setValue(node.value("value", 0));
        connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, [this, id](int value) {
            emitEvent(id, "change", nlohmann::json(value));
        });
        control = spin;
    } else if (type == "text") {
        auto* edit = new QLineEdit(row);
        edit->setText(QString::fromStdString(node.value("value", std::string())));
        connect(edit, &QLineEdit::editingFinished, this, [this, id, edit]() {
            emitEvent(id, "change", nlohmann::json(edit->text().toStdString()));
        });
        control = edit;
    } else if (type == "vector3") {
        auto* container = new QWidget(row);
        auto* box = new QHBoxLayout(container);
        box->setContentsMargins(0, 0, 0, 0);
        box->setSpacing(4);
        const std::vector<double> value = node.value("value", std::vector<double>{0.0, 0.0, 0.0});
        QDoubleSpinBox* spins[3] = {nullptr, nullptr, nullptr};
        for (int i = 0; i < 3; ++i) {
            auto* spin = new QDoubleSpinBox(container);
            spin->setDecimals(4);
            spin->setRange(-1.0e9, 1.0e9);
            spin->setValue(i < static_cast<int>(value.size()) ? value[static_cast<std::size_t>(i)] : 0.0);
            box->addWidget(spin);
            spins[i] = spin;
        }
        auto push = [this, id, spins]() {
            nlohmann::json array = nlohmann::json::array();
            array.push_back(spins[0]->value());
            array.push_back(spins[1]->value());
            array.push_back(spins[2]->value());
            emitEvent(id, "change", array);
        };
        for (int i = 0; i < 3; ++i) {
            connect(spins[i], qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                    [push](double) { push(); });
        }
        control = container;
    } else if (type == "color") {
        const std::vector<double> value = node.value("value", std::vector<double>{1.0, 1.0, 1.0, 1.0});
        const auto component = [&value](std::size_t index) {
            return index < value.size() ? value[index] : 1.0;
        };
        QColor color(static_cast<int>(component(0) * 255.0), static_cast<int>(component(1) * 255.0),
                     static_cast<int>(component(2) * 255.0), static_cast<int>(component(3) * 255.0));
        auto* button = new QPushButton(row);
        button->setText(color.name(QColor::HexArgb));
        connect(button, &QPushButton::clicked, this, [this, id, button, color]() mutable {
            const QColor picked =
                QColorDialog::getColor(color, button, tr("Select Color"), QColorDialog::ShowAlphaChannel);
            if (!picked.isValid()) {
                return;
            }
            color = picked;
            button->setText(picked.name(QColor::HexArgb));
            nlohmann::json array = nlohmann::json::array();
            array.push_back(picked.redF());
            array.push_back(picked.greenF());
            array.push_back(picked.blueF());
            array.push_back(picked.alphaF());
            emitEvent(id, "change", array);
        });
        control = button;
    } else if (type == "object" || type == "asset") {
        auto* edit = new QLineEdit(row);
        edit->setText(QString::number(static_cast<qulonglong>(node.value("value", 0ull))));
        connect(edit, &QLineEdit::editingFinished, this, [this, id, edit]() {
            bool ok = false;
            const qulonglong value = edit->text().toULongLong(&ok);
            if (ok) {
                emitEvent(id, "change",
                          nlohmann::json(static_cast<unsigned long long>(value)));
            }
        });
        control = edit;
    }

    if (control != nullptr) {
        rowLayout->addWidget(control, 1);
    }
    layout->addWidget(row);
}

void EditorRemoteWidget::buildPropertyNode(QBoxLayout* layout, const nlohmann::json& node)
{
    const std::string name = node.value("name", std::string());
    auto* row = new QWidget(this);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(6);

    const InspectorFieldMetadata* metadata = nullptr;
    for (const InspectorFieldMetadata& field : m_property.fields) {
        if (field.name == name) {
            metadata = &field;
            break;
        }
    }

    std::string label = node.value("text", std::string());
    if (label.empty()) {
        label = metadata != nullptr ? metadata->name : name;
    }
    auto* labelWidget = new QLabel(QString::fromStdString(label), row);
    labelWidget->setMinimumWidth(90);
    rowLayout->addWidget(labelWidget);

    nlohmann::json* value =
        m_property.value.is_null() ? nullptr : JsonAtPath(m_property.value, name);
    if (value == nullptr) {
        auto* missing = new QLabel(tr("(unbound)"), row);
        missing->setStyleSheet(QStringLiteral("color:#A0A0A0;"));
        rowLayout->addWidget(missing, 1);
        layout->addWidget(row);
        return;
    }

    const auto commit = [this]() {
        if (m_property.onChanged) {
            m_property.onChanged(m_property.value);
        }
    };

    InspectorFieldKind kind = metadata != nullptr ? metadata->kind : InspectorFieldKind::Unknown;
    if (kind == InspectorFieldKind::Unknown) {
        if (value->is_boolean()) {
            kind = InspectorFieldKind::Bool;
        } else if (value->is_number_integer() || value->is_number_unsigned()) {
            kind = InspectorFieldKind::Integer;
        } else if (value->is_number_float()) {
            kind = InspectorFieldKind::Float;
        } else if (value->is_string()) {
            kind = InspectorFieldKind::String;
        } else if (value->is_array()) {
            kind = InspectorFieldKind::Vector;
        }
    }

    QWidget* control = nullptr;

    if (kind == InspectorFieldKind::Bool) {
        auto* check = new QCheckBox(row);
        check->setChecked(value->is_boolean() ? value->get<bool>() : false);
        connect(check, &QCheckBox::toggled, this, [value, commit](bool checked) {
            *value = checked;
            commit();
        });
        control = check;
    } else if (kind == InspectorFieldKind::Integer) {
        auto* spin = new QSpinBox(row);
        const int minimum = metadata != nullptr && metadata->hasRange ? static_cast<int>(metadata->rangeMin) : -1000000000;
        const int maximum = metadata != nullptr && metadata->hasRange ? static_cast<int>(metadata->rangeMax) : 1000000000;
        spin->setRange(minimum, maximum);
        spin->setValue(value->is_number() ? value->get<int>() : 0);
        connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, [value, commit](int v) {
            *value = v;
            commit();
        });
        control = spin;
    } else if (kind == InspectorFieldKind::Float || kind == InspectorFieldKind::Double) {
        auto* spin = new QDoubleSpinBox(row);
        const double minimum = metadata != nullptr && metadata->hasRange ? metadata->rangeMin : -1.0e9;
        const double maximum = metadata != nullptr && metadata->hasRange ? metadata->rangeMax : 1.0e9;
        spin->setRange(minimum, maximum);
        spin->setDecimals(4);
        spin->setValue(value->is_number() ? value->get<double>() : 0.0);
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [value, commit](double v) {
            *value = v;
            commit();
        });
        control = spin;
    } else if (kind == InspectorFieldKind::String) {
        auto* edit = new QLineEdit(row);
        edit->setText(QString::fromStdString(value->is_string() ? value->get<std::string>() : std::string()));
        connect(edit, &QLineEdit::editingFinished, this, [value, commit, edit]() {
            *value = edit->text().toStdString();
            commit();
        });
        control = edit;
    } else if (kind == InspectorFieldKind::Enum && metadata != nullptr) {
        auto* combo = new QComboBox(row);
        const int current = value->is_number() ? value->get<int>() : 0;
        int currentIndex = -1;
        for (std::size_t i = 0; i < metadata->enumValues.size(); ++i) {
            const InspectorEnumValue& entry = metadata->enumValues[i];
            combo->addItem(QString::fromStdString(entry.name), entry.value);
            if (entry.value == current) {
                currentIndex = static_cast<int>(i);
            }
        }
        if (currentIndex >= 0) {
            combo->setCurrentIndex(currentIndex);
        }
        connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, [value, commit, combo](int index) {
            const QVariant data = combo->itemData(index);
            if (data.isValid()) {
                *value = data.toInt();
                commit();
            }
        });
        control = combo;
    } else if (kind == InspectorFieldKind::Color) {
        const std::vector<double> components = value->is_array()
            ? value->get<std::vector<double>>() : std::vector<double>{1.0, 1.0, 1.0, 1.0};
        const auto component = [&components](std::size_t index) {
            return index < components.size() ? components[index] : 1.0;
        };
        QColor color(static_cast<int>(component(0) * 255.0), static_cast<int>(component(1) * 255.0),
                     static_cast<int>(component(2) * 255.0), static_cast<int>(component(3) * 255.0));
        auto* button = new QPushButton(row);
        button->setText(color.name(QColor::HexArgb));
        connect(button, &QPushButton::clicked, this, [this, value, commit, button, color]() mutable {
            const QColor picked =
                QColorDialog::getColor(color, button, tr("Select Color"), QColorDialog::ShowAlphaChannel);
            if (!picked.isValid()) {
                return;
            }
            color = picked;
            button->setText(picked.name(QColor::HexArgb));
            nlohmann::json array = nlohmann::json::array();
            array.push_back(picked.redF());
            array.push_back(picked.greenF());
            array.push_back(picked.blueF());
            array.push_back(picked.alphaF());
            *value = array;
            commit();
        });
        control = button;
    } else if (kind == InspectorFieldKind::AssetHandle || kind == InspectorFieldKind::UUID ||
               kind == InspectorFieldKind::UnsignedInteger) {
        auto* edit = new QLineEdit(row);
        edit->setText(QString::number(value->is_number_unsigned() ? value->get<unsigned long long>() : 0ull));
        connect(edit, &QLineEdit::editingFinished, this, [value, commit, edit]() {
            bool ok = false;
            const qulonglong parsed = edit->text().toULongLong(&ok);
            if (ok) {
                *value = static_cast<unsigned long long>(parsed);
                commit();
            }
        });
        control = edit;
    } else if (kind == InspectorFieldKind::Vector && value->is_array()) {
        const std::size_t count = value->size();
        const std::size_t shown = count == 0 ? 3 : (count <= 4 ? count : 4);
        auto* container = new QWidget(row);
        auto* box = new QHBoxLayout(container);
        box->setContentsMargins(0, 0, 0, 0);
        box->setSpacing(4);
        std::vector<QDoubleSpinBox*> spins;
        spins.reserve(shown);
        for (std::size_t i = 0; i < shown; ++i) {
            auto* spin = new QDoubleSpinBox(container);
            spin->setDecimals(4);
            spin->setRange(-1.0e9, 1.0e9);
            spin->setValue(i < count && (*value)[i].is_number() ? (*value)[i].get<double>() : 0.0);
            box->addWidget(spin);
            spins.push_back(spin);
        }
        for (QDoubleSpinBox* spin : spins) {
            connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                    [value, commit, spins](double) {
                        nlohmann::json array = nlohmann::json::array();
                        for (QDoubleSpinBox* item : spins) {
                            array.push_back(item->value());
                        }
                        *value = array;
                        commit();
                    });
        }
        control = container;
    } else {
        auto* text = new QLabel(QString::fromStdString(value->dump()), row);
        text->setStyleSheet(QStringLiteral("color:#A0A0A0;"));
        control = text;
    }

    if (control != nullptr) {
        rowLayout->addWidget(control, 1);
    }
    layout->addWidget(row);
}

void EditorRemoteWidget::emitEvent(const std::string& controlId, const std::string& event,
                                   const nlohmann::json& value)
{
    if (m_building || !m_dispatcher) {
        return;
    }
    m_dispatcher(controlId, event, value);
}

} // namespace cakery
