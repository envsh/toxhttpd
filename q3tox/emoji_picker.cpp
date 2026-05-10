#include "emoji_picker.h"
#include "emojiwidgets.h"
#include "translator.h"
#include "placeholderlineedit.h"
#include <algorithm>

// ============ Emoji Data ============

static const EmojiData smileys_data[] = {
    {"😀", "grinning face"},
    {"😃", "grinning face with big eyes"},
    {"😄", "grinning face with smiling eyes"},
    {"😁", "beaming face with smiling eyes"},
    {"😆", "grinning squinting face"},
    {"😅", "grinning face with sweat"},
    {"🤣", "rolling on the floor laughing"},
    {"😂", "face with tears of joy"},
    {"🙂", "slightly smiling face"},
    {"🙃", "upside-down face"},
    {"😉", "winking face"},
    {"😊", "smiling face with smiling eyes"},
    {"😇", "smiling face with halo"},
    {"🥰", "smiling face with hearts"},
    {"😍", "smiling face with heart-eyes"},
    {"🤩", "star-struck"},
    {"😘", "face blowing a kiss"},
    {"😗", "kissing face"},
    {"😋", "face savoring food"},
    {"😛", "face with tongue"},
    {"😜", "winking face with tongue"},
    {"🤪", "zany face"},
    {"🤑", "money-mouth face"},
    {"🤗", "hugging face"},
    {"🤭", "face with hand over mouth"},
    {"🤫", "shushing face"},
    {"🤔", "thinking face"},
    {"🤐", "zipper-mouth face"},
    {"😐", "neutral face"},
    {"😑", "expressionless face"},
    {"😶", "face without mouth"},
    {"😏", "smirking face"},
    {"😒", "unamused face"},
    {"🙄", "face with rolling eyes"},
    {"😬", "grimacing face"},
    {"🤥", "lying face"},
    {"😌", "relieved face"},
    {"😔", "pensive face"},
    {"😪", "sleepy face"},
    {"🤤", "drooling face"},
    {"😴", "sleeping face"},
    {0, 0}
};

static const EmojiData people_data[] = {
    {"👋", "waving hand"},
    {"🤚", "raised back of hand"},
    {"✋", "raised hand"},
    {"🖐", "hand with fingers splayed"},
    {"✌", "victory hand"},
    {"🤞", "crossed fingers"},
    {"🤟", "love-you gesture"},
    {"🤘", "sign of the horns"},
    {"👌", "OK hand"},
    {"🤏", "pinching hand"},
    {"👈", "backhand index pointing left"},
    {"👉", "backhand index pointing right"},
    {"👆", "backhand index pointing up"},
    {"👇", "backhand index pointing down"},
    {"👍", "thumbs up"},
    {"👎", "thumbs down"},
    {"✊", "raised fist"},
    {"👊", "oncoming fist"},
    {"🤛", "left-facing fist"},
    {"🤜", "right-facing fist"},
    {"👏", "clapping hands"},
    {"🙌", "raising hands"},
    {"👐", "open hands"},
    {"🤲", "palms up together"},
    {"🤝", "handshake"},
    {"💅", "nail polish"},
    {"👂", "ear"},
    {"👃", "nose"},
    {"🧠", "brain"},
    {"🦶", "foot"},
    {"👀", "eyes"},
    {"👅", "tongue"},
    {"👄", "mouth"},
    {"👶", "baby"},
    {"🧒", "child"},
    {"👦", "boy"},
    {"👧", "girl"},
    {"🧑", "person"},
    {"👨", "man"},
    {"👩", "woman"},
    {"🧔", "bearded person"},
    {"👴", "old man"},
    {"👵", "old woman"},
    {0, 0}
};

