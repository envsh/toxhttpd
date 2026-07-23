#ifndef CHANNEL_LIST_H
#define CHANNEL_LIST_H

#include <QskScrollArea.h>
#include <QskAnimator.h>
#include <QskPaintedNode.h>
#include <QQuickItem>
#include <QColor>
#include <QVector>
#include <QMap>
#include <QPointer>
#include <private/qquicktaphandler_p.h>

struct ChannelItem {
    QString avatarLetter;
    QColor  avatarColor;
    QString title;
    QString lastMessage;
    QString time;
    int     unreadCount;
};

class ChannelRowNode : public QskPaintedNode
{
public:
    void setItem(const ChannelItem& item);
    void triggerUpdate(QQuickWindow* window, const QRectF& rect, const QSizeF& size);
    void paint(QPainter*, const QSize&, const void*) override;
    QskHashValue hash(const void*) const override;

private:
    ChannelItem m_item;
};

class ChannelRowItem : public QQuickItem
{
public:
    ChannelRowItem(QQuickItem* parent = nullptr);

    void setChannelData(const ChannelItem& item);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private:
    ChannelItem m_item;
    bool m_dirty = true;
};

class ChannelListWidget : public QskScrollArea
{
    Q_OBJECT
public:
    ChannelListWidget(QQuickItem* parent = nullptr);
    ~ChannelListWidget() override;

    void populateData();

Q_SIGNALS:
    void rowClicked(int row, const QString& chatName);
    void rowLongPressed(int row, const QPointF& scenePos);

protected:
    void geometryChangeEvent(QskGeometryChangeEvent*) override;

private:
    int rowAtPos(const QPointF& pos) const;
    int rowFromPosition(const QPointF& localPos) const;
    void updateVisibleRows();

    QQuickItem* m_contentView = nullptr;
    QVector<ChannelItem> m_items;
    QMap<int, ChannelRowItem*> m_visibleRows;
    QQuickTapHandler* m_tapHandler = nullptr;
    bool m_longPressFired = false;
};

class StaggerFadeAnimator : public QskAnimator
{
public:
    StaggerFadeAnimator(QQuickItem* target, QObject* parent = nullptr);

protected:
    void advance(qreal value) override;

private:
    QPointer<QQuickItem> m_target;
};

#endif
