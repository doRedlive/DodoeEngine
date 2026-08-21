// do@Redlive

#include "MemoryPanel.h"
#include "framework/EditorContext.h"
#include "runtime/core/memory/memory.h"

#include <QVBoxLayout>
#include <QHeaderView>

namespace cakery {

static const char* kTierNames[] = {"Persistent", "Frame", "Scratch"};
static const char* kTagNames[] = {"Object", "RenderCmd", "Texture", "Resource", "Misc"};

static const int kTierCount = static_cast<int>(dodoe::AllocTier::Count);
static const int kTagCount = static_cast<int>(dodoe::AllocTag::Count);
static const int kRowCount = kTierCount * kTagCount;

MemoryPanel::MemoryPanel(EditorContext& ctx, QWidget* parent)
    : Panel(ctx, parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(3, 3, 3, 3);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({"Tier / Tag", "Current(B)", "Peak(B)", "Allocs"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_table->setRowCount(kRowCount);
    for (int t = 0; t < kTierCount; ++t) {
        for (int g = 0; g < kTagCount; ++g) {
            int row = t * kTagCount + g;
            char label[64];
            snprintf(label, sizeof(label), "%s / %s", kTierNames[t], kTagNames[g]);
            m_table->setItem(row, 0, new QTableWidgetItem(label));
            m_table->setItem(row, 1, new QTableWidgetItem("0"));
            m_table->setItem(row, 2, new QTableWidgetItem("0"));
            m_table->setItem(row, 3, new QTableWidgetItem("0"));
        }
    }

    layout->addWidget(m_table);

    m_timer = new QTimer(this);
    QObject::connect(m_timer, &QTimer::timeout, this, &MemoryPanel::refresh);
    m_timer->start(1000);

    refresh();
}

void MemoryPanel::refresh() {
    for (int t = 0; t < kTierCount; ++t) {
        for (int g = 0; g < kTagCount; ++g) {
            int row = t * kTagCount + g;
            auto& stats = dodoe::Memory::GetStats(
                static_cast<dodoe::AllocTier>(t),
                static_cast<dodoe::AllocTag>(g));
            Size_t cur = stats.current_bytes.load(std::memory_order_relaxed);
            Size_t peak = stats.peak_bytes.load(std::memory_order_relaxed);
            UInt64 allocs = stats.alloc_count.load(std::memory_order_relaxed);

            m_table->item(row, 1)->setText(QString::number(cur));
            m_table->item(row, 2)->setText(QString::number(peak));
            m_table->item(row, 3)->setText(QString::number(allocs));
        }
    }
}

} // namespace cakery
