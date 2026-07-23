#include "messagelist.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QTimer>
#include <QEasingCurve>
#include <QQuickWindow>
#include <QtMath>
#include <QskEvent.h>

static constexpr qreal ROW_HEIGHT = 80;

// ═══════════════════════════════════════════════════════════════════
// 120 条共享模拟消息数据
// ═══════════════════════════════════════════════════════════════════

static const MessageItem s_mockMessages[] = {
    {"Alice", "大家好！今天天气不错 ☀️", "08:00", false},
    {"Bob", "是啊，出去走走？", "08:01", false},
    {"我", "走！下午有空的举手 🙋", "08:02", true},
    {"Charlie", "我我我！", "08:02", false},
    {"Alice", "那就下午两点公园见", "08:03", false},
    {"Dave", "记得带飞盘 🥏", "08:04", false},
    {"我", "我带点水果来", "08:05", true},
    {"Eve", "太好了，我带饮料", "08:06", false},
    {"Frank", "有没有人想打羽毛球？", "08:10", false},
    {"我", "我可以打！", "08:11", true},
    {"Grace", "我也想打，三个人正好轮换", "08:12", false},
    {"Alice", "那我和 Dave 飞盘，你们三个羽毛球", "08:13", false},
    {"Bob", "完美分工 👍", "08:14", false},
    {"我", "下午见！", "08:15", true},
    {"Charlie", "等等，我中午请大家吃火锅怎么样？", "09:00", false},
    {"Dave", "真的吗！太好了 🍲", "09:01", false},
    {"Eve", "火锅！我的最爱！", "09:01", false},
    {"我", "那先吃火锅再运动，完美", "09:02", true},
    {"Frank", "在哪吃？", "09:03", false},
    {"Charlie", "学校旁边那家新开的", "09:04", false},
    {"Grace", "听说那家很好吃，排队排很长", "09:05", false},
    {"Alice", "早点去应该不用排", "09:06", false},
    {"我", "我 11 点就到了", "09:07", true},
    {"Bob", "这么早？你是想先吃一顿吗 😂", "09:08", false},
    {"我", "哈哈哈不是，先占位子", "09:09", true},
    {"Charlie", "行，那 11 点门口集合", "09:10", false},
    {"Dave", "收到 ✅", "09:11", false},
    {"Eve", "好的！", "09:11", false},
    {"Frank", "👍", "09:12", false},
    {"Grace", "今天真开心", "09:13", false},
    {"我", "对了，有人带充电宝吗？我手机快没电了", "09:30", true},
    {"Alice", "我带了两个", "09:31", false},
    {"Bob", "我还带了蓝牙音箱，可以放音乐 🎵", "09:32", false},
    {"我", "太棒了！放点轻松的", "09:33", true},
    {"Charlie", "放周杰伦！", "09:34", false},
    {"Dave", "必须的", "09:34", false},
    {"Eve", "我喜欢听 lofi hip hop", "09:35", false},
    {"Frank", "都行都行，有音乐就行", "09:36", false},
    {"Grace", "我来放吧，我的歌单很全", "09:37", false},
    {"我", "交给你了 🎧", "09:38", true},
    {"Alice", "今天有人拍照吗？", "10:00", false},
    {"Bob", "我带了相机，Canon 的", "10:01", false},
    {"我", "太好了，帮我拍几张帅的", "10:02", true},
    {"Charlie", "哈哈，你本来就帅，随便拍都好看", "10:03", false},
    {"Dave", "这嘴甜的 🍬", "10:04", false},
    {"Eve", "拍合照吧！", "10:05", false},
    {"Frank", "到时候发朋友圈", "10:06", false},
    {"Grace", "我来修图！", "10:07", false},
    {"我", "专业修图师上线了", "10:08", true},
    {"Alice", "那今天就是完美的一天", "10:10", false},
    {"Bob", "对了，下午可能有点热，记得涂防晒", "10:30", false},
    {"我", "好提醒！", "10:31", true},
    {"Charlie", "我带了遮阳伞", "10:32", false},
    {"Dave", "男生也要涂防晒哦，紫外线不分性别", "10:33", false},
    {"Eve", "说得对！", "10:33", false},
    {"Frank", "我带了帽子", "10:34", false},
    {"Grace", "我带了太阳镜 😎", "10:35", false},
    {"Alice", "大家准备太充分了", "10:36", false},
    {"我", "毕竟是一年中最适合出去玩的季节", "10:37", true},
    {"Bob", "夏天就是好", "10:38", false},
    {"Charlie", "突然想到，有没有人会游泳？", "11:00", false},
    {"Dave", "公园里有湖吗？", "11:01", false},
    {"Eve", "有一个人工湖，但是不能游泳", "11:02", false},
    {"我", "那就算了，安全第一", "11:03", true},
    {"Frank", "对对对，生命安全最重要", "11:04", false},
    {"Grace", "可以在湖边散步，风景很好", "11:05", false},
    {"Alice", "还可以喂鱼 🐟", "11:06", false},
    {"Bob", "公园里有卖鱼食吗？", "11:07", false},
    {"我", "应该有小卖部", "11:08", true},
    {"Charlie", "我上次去看到有的", "11:09", false},
    {"Dave", "那就带点面包也行", "11:10", false},
    {"Eve", "面包屑不能喂鱼吧？", "11:11", false},
    {"Grace", "好像可以的，但是不能太多", "11:12", false},
    {"我", "查了一下，确实可以喂面包", "11:15", true},
    {"Frank", "那我带点全麦面包 🍞", "11:16", false},
    {"Alice", "全麦面包喂鱼？鱼也养生吗 😂", "11:17", false},
    {"Bob", "哈哈哈", "11:17", false},
    {"我", "鱼：谢谢，我最近在控制碳水", "11:18", true},
    {"Charlie", "笑死我了", "11:19", false},
    {"Dave", "你们太有才了", "11:20", false},
    {"Eve", "好期待下午的活动！", "11:30", false},
    {"Grace", "我也是！", "11:31", false},
    {"我", "话说有人带桌游吗？", "11:35", true},
    {"Alice", "我带了 UNO！", "11:36", false},
    {"Bob", "UNO 永远的神", "11:37", false},
    {"Charlie", "还有狼人杀", "11:38", false},
    {"Dave", "狼人杀太好玩了", "11:39", false},
    {"Frank", "我带了扑克牌", "11:40", false},
    {"我", "那今天就是桌游大会了", "11:41", true},
    {"Eve", "哈哈，运动+桌游，完美组合", "11:42", false},
    {"Grace", "先吃火锅，再运动，再桌游", "11:43", false},
    {"Alice", "今天安排得满满的", "11:44", false},
    {"Bob", "这才是生活！", "11:45", false},
    {"Charlie", "感觉像夏令营", "12:00", false},
    {"Dave", "对啊，好怀念小时候", "12:01", false},
    {"我", "长大了也要保持童心", "12:02", true},
    {"Eve", "说得真好", "12:03", false},
    {"Grace", "我们就是一群大孩子", "12:04", false},
    {"Alice", "永远年轻，永远热泪盈眶", "12:05", false},
    {"Frank", "太文艺了 😂", "12:06", false},
    {"我", "好了不说了，准备出门！", "12:30", true},
    {"Bob", "冲冲冲！", "12:31", false},
    {"Charlie", "集合集合！", "12:32", false},
    {"Dave", "出发！", "12:33", false},
    {"Eve", "Let's go!", "12:33", false},
    {"Grace", "大家路上注意安全", "12:34", false},
    {"Alice", "到了在群里说一声", "12:35", false},
    {"Frank", "好的！", "12:35", false},
    {"我", "我已经在火锅店了，给你们占了位子", "12:50", true},
    {"Bob", "这么快！", "12:51", false},
    {"Charlie", "等我 5 分钟！", "12:52", false},
    {"Dave", "我也快到了", "12:53", false},
    {"Eve", "在路上了～", "12:54", false},
    {"Grace", "马上到！", "12:55", false},
    {"Alice", "我已经到了门口", "12:56", false},
    {"Frank", "来了来了", "12:57", false},
    {"我", "锅底选了鸳鸯锅，一半辣一半不辣", "13:00", true},
    {"Bob", "明智之选！", "13:01", false},
    {"Charlie", "我要吃毛肚！", "13:02", false},
    {"Dave", "我点虾滑", "13:03", false},
    {"Eve", "我要肥牛！", "13:03", false},
    {"Grace", "菌菇拼盘！", "13:04", false},
    {"Alice", "蔬菜也不能少", "13:05", false},
    {"Frank", "我点个鸭血", "13:06", false},
    {"我", "已经点了，大家看看还要加什么", "13:07", true},
    {"Bob", "再加一份宽粉！", "13:08", false},
    {"Charlie", "还要豆皮！", "13:09", false},
    {"Dave", "冰粉！吃火锅必须配冰粉", "13:10", false},
    {"Eve", "同意！", "13:10", false},
    {"我", "已经加了，还有酸梅汤", "13:11", true},
    {"Grace", "你太贴心了", "13:12", false},
};

