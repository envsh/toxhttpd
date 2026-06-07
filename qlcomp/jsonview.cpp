#include "jsonview.h"
#include "cJSON.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qtextedit.h>
#include <qapplication.h>
#include <qlayout.h>
#ifdef QT3_BUILD
#include <qobjectlist.h>
#include <qclipboard.h>
#include <qdialog.h>
#include <qtooltip.h>
#else
#include <QClipboard>
#include <QDialog>
#endif

JsonViewWidget::JsonViewWidget(QWidget* parent)
    : QWidget(parent), m_hoveredLabel(nullptr), m_showRaw(false), m_maxDepth(0), m_totalNodes(0) {
    m_mainLayout = new QVBoxLayout(this);
    qSetMargins(m_mainLayout, 4, 4, 4, 4);
    m_mainLayout->setSpacing(4);

    // Toolbar
    m_toolbar = new QWidget(this);
    QHBoxLayout* tbLayout = new QHBoxLayout(m_toolbar);
    qSetMargins(tbLayout, 0, 0, 0, 0);
    tbLayout->setSpacing(2);

    m_btnPaste = new QPushButton("[+]", m_toolbar);
    m_btnPaste->setFlat(true);
    m_btnPaste->setFixedWidth(24);
#ifdef QT3_BUILD
    QToolTip::add(m_btnPaste, "Paste JSON");
#else
    m_btnPaste->setToolTip("Paste JSON");
#endif
    connect(m_btnPaste, SIGNAL(clicked()), this, SLOT(onPaste()));
    tbLayout->addWidget(m_btnPaste);

    m_btnExpand = new QPushButton("[+]", m_toolbar);
    m_btnExpand->setFlat(true);
    m_btnExpand->setFixedWidth(24);
#ifdef QT3_BUILD
    QToolTip::add(m_btnExpand, "Expand all");
#else
    m_btnExpand->setToolTip("Expand all");
#endif
    connect(m_btnExpand, SIGNAL(clicked()), this, SLOT(expandAll()));
    tbLayout->addWidget(m_btnExpand);

    m_btnCollapse = new QPushButton("[-]", m_toolbar);
    m_btnCollapse->setFlat(true);
    m_btnCollapse->setFixedWidth(24);
#ifdef QT3_BUILD
    QToolTip::add(m_btnCollapse, "Collapse all");
#else
    m_btnCollapse->setToolTip("Collapse all");
#endif
    connect(m_btnCollapse, SIGNAL(clicked()), this, SLOT(collapseAll()));
    tbLayout->addWidget(m_btnCollapse);

    tbLayout->addStretch();

    // [{}] Raw toggle button
    m_btnRaw = new QPushButton("[{}]", m_toolbar);
    m_btnRaw->setFlat(true);
    m_btnRaw->setFixedWidth(24);
    connect(m_btnRaw, SIGNAL(clicked()), this, SLOT(onToggleRaw()));
    tbLayout->addWidget(m_btnRaw);

    m_mainLayout->addWidget(m_toolbar);

    // Content area
    m_contentArea = new QWidget(this);
    m_contentLayout = new QVBoxLayout(m_contentArea);
    qSetMargins(m_contentLayout, 0, 0, 0, 0);
    m_contentLayout->setSpacing(2);
    m_mainLayout->addWidget(m_contentArea, 1);

    // Error label (hidden by default)
    m_errorLabel = new QLabel(m_contentArea);
    m_errorLabel->hide();
    m_contentLayout->addWidget(m_errorLabel);

    // Tree container — wrapped in scroll area
#ifdef QT3_BUILD
    QScrollView* scroll = new QScrollView(m_contentArea);
    scroll->setResizePolicy(QScrollView::AutoOneFit);
    m_treeScroll = scroll;
    m_treeContainer = new QWidget();
    scroll->addChild(m_treeContainer);
#else
    QScrollArea* scroll = new QScrollArea(m_contentArea);
    m_treeScroll = scroll;
    scroll->setWidgetResizable(true);
    m_treeContainer = new QWidget();
    scroll->setWidget(m_treeContainer);
#endif
    m_treeLayout = new QVBoxLayout(m_treeContainer);
    qSetMargins(m_treeLayout, 0, 0, 0, 0);
    m_treeLayout->setSpacing(2);
    m_contentLayout->addWidget(m_treeScroll, 1);

    // Raw text edit (hidden by default)
    m_rawEdit = new QTextEdit(m_contentArea);
    m_rawEdit->setReadOnly(true);
    m_rawEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_rawEdit->hide();
    m_contentLayout->addWidget(m_rawEdit, 1);

    // Status bar
    m_statusBar = new QWidget(this);
    QHBoxLayout* sbLayout = new QHBoxLayout(m_statusBar);
    qSetMargins(sbLayout, 0, 0, 0, 0);
    sbLayout->setSpacing(4);

    m_depthLabel = new QLabel(m_statusBar);
    m_pathLabel = new QLabel(m_statusBar);
    m_pathLabel->setAlignment(Qt::AlignRight);
    sbLayout->addWidget(m_depthLabel);
    sbLayout->addStretch();
    sbLayout->addWidget(m_pathLabel);
    m_mainLayout->addWidget(m_statusBar);

    clearStatusBar();
}

