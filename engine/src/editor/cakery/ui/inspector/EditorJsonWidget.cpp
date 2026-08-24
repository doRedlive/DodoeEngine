// do@Redlive

#include "EditorJsonWidget.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace cakery {

namespace {

class ScrubDoubleSpinBox : public QDoubleSpinBox {
public:
    explicit ScrubDoubleSpinBox(QWidget* parent = nullptr) : QDoubleSpinBox(parent) {
        lineEdit()->installEventFilter(this);
    }
protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == lineEdit()) {
            switch (event->type()) {
            case QEvent::MouseButtonPress:
                if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
                    beginScrub(static_cast<QMouseEvent*>(event));
                }
                break;
            case QEvent::MouseMove:
                if (scrub(static_cast<QMouseEvent*>(event))) {
                    return true;
                }
                break;
            case QEvent::MouseButtonRelease:
                if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton
                    && endScrub(static_cast<QMouseEvent*>(event))) {
                    return true;
                }
                break;
            default:
                break;
            }
        }
        return QDoubleSpinBox::eventFilter(watched, event);
    }
    void wheelEvent(QWheelEvent* event) override { event->ignore(); }
    void mousePressEvent(QMouseEvent* event) override {
        QDoubleSpinBox::mousePressEvent(event);
        if (event->button() == Qt::LeftButton) {
            beginScrub(event);
            event->accept();
        }
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if (scrub(event)) {
            event->accept();
            return;
        }
        QDoubleSpinBox::mouseMoveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            endScrub(event);
        }
        QDoubleSpinBox::mouseReleaseEvent(event);
    }
private:
    void beginScrub(QMouseEvent* event) {
        m_pressed = true;
        m_dragging = false;
        m_pressPos = event->position();
        m_lastX = event->globalPosition().x();
        m_startValue = value();
    }
    bool scrub(QMouseEvent* event) {
        if (!m_pressed) {
            return false;
        }
        if (!m_dragging && (event->position() - m_pressPos).manhattanLength() > 4) {
            m_dragging = true;
            setCursor(Qt::SizeHorCursor);
        }
        if (!m_dragging) {
            return false;
        }
        const double step = qMax(qAbs(m_startValue) * 0.01, 0.05);
        setValue(value() + (event->globalPosition().x() - m_lastX) * step);
        m_lastX = event->globalPosition().x();
        return true;
    }
    bool endScrub(QMouseEvent* event) {
        if (!m_pressed) {
            return false;
        }
        m_pressed = false;
        if (!m_dragging) {
            return false;
        }
        m_dragging = false;
        unsetCursor();
        emit editingFinished();
        return true;
    }
    bool m_pressed = false;
    bool m_dragging = false;
    QPointF m_pressPos;
    double m_lastX = 0.0;
    double m_startValue = 0.0;
};

class ScrubIntSpinBox : public QSpinBox {
public:
    explicit ScrubIntSpinBox(QWidget* parent = nullptr) : QSpinBox(parent) {
        lineEdit()->installEventFilter(this);
    }
protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == lineEdit()) {
            switch (event->type()) {
            case QEvent::MouseButtonPress:
                if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
                    beginScrub(static_cast<QMouseEvent*>(event));
                }
                break;
            case QEvent::MouseMove:
                if (scrub(static_cast<QMouseEvent*>(event))) {
                    return true;
                }
                break;
            case QEvent::MouseButtonRelease:
                if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton
                    && endScrub(static_cast<QMouseEvent*>(event))) {
                    return true;
                }
                break;
            default:
                break;
            }
        }
        return QSpinBox::eventFilter(watched, event);
    }
    void wheelEvent(QWheelEvent* event) override { event->ignore(); }
    void mousePressEvent(QMouseEvent* event) override {
        QSpinBox::mousePressEvent(event);
        if (event->button() == Qt::LeftButton) {
            beginScrub(event);
            event->accept();
        }
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if (scrub(event)) {
            event->accept();
            return;
        }
        QSpinBox::mouseMoveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            endScrub(event);
        }
        QSpinBox::mouseReleaseEvent(event);
    }