static constexpr int s_mockCount = sizeof(s_mockMessages) / sizeof(s_mockMessages[0]);

// ═══════════════════════════════════════════════════════════════════
// MessageRowNode — 绘制单条消息气泡
// ═══════════════════════════════════════════════════════════════════

void MessageRowNode::setItem(const MessageItem& item)
{
    m_item = item;
}

void MessageRowNode::triggerUpdate(QQuickWindow* window, const QRectF& rect, const QSizeF& size)
{
    update(window, rect, size, nullptr);
}

void MessageRowNode::paint(QPainter* painter, const QSize& size, const void*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const int w = size.width();
    const int h = size.height();

    // ── 背景 ──
    painter->fillRect(0, 0, w, h, QColor("#1a1a2e"));

    // ── 发送者名字 ──
    QFont nameFont;
    nameFont.setPixelSize(11);
    painter->setFont(nameFont);

    QFontMetrics nameFm(nameFont);

    // ── 气泡绘制 ──
    QFont msgFont;
    msgFont.setPixelSize(14);
    painter->setFont(msgFont);
    QFontMetrics msgFm(msgFont);

    const int maxBubbleWidth = w * 65 / 100;
    const int paddingH = 12;
    const int paddingV = 8;
    const int namePaddingH = 12;

    // 计算内容需要的宽度
    QString displayContent = m_item.content;
    int contentWidth = msgFm.horizontalAdvance(displayContent);
    if (contentWidth > maxBubbleWidth - paddingH * 2) {
        contentWidth = maxBubbleWidth - paddingH * 2;
    }
    int bubbleWidth = contentWidth + paddingH * 2;
    if (bubbleWidth < 40) {
        bubbleWidth = 40;
    }

    // 计算行数
    int lineHeight = msgFm.height();
    int lines = 1;
    if (contentWidth < msgFm.horizontalAdvance(displayContent)) {
        lines = qCeil((qreal)msgFm.horizontalAdvance(displayContent) / (qreal)contentWidth);
    }
    int textHeight = lines * lineHeight;
    int bubbleHeight = textHeight + paddingV * 2;
    if (bubbleHeight < 36) {
        bubbleHeight = 36;
    }

    int totalRowHeight = 20 + bubbleHeight + 4;

    if (m_item.isSelf) {
        // ── 自己的消息：右侧对齐，蓝色气泡 ──
        int bubbleX = w - bubbleWidth - namePaddingH;
        int bubbleY = 20;

        // 名字
        painter->setPen(QColor("#888"));
        QFontMetrics selfNameFm(nameFont);
        int nameW = selfNameFm.horizontalAdvance(m_item.sender);
        painter->drawText(QRect(w - namePaddingH - nameW, 4, nameW, 16),
            Qt::AlignLeft | Qt::AlignVCenter, m_item.sender);

        // 气泡背景
        painter->setBrush(QColor("#1565C0"));
        painter->setPen(Qt::NoPen);
        QPainterPath bubblePath;
        bubblePath.addRoundedRect(bubbleX, bubbleY, bubbleWidth, bubbleHeight, 10, 10);
        painter->drawPath(bubblePath);

        // 消息文字
        painter->setPen(Qt::white);
        painter->drawText(QRect(bubbleX + paddingH, bubbleY + paddingV,
            contentWidth, textHeight),
            Qt::AlignLeft | Qt::AlignVCenter, displayContent);

        // 时间（左下角）
        QFont timeFont;
        timeFont.setPixelSize(9);
        painter->setFont(timeFont);
        painter->setPen(QColor("#666"));
        int timeW = QFontMetrics(timeFont).horizontalAdvance(m_item.time);
        painter->drawText(QRect(bubbleX, bubbleY + bubbleHeight + 2, timeW, 14),
            Qt::AlignLeft | Qt::AlignVCenter, m_item.time);
    } else {
        // ── 他人的消息：左侧对齐，灰色气泡 ──
        int bubbleX = namePaddingH;
        int bubbleY = 20;

        // 名字
        painter->setPen(QColor("#888"));
        painter->drawText(QRect(namePaddingH, 4, 200, 16),
            Qt::AlignLeft | Qt::AlignVCenter, m_item.sender);

        // 气泡背景
        painter->setBrush(QColor("#2a2a3e"));
        painter->setPen(Qt::NoPen);
        QPainterPath bubblePath2;
        bubblePath2.addRoundedRect(bubbleX, bubbleY, bubbleWidth, bubbleHeight, 10, 10);
        painter->drawPath(bubblePath2);

        // 消息文字
        painter->setPen(QColor("#ddd"));
        painter->drawText(QRect(bubbleX + paddingH, bubbleY + paddingV,
            contentWidth, textHeight),
            Qt::AlignLeft | Qt::AlignVCenter, displayContent);

        // 时间（右下角）
        QFont timeFont;
        timeFont.setPixelSize(9);
        painter->setFont(timeFont);
        painter->setPen(QColor("#666"));
        int timeW = QFontMetrics(timeFont).horizontalAdvance(m_item.time);
        painter->drawText(QRect(bubbleX + bubbleWidth - timeW, bubbleY + bubbleHeight + 2,
            timeW, 14),
            Qt::AlignRight | Qt::AlignVCenter, m_item.time);
    }

    Q_UNUSED(totalRowHeight)
}