static const EmojiData nature_data[] = {
    {"🐶", "dog face"},
    {"🐱", "cat face"},
    {"🐭", "mouse face"},
    {"🐹", "hamster face"},
    {"🐰", "rabbit face"},
    {"🦊", "fox face"},
    {"🐻", "bear face"},
    {"🐼", "panda face"},
    {"🐨", "koala"},
    {"🦁", "lion face"},
    {"🐯", "tiger face"},
    {"🐮", "cow face"},
    {"🐷", "pig face"},
    {"🐸", "frog face"},
    {"🐵", "monkey face"},
    {"🐔", "chicken"},
    {"🐧", "penguin"},
    {"🐦", "bird"},
    {"🐤", "baby chick"},
    {"🦉", "owl"},
    {"🦇", "bat"},
    {"🐺", "wolf"},
    {"🐗", "boar"},
    {"🐴", "horse face"},
    {"🦄", "unicorn"},
    {"🐝", "honeybee"},
    {"🐛", "bug"},
    {"🦋", "butterfly"},
    {"🐌", "snail"},
    {"🐢", "turtle"},
    {"🐍", "snake"},
    {"🐲", "dragon face"},
    {"🐳", "spouting whale"},
    {"🐬", "dolphin"},
    {"🐟", "fish"},
    {"🐙", "octopus"},
    {"🌸", "cherry blossom"},
    {"🌺", "hibiscus"},
    {"🌻", "sunflower"},
    {"🌹", "rose"},
    {"🌷", "tulip"},
    {"🌲", "evergreen tree"},
    {"🌳", "deciduous tree"},
    {"🌴", "palm tree"},
    {"🍀", "four leaf clover"},
    {0, 0}
};

static const EmojiData objects_data[] = {
    {"🍇", "grapes"},
    {"🍈", "melon"},
    {"🍉", "watermelon"},
    {"🍊", "tangerine"},
    {"🍋", "lemon"},
    {"🍌", "banana"},
    {"🍍", "pineapple"},
    {"🥭", "mango"},
    {"🍎", "red apple"},
    {"🍏", "green apple"},
    {"🍐", "pear"},
    {"🍑", "peach"},
    {"🍒", "cherries"},
    {"🍓", "strawberry"},
    {"🥝", "kiwi fruit"},
    {"🍅", "tomato"},
    {"🥥", "coconut"},
    {"🍞", "bread"},
    {"🧀", "cheese wedge"},
    {"🍕", "pizza"},
    {"🍔", "hamburger"},
    {"🍟", "french fries"},
    {"🌭", "hot dog"},
    {"🍿", "popcorn"},
    {"🥓", "bacon"},
    {"🍦", "soft ice cream"},
    {"🍧", "shaved ice"},
    {"🍨", "ice cream"},
    {"🍩", "doughnut"},
    {"🍪", "cookie"},
    {"🎂", "birthday cake"},
    {"🍫", "chocolate"},
    {"🍬", "candy"},
    {"🍭", "lollipop"},
    {"🍮", "custard"},
    {"☕", "hot beverage"},
    {"🍵", "teacup without handle"},
    {"🍺", "beer mug"},
    {"🍻", "clinking beer mugs"},
    {"🥂", "clinking glasses"},
    {"🥤", "cup with straw"},
    {0, 0}
};

static const EmojiData symbols_data[] = {
    {"❤", "red heart"},
    {"🧡", "orange heart"},
    {"💛", "yellow heart"},
    {"💚", "green heart"},
    {"💙", "blue heart"},
    {"💜", "purple heart"},
    {"🖤", "black heart"},
    {"🤍", "white heart"},
    {"💔", "broken heart"},
    {"❣", "heart exclamation"},
    {"💕", "two hearts"},
    {"💞", "revolving hearts"},
    {"💗", "growing heart"},
    {"💖", "sparkling heart"},
    {"💘", "heart with arrow"},
    {"💝", "heart with ribbon"},
    {"⭐", "star"},
    {"🌟", "glowing star"},
    {"✨", "sparkles"},
    {"💫", "dizzy"},
    {"💥", "collision"},
    {"💯", "hundred points"},
    {"🔥", "fire"},
    {"💦", "sweat droplets"},
    {"💨", "dashing away"},
    {"💣", "bomb"},
    {"💬", "speech balloon"},
    {"💤", "zzz"},
    {"🚀", "rocket"},
    {"🛸", "flying saucer"},
    {"🌍", "globe showing Europe-Africa"},
    {"🌏", "globe showing Asia-Australia"},
    {"🌎", "globe showing Americas"},
    {"🎉", "party popper"},
    {"🎊", "confetti ball"},
    {"🏁", "chequered flag"},
    {"🚩", "triangular flag"},
    {"🎈", "balloon"},
    {"🎁", "wrapped gift"},
    {"🏆", "trophy"},
    {"💎", "gem stone"},
    {0, 0}
};

