// do@Redlive
#pragma once
#include "dopch.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFocusEvent>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSize>
#include <QSizePolicy>
#include <QSpinBox>
#include <QWheelEvent>
#include <QLocale>

namespace cakery {

class ScrubComboBox : public QComboBox {
public:
    explicit ScrubComboBox(QWidget* parent = nullptr)
        : QComboBox(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void wheelEvent(QWheelEvent* event) override
    {
        event->ignore();
    }
};

class ScrubDoubleSpinBox : public QDoubleSpinBox {
public:
    explicit ScrubDoubleSpinBox(QWidget* parent = nullptr)
        : QDoubleSpinBox(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
        setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

protected:
    QString textFromValue(double value) const override
    {
        QLocale displayLocale(locale());
        if (!isGroupSeparatorShown()) {
            displayLocale.setNumberOptions(displayLocale.numberOptions() | QLocale::OmitGroupSeparator);
        }
        QString text = m_editing
            ? QDoubleSpinBox::textFromValue(value)
            : displayLocale.toString(value, 'f', qMin(3, decimals()));
        if (!m_editing && value != 0.0 && displayLocale.toDouble(text) == 0.0) {
            return displayLocale.toString(value, 'g', 3);
        }
        const QString point = locale().decimalPoint();
        if (text.contains(point)) {
            while (text.endsWith(QLatin1Char('0'))) text.chop(1);
            if (text.endsWith(point)) text.chop(point.size());
        }
        return text == QStringLiteral("-0") ? QStringLiteral("0") : text;
    }

    void focusInEvent(QFocusEvent* event) override
    {
        m_editing = true;
        refreshDisplay();
        QDoubleSpinBox::focusInEvent(event);
    }

    void focusOutEvent(QFocusEvent* event) override
    {
        QDoubleSpinBox::focusOutEvent(event);
        m_editing = false;
        refreshDisplay();
    }

    QSize minimumSizeHint() const override
    {
        return QSize(48, QAbstractSpinBox::minimumSizeHint().height());
    }

    QSize sizeHint() const override
    {
        return QSize(88, QAbstractSpinBox::sizeHint().height());
    }

    void wheelEvent(QWheelEvent* event) override
    {
        event->ignore();
    }

private:
    void refreshDisplay()
    {
        // Formatting alone must never be interpreted as a user edit.
        const QSignalBlocker blocker(lineEdit());
        lineEdit()->setText(prefix() + textFromValue(value()) + suffix());
    }

    bool m_editing = false;
};

class ScrubIntSpinBox : public QSpinBox {
public:
    explicit ScrubIntSpinBox(QWidget* parent = nullptr)
        : QSpinBox(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
        setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

protected:
    QSize minimumSizeHint() const override
    {
        return QSize(48, QAbstractSpinBox::minimumSizeHint().height());
    }

    QSize sizeHint() const override
    {
        return QSize(88, QAbstractSpinBox::sizeHint().height());
    }

    void wheelEvent(QWheelEvent* event) override
    {
        event->ignore();
    }
};

} // namespace cakery
