// do@Redlive
#pragma once
#include "dopch.h"

#include <QComboBox>
#include <QDoubleSpinBox>
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
        QString text = QDoubleSpinBox::textFromValue(value);
        const QString point = locale().decimalPoint();
        if (text.contains(point)) {
            while (text.endsWith(QLatin1Char('0'))) text.chop(1);
            if (text.endsWith(point)) text.chop(point.size());
        }
        return text == QStringLiteral("-0") ? QStringLiteral("0") : text;
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