JsonViewWidget::~JsonViewWidget() {
    // Qt 父-子链自然析构，不手动 cleanupTree 避免与 Qt 的 cascade delete 冲突
}

void JsonViewWidget::setJson(const QString& jsonStr) {
    m_rawSource = jsonStr;
    m_showRaw = false;
    m_btnRaw->setText("[{}]");

    if (jsonStr.length() > MAX_INPUT_SIZE) {
        cleanupTree();
        m_errorLabel->setText("<font color='red'>Error: JSON exceeds 16KB limit</font>");
        m_errorLabel->show();
        m_rawEdit->hide();
        m_treeContainer->hide();
        m_btnRaw->hide();
        return;
    }

    m_rawEdit->hide();
    m_treeContainer->show();

    QByteArray utf8 = qToUtf8(jsonStr);
    cJSON* root = cJSON_Parse(utf8.data());
    if (!root) {
        setPlainText(jsonStr);
        return;
    }

    cleanupTree();
    m_errorLabel->hide();
    rebuildTree(root);
    cJSON_Delete(root);
    m_btnRaw->show();
}

void JsonViewWidget::setJson(cJSON* root) {
    if (!root) {
        m_errorLabel->setText("<font color='red'>Error: null JSON</font>");
        m_errorLabel->show();
        return;
    }
    char* jsonStr = cJSON_PrintUnformatted(root);
    m_rawSource = qFromUtf8(jsonStr);
    free(jsonStr);
    m_showRaw = false;
    m_btnRaw->setText("[{}]");
    m_rawEdit->hide();
    m_treeContainer->show();
    m_btnRaw->show();
    cleanupTree();
    m_errorLabel->hide();
    rebuildTree(root);
}

void JsonViewWidget::clear() {
    m_rawSource = QString();
    cleanupTree();
    m_errorLabel->hide();
    clearStatusBar();
    m_rawEdit->hide();
    m_treeContainer->show();
    m_showRaw = false;
    m_btnRaw->hide();
}

void JsonViewWidget::setPlainText(const QString& text) {
    m_rawSource = text;
    m_showRaw = true;
    m_btnRaw->hide();
    cleanupTree();
    m_errorLabel->hide();
    clearStatusBar();

    m_treeContainer->hide();
    m_rawEdit->show();
#ifdef QT3_BUILD
    m_rawEdit->setText(text);
#else
    m_rawEdit->setPlainText(text);
#endif
}

void JsonViewWidget::rebuildTree(cJSON* root) {
    m_maxDepth = 0;
    m_totalNodes = 0;
    m_nodeMap.clear();
    m_toggleMap.clear();

    QWidget* topRow = buildNode(root, "", "", 0, m_treeContainer, false, std::vector<bool>());
    if (topRow) {
        m_treeLayout->addWidget(topRow);
        m_treeLayout->addStretch();
    }

    updateStatusBar(0, "root");
    m_depthLabel->setText("Depth: 0 / " + QString::number(m_maxDepth));
}