QskHashValue MessageRowNode::hash(const void*) const
{
    return qHash(m_item.sender) ^ qHash(m_item.content)
        ^ qHash(m_item.time) ^ qHash(m_item.isSelf);
}

// ═══════════════════════════════════════════════════════════════════
// MessageRowItem
// ═══════════════════════════════════════════════════════════════════

MessageRowItem::MessageRowItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    setHeight(ROW_HEIGHT);
}

void MessageRowItem::setMessageData(const MessageItem& item)
{
    m_item = item;
    m_dirty = true;
    update();
}

QSGNode* MessageRowItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    auto* node = static_cast<MessageRowNode*>(oldNode);
    if (!node) {
        node = new MessageRowNode();
    }
    if (m_dirty) {
        node->setItem(m_item);
        m_dirty = false;
    }
    node->triggerUpdate(window(), QRectF(QPointF(0, 0), QSizeF(width(), height())),
        QSizeF(width(), height()));
    return node;
}

// ═══════════════════════════════════════════════════════════════════
// MessageListWidget — 虚拟渲染滚动列表
// ═══════════════════════════════════════════════════════════════════

MessageListWidget::MessageListWidget(QQuickItem* parent)
    : QskScrollArea(parent)
{
    setFlickableOrientations(Qt::Vertical);
    setItemResizable(false);

    m_contentView = new QQuickItem(this);
    setScrolledItem(m_contentView);

    m_tapHandler = new QQuickTapHandler(this);
    m_tapHandler->setGesturePolicy(QQuickTapHandler::DragThreshold);

    connect(m_tapHandler, &QQuickTapHandler::singleTapped, this,
        [this](QEventPoint pt, Qt::MouseButton) {
            if (m_longPressFired) {
                m_longPressFired = false;
                return;
            }
            int row = rowFromPosition(pt.position());
            if (row >= 0 && row < m_items.size()) {
                Q_EMIT rowClicked(row);
                if (auto* rowItem = m_visibleRows.value(row)) {
                    rowItem->setOpacity(0.5);
                    QTimer::singleShot(150, rowItem, [rowItem]() {
                        rowItem->setOpacity(1.0);
                    });
                }
            }
        });

    connect(m_tapHandler, &QQuickTapHandler::longPressed, this, [this]() {
        m_longPressFired = true;
    });

    connect(this, &QskScrollBox::scrollPosChanged,
        this, &MessageListWidget::updateVisibleRows);
}