private:
    void beginScrub(QMouseEvent* event) {
        m_pressed = true;
        m_dragging = false;
        m_pressPos = event->position();
        m_lastX = event->globalPosition().x();
        m_startValue = value();
    }
    bool scrub(QMouseEvent* event) {
        if (!m_pressed) {
            return false;
        }
        if (!m_dragging && (event->position() - m_pressPos).manhattanLength() > 4) {
            m_dragging = true;
            setCursor(Qt::SizeHorCursor);
        }
        if (!m_dragging) {
            return false;
        }
        const int step = qMax(1, qAbs(m_startValue) / 50);
        setValue(value() + static_cast<int>((event->globalPosition().x() - m_lastX) * step));
        m_lastX = event->globalPosition().x();
        return true;
    }
    bool endScrub(QMouseEvent* event) {
        if (!m_pressed) {
            return false;
        }
        m_pressed = false;
        if (!m_dragging) {
            return false;
        }
        m_dragging = false;
        unsetCursor();
        emit editingFinished();
        return true;
    }
    bool m_pressed = false;
    bool m_dragging = false;
    QPointF m_pressPos;
    double m_lastX = 0.0;
    int m_startValue = 0;
};

int DecimalsFor(double value);

QDoubleSpinBox* MakeDoubleSpinBox(double value) {
    auto* spin = new ScrubDoubleSpinBox();
    spin->setRange(-1000000.0, 1000000.0);
    spin->setDecimals(DecimalsFor(value));
    spin->setValue(value);
    return spin;
}

QLabel* MakeNonEditableLabel(const std::string& text) {
    auto* label = new QLabel(QString::fromStdString(text));
    label->setStyleSheet(QStringLiteral("color: #A0A0A0;"));
    return label;
}

int ChannelByte(double v) {
    int value = static_cast<int>(v * 255.0 + 0.5);
    return value < 0 ? 0 : (value > 255 ? 255 : value);
}

QColor ColorFromArray(const nlohmann::json& value) {
    if (value.is_array() && value.size() >= 3) {
        return QColor(ChannelByte(value[0].get<double>()), ChannelByte(value[1].get<double>()),
                      ChannelByte(value[2].get<double>()),
                      value.size() >= 4 ? ChannelByte(value[3].get<double>()) : 255);
    }
    return QColor(255, 255, 255);
}

QString SwatchStyle(const QColor& color) {
    return QStringLiteral("background: %1; border: 1px solid #000000; border-radius: 2px;")
        .arg(color.name());
}

int DecimalsFor(double value) {
    QString text = QString::number(value, 'g', 12);
    const int dot = text.indexOf(QLatin1Char('.'));
    if (dot < 0) {
        return 1;
    }
    int decimals = text.size() - dot - 1;
    while (decimals > 0 && text.at(dot + decimals) == QLatin1Char('0')) {
        --decimals;
    }
    if (decimals == 0) {
        return 1;
    }
    return qMin(decimals, 6);
}

QString TitleCaseKey(const std::string& key) {
    QString result;
    bool capitalize = true;
    for (char c : key) {
        if (c == '_') {
            result += QLatin1Char(' ');
            capitalize = true;
        } else if (capitalize) {
            result += QChar::fromLatin1(c).toUpper();
            capitalize = false;
        } else {
            result += QChar::fromLatin1(c);
        }
    }
    return result;
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
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(7);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    for (auto it = m_value.begin(); it != m_value.end(); ++it) {
        buildField(form, it.key(), it.key(), it.value());
    }
    layout->addLayout(form);
}

