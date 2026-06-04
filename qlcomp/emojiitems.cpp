#include "emojiitems.h"
#include "emojiutil.h"

#ifdef QT3_BUILD
#ifdef EMOJI_RENDER_QT34

// ============ EmojiListBoxText ============

void EmojiListBoxText::paint(QPainter* p) {
    if (!textHasEmoji(text())) { QListBoxText::paint(p); return; }
    QListBox* lb = listBox();
    if (!lb) { QListBoxText::paint(p); return; }
    QRect r = lb->itemRect(this);
    if (isSelected()) {
        p->fillRect(r, lb->colorGroup().highlight());
        p->setPen(lb->colorGroup().highlightedText());
    } else {
        p->fillRect(r, lb->colorGroup().base());
        p->setPen(lb->colorGroup().text());
    }
    EmojiRenderer::instance().drawText(*p, r, text());
}

// ============ EmojiListViewItem ============

void EmojiListViewItem::paintCell(QPainter* p, const QColorGroup& cg,
                                  int column, int width, int alignment) {
    QString t = text(column);
    if (!textHasEmoji(t)) { QListViewItem::paintCell(p, cg, column, width, alignment); return; }
    p->save();
    if (isSelected())
        p->fillRect(0, 0, width, height(), cg.highlight());
    p->setPen(isSelected() ? cg.highlightedText() : cg.text());
    QRect cr(0, 0, width, height());
    EmojiRenderer::instance().drawText(*p, cr, t);
    p->restore();
}

// ============ EmojiTableItem ============

void EmojiTableItem::paint(QPainter* p, const QColorGroup& cg,
                           const QRect& cr, bool selected) {
    if (!textHasEmoji(text())) { QTableItem::paint(p, cg, cr, selected); return; }
    p->save();
    if (selected)
        p->fillRect(cr, cg.highlight());
    p->setPen(selected ? cg.highlightedText() : cg.text());
    EmojiRenderer::instance().drawText(*p, cr, text());
    p->restore();
}

#endif // EMOJI_RENDER_QT34
#endif // QT3_BUILD
