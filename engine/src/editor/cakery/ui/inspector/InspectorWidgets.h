// do@Redlive

#pragma once

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSize>
#include <QSizePolicy>
#include <QSpinBox>
#include <QWheelEvent>

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