MessageListWidget::~MessageListWidget()
{
    for (auto* row : std::as_const(m_visibleRows)) {
        delete row;
    }
    m_visibleRows.clear();
}

void MessageListWidget::populateMessages()
{
    m_items.clear();
    for (int i = 0; i < s_mockCount; ++i) {
        m_items.append(s_mockMessages[i]);
    }

    qreal viewW = viewContentsRect().width();
    if (viewW <= 0) viewW = width();
    m_contentView->setSize(QSizeF(viewW, m_items.size() * ROW_HEIGHT));
    update();
    updateVisibleRows();

    // 滚动到底部
    qreal totalH = m_items.size() * ROW_HEIGHT;
    qreal viewH = viewContentsRect().height();
    if (totalH > viewH) {
        setScrollPos(QPointF(0, totalH - viewH));
    }

    QTimer::singleShot(100, this, [this]() {
        if (window()) {
            auto* anim = new MessageListAnimator(this, this);
            anim->setWindow(window());
            anim->start();
        }
    });
}

void MessageListWidget::updateVisibleRows()
{
    if (!m_contentView || m_items.isEmpty()) {
        return;
    }

    const qreal scrollY = scrollPos().y();
    const qreal viewH = viewContentsRect().height();

    int rowMin = qMax(0, qFloor(scrollY / ROW_HEIGHT) - 2);
    int rowMax = qMin(m_items.size() - 1,
        qCeil((scrollY + viewH) / ROW_HEIGHT) + 2);

    auto it = m_visibleRows.begin();
    while (it != m_visibleRows.end()) {
        if (it.key() < rowMin || it.key() > rowMax) {
            it.value()->deleteLater();
            it = m_visibleRows.erase(it);
        } else {
            ++it;
        }
    }

    for (int i = rowMin; i <= rowMax; ++i) {
        if (!m_visibleRows.contains(i)) {
            auto* row = new MessageRowItem(m_contentView);
            row->setX(0);
            row->setY(i * ROW_HEIGHT);
            row->setWidth(m_contentView->width());
            row->setMessageData(m_items[i]);
            m_visibleRows[i] = row;
        }
    }
}

