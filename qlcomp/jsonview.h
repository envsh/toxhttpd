#ifndef JSONVIEW_H
#define JSONVIEW_H

#include "compat34.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qtextedit.h>
#include <qlineedit.h>
#include <qapplication.h>
#include <qlayout.h>
#ifdef QT3_BUILD
#include <qscrollview.h>
#include <qpopupmenu.h>
#include <qaction.h>
#else
#include <QScrollArea>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#endif
#include <map>
#include <vector>

struct cJSON;

class JsonViewWidget : public QWidget {
    Q_OBJECT
public:
    JsonViewWidget(QWidget* parent = nullptr);
    ~JsonViewWidget();

    void setJson(const QString& jsonStr);
    void setJson(cJSON* root);
    void setPlainText(const QString& text);
    void clear();

public slots:
    void expandAll();
    void collapseAll();

private slots:
    void onPaste();
    void onToggle();
    void onToggleRaw();

protected:
    bool eventFilter(QObject* obj, QEvent* event);
    void keyPressEvent(QKeyEvent* event);
    void contextMenuEvent(QContextMenuEvent* event);

private:
    struct NodeInfo {
        int depth;
        QString path;
        QString copyText;
    };

    void rebuildTree(cJSON* root);
    QWidget* buildNode(cJSON* node, const QString& key, const QString& path, int depth, QWidget* parent, bool hasNext, const std::vector<bool>& ancestors);
    QString valueToHtml(cJSON* node);
    QString escapeHtml(const QString& s);
    QString typeSummary(cJSON* node);
    int childCount(cJSON* node);
    void cleanupTree();
    QWidget* createLabel(const QString& html, QWidget* parent);
    void updateStatusBar(int depth, const QString& path);
    void clearStatusBar();

    QVBoxLayout* m_mainLayout;
    QWidget* m_toolbar;
    QPushButton* m_btnPaste;
    QPushButton* m_btnExpand;
    QPushButton* m_btnCollapse;
    QWidget* m_contentArea;
    QVBoxLayout* m_contentLayout;
    QWidget* m_statusBar;
    QLabel* m_depthLabel;
    QLabel* m_pathLabel;
    QLabel* m_errorLabel;

    QWidget* m_hoveredLabel;
    std::map<QWidget*, NodeInfo> m_nodeMap;
    std::map<QPushButton*, QWidget*> m_toggleMap;

    QString m_rawSource;
    QTextEdit* m_rawEdit;
    QPushButton* m_btnRaw;
    bool m_showRaw;
    QWidget* m_treeScroll;
    QWidget* m_treeContainer;
    QVBoxLayout* m_treeLayout;

    int m_maxDepth;
    int m_totalNodes;

    static const int MAX_INPUT_SIZE = 16384;
};

#endif