QString JsonViewWidget::valueToHtml(cJSON* node) {
    int t = node->type & 0xFF;
    switch (t) {
    case cJSON_String:
        return "<font color='#008000'>\"" + escapeHtml(node->valuestring ? node->valuestring : "") + "\"</font>";
    case cJSON_Number: {
        double d = node->valuedouble;
        if (d == (int)d)
            return "<font color='#0000FF'>" + QString::number((int)d) + "</font>";
        else
            return "<font color='#0000FF'>" + QString::number(d, 'g', 6) + "</font>";
    }
    case cJSON_True:
        return "<font color='#800080'>true</font>";
    case cJSON_False:
        return "<font color='#800080'>false</font>";
    case cJSON_NULL:
        return "<font color='#808080'>null</font>";
    default:
        return "";
    }
}

QString JsonViewWidget::escapeHtml(const QString& s) {
    QString r = s;
    r.replace('&', "&amp;");
    r.replace('<', "&lt;");
    r.replace('>', "&gt;");
    r.replace('"', "&quot;");
    return r;
}

QString JsonViewWidget::typeSummary(cJSON* node) {
    int t = node->type & 0xFF;
    int cnt = childCount(node);
    if (t == cJSON_Object) {
        if (cnt == 0) return "<font color='#808080'>{ }</font>";
        return "<font color='#808080'>{ " + QString::number(cnt) + " item" + (cnt == 1 ? "" : "s") + " }</font>";
    }
    if (t == cJSON_Array) {
        if (cnt == 0) return "<font color='#808080'>[ ]</font>";
        return "<font color='#808080'>[ " + QString::number(cnt) + " value" + (cnt == 1 ? "" : "s") + " ]</font>";
    }
    return "";
}

int JsonViewWidget::childCount(cJSON* node) {
    int cnt = 0;
    cJSON* child = node->child;
    while (child) { ++cnt; child = child->next; }
    return cnt;
}

static QString valuePlainText(cJSON* node) {
    int t = node->type & 0xFF;
    switch (t) {
    case cJSON_String:
        return "\"" + QString::fromUtf8(node->valuestring ? node->valuestring : "") + "\"";
    case cJSON_Number: {
        double d = node->valuedouble;
        if (d == (int)d)
            return QString::number((int)d);
        return QString::number(d, 'g', 6);
    }
    case cJSON_True: return "true";
    case cJSON_False: return "false";
    case cJSON_NULL: return "null";
    default: return "";
    }
}

static QString stripHtml(const QString& html) {
    QString plain;
    bool inTag = false;
    for (int i = 0; i < html.length(); ++i) {
        if (html[i] == '<') { inTag = true; continue; }
        if (html[i] == '>') { inTag = false; continue; }
        if (!inTag) plain += html[i];
    }
    return plain;
}

static QString buildTreePrefix(int depth, bool hasNext, const std::vector<bool>& ancestors) {
    QString p;
    for (int d = 0; d < depth; ++d) {
        if (d < (int)ancestors.size() && ancestors[d])
            p += QString::fromUtf8("│   ");
        else
            p += "    ";
    }
    if (depth > 0) {
        if (hasNext)
            p += QString::fromUtf8("├── ");
        else
            p += QString::fromUtf8("└── ");
    }
    return p;
}

QWidget* JsonViewWidget::createLabel(const QString& html, QWidget* parent) {
#ifdef QT3_BUILD
    QTextEdit* te = new QTextEdit(parent);
    te->setReadOnly(true);
    te->setFrameStyle(QFrame::NoFrame);
    te->setWordWrap(QTextEdit::NoWrap);
    te->setText(html);
    QFontMetrics fm(te->font());
    te->setFixedHeight(fm.lineSpacing() + 4);
    return te;
#else
    QLabel* label = new QLabel(html, parent);
    label->setWordWrap(false);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
#endif
}