void MessageListWidget::geometryChangeEvent(QskGeometryChangeEvent* event)
{
    QskScrollArea::geometryChangeEvent(event);
    if (event->isResized() && m_contentView) {
        qreal viewW = viewContentsRect().width();
        if (viewW <= 0) viewW = width();
        m_contentView->setSize(QSizeF(viewW, m_items.size() * ROW_HEIGHT));
        for (auto* row : std::as_const(m_visibleRows)) {
            row->setWidth(m_contentView->width());
        }
        updateVisibleRows();
    }
}

int MessageListWidget::rowFromPosition(const QPointF& localPos) const
{
    const auto vr = viewContentsRect();
    qreal contentY = localPos.y() - vr.top() + scrollPos().y();
    int row = qFloor(contentY / ROW_HEIGHT);
    if (row >= 0 && row < m_items.size()) {
        return row;
    }
    return -1;
}

// ═══════════════════════════════════════════════════════════════════
// MessageListAnimator
// ═══════════════════════════════════════════════════════════════════

MessageListAnimator::MessageListAnimator(QQuickItem* target, QObject* parent)
    : QskAnimator()
    , m_target(target)
{
    Q_UNUSED(parent)
    setDuration(800);
    setEasingCurve(QEasingCurve::OutCubic);

    if (m_target) {
        m_target->setOpacity(0.0);
    }
}

void MessageListAnimator::advance(qreal value)
{
    if (!m_target) {
        return;
    }
    m_target->setOpacity(value);
}

#include "moc_messagelist.cpp"
