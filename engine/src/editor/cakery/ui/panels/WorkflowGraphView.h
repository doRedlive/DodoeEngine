// do@Redlive

#pragma once

#include <QGraphicsView>
#include <QPen>

#include <cstdint>
#include <nlohmann/json.hpp>
#include <unordered_map>

class QGraphicsScene;

namespace cakery {

class WorkflowGraphView : public QGraphicsView {    Q_OBJECT
public:
    explicit WorkflowGraphView(QWidget* parent = nullptr);

    void setGraph(const nlohmann::json& graph);
    void highlightNode(std::uint64_t sequence);

signals:
    void nodeSelected(std::uint64_t sequence);
    void nodeActivated(std::uint64_t sequence, const QString& uuid);

protected:
    void wheelEvent(QWheelEvent* event) override;

private:
    QGraphicsScene* m_scene = nullptr;
    std::unordered_map<std::uint64_t, QGraphicsRectItem*> m_itemsBySeq;
    std::unordered_map<std::uint64_t, QPen> m_normalPens;
    std::uint64_t m_selectedSequence = 0;
    bool m_hadContent = false;
};

} // namespace cakery