QWidget* JsonViewWidget::buildNode(cJSON* node, const QString& key, const QString& path, int depth, QWidget* parent, bool hasNext, const std::vector<bool>& ancestors) {
    if (!node) return nullptr;
    if (depth > m_maxDepth) m_maxDepth = depth;
    ++m_totalNodes;

    int t = node->type & 0xFF;
    bool isContainer = (t == cJSON_Object || t == cJSON_Array);

    QWidget* row = new QWidget(parent);
    QVBoxLayout* rowVLayout = new QVBoxLayout(row);
    qSetMargins(rowVLayout, 0, 0, 0, 0);
    rowVLayout->setSpacing(0);

    QWidget* headerRow = new QWidget(row);
    QHBoxLayout* rowLayout = new QHBoxLayout(headerRow);
    qSetMargins(rowLayout, 0, 0, 0, 0);
    rowLayout->setSpacing(4);

    // Tree prefix (indentation with tree lines)
    QLabel* treeLabel = nullptr;
    if (depth > 0) {
        treeLabel = new QLabel(buildTreePrefix(depth, hasNext, ancestors), headerRow);
        QFont tf("monospace", 10);
        tf.setStyleHint(QFont::TypeWriter);
        treeLabel->setFont(tf);
        rowLayout->addWidget(treeLabel);
    }

    QWidget* keyLabel = nullptr;
    QWidget* valueLabel = nullptr;
    QWidget* childrenContainer = nullptr;

    if (isContainer) {
        int cnt = childCount(node);
        bool hasChildren = (cnt > 0);

        QPushButton* toggleBtn = new QPushButton(hasChildren ? (depth > 0 ? "[+]" : "[-]") : "", headerRow);
        toggleBtn->setFlat(true);
        toggleBtn->setFixedWidth(20);
        toggleBtn->setEnabled(hasChildren);
        rowLayout->addWidget(toggleBtn);

        if (!key.isEmpty()) {
            keyLabel = createLabel("<b>" + escapeHtml(key) + "</b>:", headerRow);
            rowLayout->addWidget(keyLabel);
        }

        valueLabel = createLabel(typeSummary(node), headerRow);
        rowLayout->addWidget(valueLabel);
        rowLayout->addStretch();

        if (hasChildren) {
            childrenContainer = new QWidget(row);
            if (depth > 0) { childrenContainer->hide(); }
            QVBoxLayout* childLayout = new QVBoxLayout(childrenContainer);
            qSetMargins(childLayout, 0, 0, 0, 0);
            childLayout->setSpacing(2);

            cJSON* child = node->child;
            if (t == cJSON_Object) {
                while (child) {
                    QString childKey = child->string ? QString::fromUtf8(child->string) : "";
                    QString childPath = path.isEmpty() ? childKey : path + "." + childKey;
                    bool childHasNext = (child->next != nullptr);
                    std::vector<bool> childAncestors = ancestors;
                    if (depth > 0) childAncestors.push_back(hasNext);
                    QWidget* childRow = buildNode(child, childKey, childPath, depth + 1, childrenContainer, childHasNext, childAncestors);
                    if (childRow) childLayout->addWidget(childRow);
                    child = child->next;
                }
            } else {
                int idx = 0;
                while (child) {
                    QString childKey = "[" + QString::number(idx) + "]";
                    QString childPath = path + childKey;
                    bool childHasNext = (child->next != nullptr);
                    std::vector<bool> childAncestors = ancestors;
                    if (depth > 0) childAncestors.push_back(hasNext);
                    QWidget* childRow = buildNode(child, childKey, childPath, depth + 1, childrenContainer, childHasNext, childAncestors);
                    if (childRow) childLayout->addWidget(childRow);
                    child = child->next;
                    ++idx;
                }
            }

            m_toggleMap[toggleBtn] = childrenContainer;
            connect(toggleBtn, SIGNAL(clicked()), this, SLOT(onToggle()));
        }
    } else {
        // Leaf node
        if (!key.isEmpty()) {
            keyLabel = createLabel("<b>" + escapeHtml(key) + "</b>:", headerRow);
            rowLayout->addWidget(keyLabel);
        }

        valueLabel = createLabel(valueToHtml(node), headerRow);
        rowLayout->addWidget(valueLabel);
        rowLayout->addStretch();
    }

    // Assemble row: header above children
    rowVLayout->addWidget(headerRow);
    if (childrenContainer) {
        rowVLayout->addWidget(childrenContainer);
    }

    if (valueLabel) {
        valueLabel->installEventFilter(this);
        NodeInfo info;
        info.depth = depth;
        info.path = path;
        info.copyText = isContainer ? stripHtml(typeSummary(node)) : valuePlainText(node);
        m_nodeMap[valueLabel] = info;
    }
    if (keyLabel) {
        keyLabel->installEventFilter(this);
        NodeInfo info;
        info.depth = depth;
        info.path = path;
        info.copyText = key;
        m_nodeMap[keyLabel] = info;
    }

    return row;
}

