#ifndef EMOJI_PICKER_H
#define EMOJI_PICKER_H

#include "compat34.h"
#include <vector>

struct EmojiData {
    const char* emoji;
    const char* name;
};

struct EmojiCategory {
    const char* key;
    const char* tabEmoji;
    const EmojiData* items;
};

class PlaceholderLineEdit;

class EmojiPicker : public QWidget {
    Q_OBJECT
public:
    EmojiPicker(QWidget* parent = 0);
    void showAt(const QPoint& pos);

signals:
    void emojiSelected(const QString& emoji);

private slots:
    void onSearchChanged(const QString& query);
    void onTabClicked();
    void onEmojiButtonClicked();

private:
    void rebuildGrid();
    void addToRecent(const QString& emoji);

    int m_activeCategory;
    QString m_searchQuery;
    QStringList m_recentEmojis;
    int m_emojiCellSize;
    std::vector<QPushButton*> m_tabButtons;

    PlaceholderLineEdit* m_searchBar;
    QLabel* m_recentLabel;
    QWidget* m_tabBar;
    QWidget* m_gridArea;
    QWidget* m_pageGrid;

    enum { MAX_RECENT = 30, GRID_COLS = 8 };
};

#endif
