#ifndef SEARCHLISTVIEW_H
#define SEARCHLISTVIEW_H

#include "compat34.h"
#include "LimeScrollBar.h"
#include <string>
#include <vector>

class SearchListView : public QWidget {
public:
    struct RowInfo {
        QString title;
        QString detail;
    };

    explicit SearchListView(QWidget* parent = 0);
    void setRows(const std::vector<RowInfo>& rows, const QString& emptyText);
    void setScrollBar(LimeScrollBar* sb) { m_bar = sb; }
    void scrollTo(int v);

protected:
    void paintEvent(QPaintEvent* e);
    void wheelEvent(QWheelEvent* e);
    void resizeEvent(QResizeEvent* e);

private:
    int totalHeight() const;
    void clampScrollY();
    void render(QPainter& p);

    std::vector<RowInfo> m_rows;
    QString m_emptyText;
    int m_scrollY = 0;
    int m_rowHeight = 44;
    LimeScrollBar* m_bar = nullptr;
    QPixmap m_backBuffer;
};

#endif