void JsonViewWidget::cleanupTree() {
    // Qt3/Qt4 兼容：逐次删除第一个 widget child，
    // cascade delete 自动清理所有子孙节点，layout 会被保留
#ifdef QT3_BUILD
    // Qt3: children() returns const QObjectList*, but first() is non-const
    QObjectList* cl = const_cast<QObjectList*>(m_treeContainer->children());
    while (cl && !cl->isEmpty()) {
        QObject* child = cl->first();
        if (!child || !child->isWidgetType()) break;
        delete static_cast<QWidget*>(child);
        cl = const_cast<QObjectList*>(m_treeContainer->children());
    }
#else
    const QObjectList& cl = m_treeContainer->children();
    while (!cl.isEmpty()) {
        QObject* child = cl.first();
        if (!child || !child->isWidgetType()) break;
        delete static_cast<QWidget*>(child);
    }
#endif
    // Recreate layout to clear leftover spacer items (stretches that accumulate on repeated setJson calls)
    delete m_treeLayout;
    m_treeLayout = new QVBoxLayout(m_treeContainer);
    qSetMargins(m_treeLayout, 0, 0, 0, 0);
    m_treeLayout->setSpacing(2);

    m_nodeMap.clear();
    m_toggleMap.clear();
    m_maxDepth = 0;
    m_totalNodes = 0;
}

void JsonViewWidget::expandAll() {
    std::map<QPushButton*, QWidget*>::iterator it;
    for (it = m_toggleMap.begin(); it != m_toggleMap.end(); ++it) {
        it->first->setText("[-]");
        it->second->show();
    }
    updateGeometry();
}

void JsonViewWidget::collapseAll() {
    std::map<QPushButton*, QWidget*>::iterator it;
    for (it = m_toggleMap.begin(); it != m_toggleMap.end(); ++it) {
        it->first->setText("[+]");
        it->second->hide();
    }
    updateGeometry();
}

void JsonViewWidget::onToggle() {
    QPushButton* btn = (QPushButton*)sender();
    if (!btn) return;
    std::map<QPushButton*, QWidget*>::iterator it = m_toggleMap.find(btn);
    if (it == m_toggleMap.end()) return;

    QWidget* container = it->second;
    bool visible = !container->isVisible();
#ifdef QT3_BUILD
    if (visible) container->show(); else container->hide();
#else
    container->setVisible(visible);
#endif
    btn->setText(visible ? "[-]" : "[+]");
    updateGeometry();
}

void JsonViewWidget::onToggleRaw() {
    if (m_rawSource.isEmpty()) return;

    m_showRaw = !m_showRaw;

    if (m_showRaw) {
        m_treeContainer->hide();
        m_errorLabel->hide();
#ifdef QT3_BUILD
        m_rawEdit->setText(m_rawSource);
#else
        m_rawEdit->setPlainText(m_rawSource);
#endif
        m_rawEdit->show();
        m_btnRaw->setText("[T]");
    } else {
        m_rawEdit->hide();
        m_treeContainer->show();
        m_btnRaw->setText("[{}]");
    }
}

