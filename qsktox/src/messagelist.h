#ifndef MESSAGE_LIST_H
#define MESSAGE_LIST_H

#include <QskScrollArea.h>
#include <QskAnimator.h>
#include <QskPaintedNode.h>
#include <QQuickItem>
#include <QColor>
#include <QVector>
#include <QMap>
#include <QPointer>
#include <private/qquicktaphandler_p.h>

struct MessageItem {
    QString sender;
    QString content;
    QString time;
    bool    isSelf;
};

class MessageRowNode : public QskPaintedNode
{
public:
    void setItem(const MessageItem& item);
    void triggerUpdate(QQuickWindow* window, const QRectF& rect, const QSizeF& size);
    void paint(QPainter*, const QSize&, const void*) override;
    QskHashValue hash(const void*) const override;

private:
    MessageItem m_item;
};

class MessageRowItem : public QQuickItem
{
public:
    MessageRowItem(QQuickItem* parent = nullptr);

    void setMessageData(const MessageItem& item);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private:
    MessageItem m_item;
    bool m_dirty = true;
};

class MessageListWidget : public QskScrollArea
{
    Q_OBJECT
public:
    MessageListWidget(QQuickItem* parent = nullptr);
    ~MessageListWidget() override;

    void populateMessages();

Q_SIGNALS:
    void rowClicked(int row);

protected:
    void geometryChangeEvent(QskGeometryChangeEvent*) override;

private:
    int rowFromPosition(const QPointF& localPos) const;
    void updateVisibleRows();

    QQuickItem* m_contentView = nullptr;
    QVector<MessageItem> m_items;
    QMap<int, MessageRowItem*> m_visibleRows;
    QQuickTapHandler* m_tapHandler = nullptr;
    bool m_longPressFired = false;
};

class MessageListAnimator : public QskAnimator
{
public:
    MessageListAnimator(QQuickItem* target, QObject* parent = nullptr);

protected:
    void advance(qreal value) override;

private:
    QPointer<QQuickItem> m_target;
};

#endif
