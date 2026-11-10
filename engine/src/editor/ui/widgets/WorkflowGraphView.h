// do@Redlive
#pragma once
#include "dopch.h"

#include <QGraphicsView>
#include <QPen>

#include <cstdint>
#include <unordered_map>

class QGraphicsScene;

namespace cakery {

class WorkflowGraphView : public QGraphicsView {
    Q_OBJECT
public:
    explicit WorkflowGraphView(QWidget* parent = nullptr);

    void setGraph(const Json& graph);
    void highlightNode(UInt64 sequence);

signals:
    void nodeSelected(UInt64 sequence);
    void nodeActivated(UInt64 sequence, const QString& uuid);

protected:
    void wheelEvent(QWheelEvent* event) override;

private:
    QGraphicsScene* m_scene = nullptr;
    UnorderedMap<UInt64, QGraphicsRectItem*> m_itemsBySeq;
    UnorderedMap<UInt64, QPen> m_normalPens;
    UInt64 m_selectedSequence = 0;
    bool m_hadContent = false;
};

} // namespace cakery