void EditorJsonWidget::buildField(QFormLayout* form, const std::string& key, const std::string& path, const nlohmann::json& value) {
    if (value.is_object()) {
        auto* group = new QGroupBox(TitleCaseKey(key));
        group->setObjectName(QStringLiteral("inspectorNestedGroup"));
        auto* subForm = new QFormLayout(group);
        subForm->setContentsMargins(6, 6, 6, 6);
        subForm->setHorizontalSpacing(8);
        subForm->setVerticalSpacing(7);
        subForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
        subForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        for (auto it = value.begin(); it != value.end(); ++it) {
            const std::string subPath = path + "." + it.key();
            buildField(subForm, it.key(), subPath, it.value());
        }
        form->addRow(group);
        return;
    }

    QLabel* label = new QLabel(TitleCaseKey(key));
    label->setObjectName(QStringLiteral("inspectorFieldLabel"));
    label->setMinimumWidth(76);

    if (value.is_number_integer() || value.is_number_unsigned()) {
        if (key == "layer" || key == "mask") {
            form->addRow(label, buildLayerField(path, value));
            return;
        }
        if (key == "id") {
            std::string text;
            if (value.is_number_unsigned()) {
                text = std::to_string(value.get<unsigned long long>());
            } else {
                text = std::to_string(value.get<long long>());
            }
            form->addRow(label, MakeNonEditableLabel(text));
            return;
        }
        if (value.is_number_unsigned()) {
            const unsigned long long number = value.get<unsigned long long>();
            if (number > static_cast<unsigned long long>(std::numeric_limits<int>::max())) {
                form->addRow(label, MakeNonEditableLabel(std::to_string(number)));
                return;
            }
            auto* spin = new ScrubIntSpinBox();
            spin->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
            spin->setValue(static_cast<int>(number));
            connect(spin, &QSpinBox::editingFinished, this, [this, path, spin]() {
                valueAt(path) = spin->value();
                emit valueChanged();
            });
            form->addRow(label, spin);
            return;
        }
        const long long number = value.get<long long>();
        if (number >= std::numeric_limits<int>::min() && number <= std::numeric_limits<int>::max()) {
            auto* spin = new ScrubIntSpinBox();
            spin->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
            spin->setValue(static_cast<int>(number));
            connect(spin, &QSpinBox::editingFinished, this, [this, path, spin]() {
                valueAt(path) = spin->value();
                emit valueChanged();
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
            emit valueChanged();
        });
        form->addRow(label, spin);
        return;
    }

    if (value.is_boolean()) {
        form->addRow(label, buildBoolField(path, value));
        return;
    }

    if (value.is_string()) {
        auto* edit = new QLineEdit(QString::fromStdString(value.get<std::string>()));
        connect(edit, &QLineEdit::editingFinished, this, [this, path, edit]() {
            valueAt(path) = edit->text().toStdString();
            emit valueChanged();
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
            const std::size_t size = value.size();
            const bool isColor = (size == 3 || size == 4)
                && (key.find("color") != std::string::npos || key == "background");
            if (isColor) {
                form->addRow(label, buildColorField(path, value));
            } else {
                form->addRow(label, buildVectorField(path, value));
            }
            return;
        }
        form->addRow(label, MakeNonEditableLabel(value.dump()));
        return;
    }

    form->addRow(label, MakeNonEditableLabel("null"));
}

QWidget* EditorJsonWidget::buildVectorField(const std::string& path, const nlohmann::json& value) {
    auto* container = new QWidget();
    container->setObjectName(QStringLiteral("inspectorVector"));
    auto* hbox = new QHBoxLayout(container);
    hbox->setContentsMargins(0, 0, 0, 0);
    hbox->setSpacing(6);
    std::vector<QDoubleSpinBox*> spins;
    static constexpr const char* axisNames[] = {"X", "Y", "Z", "W"};
    for (std::size_t index = 0; index < value.size(); ++index) {
        auto* axis = new QLabel(QString::fromLatin1(axisNames[index < 4 ? index : 3]), container);
        axis->setObjectName(QStringLiteral("inspectorAxisLabel"));
        axis->setProperty("axis", QString::fromLatin1(axisNames[index < 4 ? index : 3]).toLower());
        hbox->addWidget(axis);
        auto* spin = MakeDoubleSpinBox(value[index].get<double>());
        spin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        spins.push_back(spin);
        hbox->addWidget(spin, 1);
    }
    for (auto* spin : spins) {
        connect(spin, &QDoubleSpinBox::editingFinished, this, [this, path, spins]() {
            nlohmann::json array = nlohmann::json::array();
            for (auto* s : spins) {
                array.push_back(s->value());
            }
            valueAt(path) = array;
            emit valueChanged();
        });
    }
    return container;
}

QWidget* EditorJsonWidget::buildColorField(const std::string& path, const nlohmann::json& value) {
    auto* container = new QWidget();
    container->setObjectName(QStringLiteral("inspectorColorField"));
    auto* hbox = new QHBoxLayout(container);
    hbox->setContentsMargins(4, 2, 4, 2);
    auto color = std::make_shared<QColor>(ColorFromArray(value));
    auto* swatch = new QPushButton(container);
    swatch->setObjectName(QStringLiteral("inspectorColorSwatch"));
    swatch->setFixedSize(28, 16);
    swatch->setCursor(Qt::PointingHandCursor);
    swatch->setStyleSheet(SwatchStyle(*color));
    hbox->addWidget(swatch);
    hbox->addStretch();
    const bool hasAlpha = value.size() >= 4;
    connect(swatch, &QPushButton::clicked, this, [this, path, swatch, color, hasAlpha](bool) {
        const QColor picked = QColorDialog::getColor(*color, swatch, tr("Select Color"));
        if (!picked.isValid()) {
            return;
        }
        *color = picked;
        swatch->setStyleSheet(SwatchStyle(*color));
        nlohmann::json array = nlohmann::json::array();
        array.push_back(picked.redF());
        array.push_back(picked.greenF());
        array.push_back(picked.blueF());
        if (hasAlpha) {
            array.push_back(picked.alphaF());
        }
        valueAt(path) = array;
        emit valueChanged();
    });
    return container;
}

QWidget* EditorJsonWidget::buildBoolField(const std::string& path, const nlohmann::json& value) {
    auto* container = new QWidget();
    container->setObjectName(QStringLiteral("inspectorBoolField"));
    auto* hbox = new QHBoxLayout(container);
    hbox->setContentsMargins(6, 1, 6, 1);
    hbox->setSpacing(6);
    auto* check = new QCheckBox(container);
    check->setChecked(value.get<bool>());
    auto* text = new QLabel(tr("Enabled"), container);
    text->setObjectName(QStringLiteral("inspectorBoolText"));
    hbox->addWidget(check);
    hbox->addWidget(text);
    hbox->addStretch();
    connect(check, &QCheckBox::toggled, this, [this, path, check](bool on) {
        valueAt(path) = on;
        emit valueChanged();
    });
    return container;
}

QWidget* EditorJsonWidget::buildLayerField(const std::string& path, const nlohmann::json& value) {
    auto* container = new QWidget();
    container->setObjectName(QStringLiteral("inspectorLayerGrid"));
    auto* grid = new QGridLayout(container);
    grid->setContentsMargins(4, 4, 4, 4);
    grid->setSpacing(2);
    auto bits = std::make_shared<std::uint32_t>(value.get<std::uint32_t>());
    for (int i = 0; i < 32; ++i) {
        auto* cell = new QPushButton(QString::number(i + 1), container);
        cell->setObjectName(QStringLiteral("inspectorLayerCell"));
        cell->setFixedSize(22, 22);
        cell->setCursor(Qt::PointingHandCursor);
        const bool on = ((*bits >> i) & 1u) != 0;
        cell->setProperty("selected", on);
        connect(cell, &QPushButton::clicked, this, [this, path, bits, i, cell](bool) {
            *bits ^= (1u << i);
            cell->setProperty("selected", ((*bits >> i) & 1u) != 0);
            cell->style()->unpolish(cell);
            cell->style()->polish(cell);
            valueAt(path) = *bits;
            emit valueChanged();
        });
        grid->addWidget(cell, i / 8, i % 8);
    }
    return container;
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