static const EmojiCategory categories[] = {
    {"emoji.smileys", "😀", smileys_data},
    {"emoji.people", "👋", people_data},
    {"emoji.nature", "🐶", nature_data},
    {"emoji.objects", "🍎", objects_data},
    {"emoji.symbols", "❤", symbols_data},
    {0, 0, 0}
};

// ============ EmojiPicker ============

EmojiPicker::EmojiPicker(QWidget* parent)
    : QWidget(parent)
    , m_activeCategory(0)
    , m_emojiCellSize(32)
{
#ifdef QT3_BUILD
    setWFlags(WType_Popup);
#else
    setWindowFlags(Qt::Popup);
#endif
    setFixedWidth(m_emojiCellSize * GRID_COLS + 16);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(2);
    qSetMargins(layout, 4, 4, 4, 4);

    // Search bar
    m_searchBar = new PlaceholderLineEdit(_("emoji.search_placeholder"), this);
    layout->addWidget(m_searchBar);
    connect(m_searchBar, SIGNAL(textChanged(const QString&)),
            this, SLOT(onSearchChanged(const QString&)));

    // Recent label
    m_recentLabel = new QLabel(_("emoji.recently_used"), this);
    QFont recentFont = m_recentLabel->font();
    recentFont.setPointSize(recentFont.pointSize() - 2);
    m_recentLabel->setFont(recentFont);
    layout->addWidget(m_recentLabel);

    // Tab bar
    m_tabBar = new QWidget(this);
    QHBoxLayout* tabLayout = new QHBoxLayout(m_tabBar);
    tabLayout->setSpacing(2);
    qSetMargins(tabLayout, 0, 0, 0, 0);

    for (int i = 0; categories[i].key; i++) {
        QPushButton* btn = new QPushButton(categories[i].tabEmoji, m_tabBar);
        btn->setFixedSize(m_emojiCellSize, m_emojiCellSize);
        btn->setFlat(true);
        connect(btn, SIGNAL(clicked()), this, SLOT(onTabClicked()));
        tabLayout->addWidget(btn);
        m_tabButtons.push_back(btn);
    }
    layout->addWidget(m_tabBar);

    // Grid area (holds the page grid, deleted/recreated on each rebuild)
    m_gridArea = new QWidget(this);
    layout->addWidget(m_gridArea);

    m_pageGrid = 0;
    rebuildGrid();
}

void EmojiPicker::showAt(const QPoint& pos) {
    move(pos);
    show();
}

void EmojiPicker::onSearchChanged(const QString& query) {
    m_searchQuery = query;
    rebuildGrid();
}

void EmojiPicker::onTabClicked() {
    QPushButton* btn = (QPushButton*)sender();
    for (size_t i = 0; i < m_tabButtons.size(); i++) {
        if (m_tabButtons[i] == btn) {
            m_activeCategory = (int)i;
            break;
        }
    }
    m_searchBar->clear();
    m_searchQuery = QString();
    rebuildGrid();
}

void EmojiPicker::onEmojiButtonClicked() {
    QPushButton* btn = (QPushButton*)sender();
    QString emoji = btn->text();
    addToRecent(emoji);
    emit emojiSelected(emoji);
    hide();
}

void EmojiPicker::addToRecent(const QString& emoji) {
#ifdef QT3_BUILD
    m_recentEmojis.remove(emoji);
#else
    m_recentEmojis.removeAll(emoji);
#endif
    m_recentEmojis.prepend(emoji);
    while (m_recentEmojis.size() > MAX_RECENT) {
        m_recentEmojis.pop_back();
    }
}

