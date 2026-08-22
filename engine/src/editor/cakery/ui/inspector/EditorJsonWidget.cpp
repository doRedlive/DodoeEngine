// do@Redlive

#include "EditorJsonWidget.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

#include <limits>
#include <string>
#include <vector>

namespace cakery {

namespace {

QDoubleSpinBox* MakeDoubleSpinBox(double value) {
    auto* spin = new QDoubleSpinBox();
    spin->setRange(-1000000.0, 1000000.0);
    spin->setDecimals(6);
    spin->setValue(value);
    return spin;
}

QLabel* MakeNonEditableLabel(const std::string& text) {
    auto* label = new QLabel(QString::fromStdString(text));
    label->setStyleSheet(QStringLiteral("color: #777;"));
    return label;
}

} // namespace

EditorJsonWidget::EditorJsonWidget(const nlohmann::json& value, QWidget* parent)
    : QWidget(parent), m_value(value)
{
    if (!m_value.is_object()) {
        m_value = nlohmann::json::object();
    }
    rebuild();
}

void EditorJsonWidget::setValue(const nlohmann::json& value) {
    if (value == m_value) {
        return;
    }
    m_value = value;
    rebuild();
}

void EditorJsonWidget::rebuild() {
    QLayout* oldLayout = layout();
    if (oldLayout) {
        delete oldLayout;
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    for (auto it = m_value.begin(); it != m_value.end(); ++it) {
        buildField(form, it.key(), it.key(), it.value());
    }
    layout->addLayout(form);
}

void EditorJsonWidget::buildField(QFormLayout* form, const std::string& key, const std::string& path, const nlohmann::json& value) {
    if (value.is_object()) {
        auto* group = new QGroupBox(QString::fromStdString(key));
        auto* subForm = new QFormLayout(group);
        subForm->setContentsMargins(6, 6, 6, 6);
        subForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
        for (auto it = value.begin(); it != value.end(); ++it) {
            const std::string subPath = path + "." + it.key();
            buildField(subForm, it.key(), subPath, it.value());
        }
        form->addRow(group);
        return;
    }

    QLabel* label = new QLabel(QString::fromStdString(key));

    if (value.is_number_integer() || value.is_number_unsigned()) {
        const long long number = value.get<long long>();
        if (number >= std::numeric_limits<int>::min() && number <= std::numeric_limits<int>::max()) {
            auto* spin = new QSpinBox();
            spin->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
            spin->setValue(static_cast<int>(number));
            connect(spin, &QSpinBox::editingFinished, this, [this, path, spin]() {
                valueAt(path) = spin->value();
                emit valueChanged(m_value);
            });
            form->addRow(label, spin);
        } else {
            form->addRow(label, MakeNonEditableLabel(std::to_string(number)));
        }
        return;
    }

    if (value.is_number_float()) {
        auto* spin = MakeDoubleSpinBox(value.get<double>());
        connect(spin, &QDoubleSpinBox::editingFinished, this, [this, path, spin]() {
            valueAt(path) = spin->value();
            emit valueChanged(m_value);
        });
        form->addRow(label, spin);
        return;
    }

    if (value.is_boolean()) {
        auto* check = new QCheckBox();
        check->setChecked(value.get<bool>());
        connect(check, &QCheckBox::toggled, this, [this, path, check](bool on) {
            valueAt(path) = on;
            emit valueChanged(m_value);
        });
        form->addRow(label, check);
        return;
    }

    if (value.is_string()) {
        auto* edit = new QLineEdit(QString::fromStdString(value.get<std::string>()));
        connect(edit, &QLineEdit::editingFinished, this, [this, path, edit]() {
            valueAt(path) = edit->text().toStdString();
            emit valueChanged(m_value);
        });
        form->addRow(label, edit);
        return;
    }

    if (value.is_array()) {
        bool allNumbers = true;
        for (const auto& element : value) {
            if (!element.is_number()) {
                allNumbers = false;
                break;
            }
        }
        if (allNumbers && !value.empty()) {
            auto* container = new QWidget();
            auto* hbox = new QHBoxLayout(container);
            hbox->setContentsMargins(0, 0, 0, 0);
            std::vector<QDoubleSpinBox*> spins;
            for (const auto& element : value) {
                auto* spin = MakeDoubleSpinBox(element.get<double>());
                spins.push_back(spin);
                hbox->addWidget(spin);
            }
            for (auto* spin : spins) {
                connect(spin, &QDoubleSpinBox::editingFinished, this, [this, path, spins]() {
                    nlohmann::json array = nlohmann::json::array();
                    for (auto* s : spins) {
                        array.push_back(s->value());
                    }
                    valueAt(path) = array;
                    emit valueChanged(m_value);
                });
            }
            form->addRow(label, container);
            return;
        }
        form->addRow(label, MakeNonEditableLabel(value.dump()));
        return;
    }

    form->addRow(label, MakeNonEditableLabel("null"));
}

nlohmann::json& EditorJsonWidget::valueAt(const std::string& path) {
    nlohmann::json* node = &m_value;
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t dot = path.find('.', start);
        const std::size_t end = dot == std::string::npos ? path.size() : dot;
        const std::string segment = path.substr(start, end - start);
        node = &(*node)[segment];
        start = end + 1;
    }
    return *node;
}

} // namespace cakery