void JsonViewWidget::onPaste() {
    QDialog dlg(this
#ifdef QT3_BUILD
        , "pasteDlg", true
#endif
    );
#ifdef QT3_BUILD
    dlg.setCaption("Paste JSON");
#else
    dlg.setWindowTitle("Paste JSON");
#endif
    dlg.resize(500, 350);

    QVBoxLayout* lay = new QVBoxLayout(&dlg);
    QTextEdit* edit = new QTextEdit(&dlg);
#ifdef QT3_BUILD
    edit->setText(QApplication::clipboard()->text());
#else
    edit->setPlainText(QApplication::clipboard()->text());
#endif
    lay->addWidget(edit, 1);

    QHBoxLayout* btnLay = new QHBoxLayout();
    btnLay->addStretch();
    QPushButton* okBtn = new QPushButton("OK", &dlg);
    QPushButton* cancelBtn = new QPushButton("Cancel", &dlg);
    btnLay->addWidget(okBtn);
    btnLay->addWidget(cancelBtn);
    lay->addLayout(btnLay);

    QObject::connect(okBtn, SIGNAL(clicked()), &dlg, SLOT(accept()));
    QObject::connect(cancelBtn, SIGNAL(clicked()), &dlg, SLOT(reject()));

    if (dlg.exec() == QDialog::Accepted) {
#ifdef QT3_BUILD
        setJson(edit->text());
#else
        setJson(edit->toPlainText());
#endif
    }
}

bool JsonViewWidget::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::Enter) {
        QWidget* w = (QWidget*)obj;
        m_hoveredLabel = w;
#ifdef QT3_BUILD
        w->setPaletteBackgroundColor(QColor(230, 240, 255));
#else
        QPalette pal = w->palette();
        pal.setColor(w->backgroundRole(), QColor(230, 240, 255));
        w->setPalette(pal);
#endif
        std::map<QWidget*, NodeInfo>::iterator it = m_nodeMap.find(w);
        if (it != m_nodeMap.end()) {
            updateStatusBar(it->second.depth, it->second.path);
        }
    } else if (event->type() == QEvent::Leave) {
        QWidget* w = (QWidget*)obj;
#ifdef QT3_BUILD
        w->unsetPalette();
#else
        w->setPalette(QPalette());
#endif
        clearStatusBar();
    }
    return QWidget::eventFilter(obj, event);
}

void JsonViewWidget::contextMenuEvent(QContextMenuEvent* event) {
    if (!m_hoveredLabel) {
        QWidget::contextMenuEvent(event);
        return;
    }
    std::map<QWidget*, NodeInfo>::iterator it = m_nodeMap.find(m_hoveredLabel);
    if (it == m_nodeMap.end()) {
        QWidget::contextMenuEvent(event);
        return;
    }

#ifdef QT3_BUILD
    QPopupMenu menu(this);
    int copyValueId = menu.insertItem("Copy value");
    int copyPathId = menu.insertItem("Copy path");
    int triggered = menu.exec(event->globalPos());
    if (triggered == copyValueId) {
        QApplication::clipboard()->setText(it->second.copyText);
    } else if (triggered == copyPathId) {
        QApplication::clipboard()->setText(it->second.path);
    }
#else
    QMenu menu(this);
    QAction* copyValue = menu.addAction("Copy value");
    QAction* copyPath = menu.addAction("Copy path");
    QAction* triggered = menu.exec(event->globalPos());
    if (triggered == copyValue) {
        QApplication::clipboard()->setText(it->second.copyText);
    } else if (triggered == copyPath) {
        QApplication::clipboard()->setText(it->second.path);
    }
#endif
}

void JsonViewWidget::keyPressEvent(QKeyEvent* event) {
#ifdef QT3_BUILD
    if (event->key() == Qt::Key_C && (event->state() & Qt::ControlButton)) {
#else
    if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier)) {
#endif
        if (m_hoveredLabel) {
            std::map<QWidget*, NodeInfo>::iterator it = m_nodeMap.find(m_hoveredLabel);
            if (it != m_nodeMap.end() && !it->second.copyText.isEmpty()) {
                QApplication::clipboard()->setText(it->second.copyText);
                return;
            }
        }
    }
    QWidget::keyPressEvent(event);
}

void JsonViewWidget::updateStatusBar(int depth, const QString& path) {
    m_depthLabel->setText("Depth: " + QString::number(depth) + " / " + QString::number(m_maxDepth));
    m_pathLabel->setText(path);
}

void JsonViewWidget::clearStatusBar() {
    m_depthLabel->setText("Depth: 0 / " + QString::number(m_maxDepth));
    m_pathLabel->setText("");
}