void EmojiPicker::rebuildGrid() {
    bool searching = !m_searchQuery.isEmpty();

    // Show/hide recent label and tab bar
#ifdef QT3_BUILD
    if (!searching && !m_recentEmojis.isEmpty())
        m_recentLabel->show();
    else
        m_recentLabel->hide();
    if (!searching)
        m_tabBar->show();
    else
        m_tabBar->hide();
#else
    m_recentLabel->setVisible(!searching && !m_recentEmojis.isEmpty());
    m_tabBar->setVisible(!searching);
#endif

    // Rebuild the page grid (delete old, create new)
    delete m_pageGrid;
    m_pageGrid = new QWidget(m_gridArea);
    QBoxLayout* areaLayout = (QBoxLayout*)m_gridArea->layout();
    if (!areaLayout) {
        areaLayout = new QVBoxLayout(m_gridArea);
        areaLayout->setSpacing(0);
        qSetMargins(areaLayout, 0, 0, 0, 0);
    }
    areaLayout->addWidget(m_pageGrid);

    // Determine which emojis to show
    struct Item { const char* emoji; const char* name; };
    std::vector<Item> items;

    if (searching) {
        QString q = m_searchQuery;
        for (int ci = 0; categories[ci].key; ci++) {
            for (int ei = 0; categories[ci].items[ei].emoji; ei++) {
                const EmojiData& d = categories[ci].items[ei];
#ifdef QT3_BUILD
                if (QString(d.name).find(q, 0, false) >= 0)
#else
                if (QString(d.name).contains(q, Qt::CaseInsensitive))
#endif
                {
                    Item it = {d.emoji, d.name};
                    items.push_back(it);
                }
            }
        }
    } else {
        int ci = m_activeCategory;
        if (ci >= 0 && categories[ci].items) {
            for (int ei = 0; categories[ci].items[ei].emoji; ei++) {
                Item it = {categories[ci].items[ei].emoji, categories[ci].items[ei].name};
                items.push_back(it);
            }
        }
    }

    // Build the page content as a vertical layout with optional recent section + emoji grid
    QVBoxLayout* innerLayout = new QVBoxLayout(m_pageGrid);
    innerLayout->setSpacing(1);
#ifdef QT3_BUILD
    innerLayout->setMargin(0);
#else
    innerLayout->setContentsMargins(0, 0, 0, 0);
#endif

    // Recent items section (inside page grid, before emoji grid)
    if (!searching && !m_recentEmojis.isEmpty()) {
#ifdef QT3_BUILD
        QGridLayout* recentGrid = new QGridLayout(1, 1, 1);
#else
        QGridLayout* recentGrid = new QGridLayout();
        recentGrid->setSpacing(1);
#endif
        QWidget* recentW = new QWidget(m_pageGrid);
        int n = (int)m_recentEmojis.size() < MAX_RECENT ? (int)m_recentEmojis.size() : MAX_RECENT;
        for (int i = 0; i < n; i++) {
            int row = i / GRID_COLS;
            int col = i % GRID_COLS;
            QPushButton* btn = new QPushButton(m_recentEmojis[i], recentW);
            btn->setFixedSize(m_emojiCellSize, m_emojiCellSize);
            btn->setFlat(true);
            connect(btn, SIGNAL(clicked()), this, SLOT(onEmojiButtonClicked()));
            recentGrid->addWidget(btn, row, col);
        }
        innerLayout->addWidget(recentW);
    }

    // Emoji grid
    {
#ifdef QT3_BUILD
        QGridLayout* pageLayout = new QGridLayout(1, 1, 1);
#else
        QGridLayout* pageLayout = new QGridLayout();
        pageLayout->setSpacing(1);
#endif
        QWidget* emojiW = new QWidget(m_pageGrid);
        int n = (int)items.size();
        for (int i = 0; i < n; i++) {
            int row = i / GRID_COLS;
            int col = i % GRID_COLS;
            QPushButton* btn = new QPushButton(QString::fromUtf8(items[i].emoji), emojiW);
            btn->setFixedSize(m_emojiCellSize, m_emojiCellSize);
            btn->setFlat(true);
            qSetToolTip(btn, QString::fromUtf8(items[i].name));
            connect(btn, SIGNAL(clicked()), this, SLOT(onEmojiButtonClicked()));
            pageLayout->addWidget(btn, row, col);
        }
        innerLayout->addWidget(emojiW);
    }

    updateGeometry();
    resize(sizeHint());
}
