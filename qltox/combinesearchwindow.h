#ifndef COMBINESEARCHWINDOW_H
#define COMBINESEARCHWINDOW_H

#include "compat34.h"
#include <qdialog.h>
#include <string>
#include <vector>

class QLineEdit;
class QPushButton;
class QLabel;

class CombineSearch : public QDialog {
    Q_OBJECT
public:
    explicit CombineSearch(QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* e);

private slots:
    void runSearch();
    void onTabClicked();
    void goFirst();
    void goPrev();
    void goNext();
    void goLast();

private:
    struct ContactHit {
        int id;
        std::string type;
        QString name;
        QString typeLabel;
    };
    struct MessageHit {
        int id;
        std::string type;
        QString chanName;
        QString sender;
        QString body;
        QString time;
    };
    struct RowInfo {
        QString title;
        QString detail;
    };

    QWidget* makeRow(const QString& title, const QString& detail, QWidget* host);
    void clearLayout(QLayout* lay);
    int activeTab() const;
    int rowTotal(int tab) const;
    void renderSlice(QWidget* inner, const std::vector<RowInfo>& rows, int page,
                     const QString& emptyText);
    void showPage(int tab, int page);

    QLineEdit* m_input = nullptr;
    QPushButton* m_searchBtn = nullptr;
    QPushButton* m_tabContacts = nullptr;
    QPushButton* m_tabMessages = nullptr;
    QPushButton* m_firstBtn = nullptr;
    QPushButton* m_prevBtn = nullptr;
    QPushButton* m_nextBtn = nullptr;
    QPushButton* m_lastBtn = nullptr;
    QLabel* m_pageLabel = nullptr;
    StackedWidget* m_stack = nullptr;
    QWidget* m_contactsInner = nullptr;
    QWidget* m_messagesInner = nullptr;
    std::vector<QWidget*> m_pages;
    std::vector<QPushButton*> m_tabButtons;
    std::vector<ContactHit> m_contacts;
    std::vector<MessageHit> m_messages;
    std::vector<RowInfo> m_contactRows;
    std::vector<RowInfo> m_messageRows;
    int m_curPage[2] = {0, 0};
};

#endif