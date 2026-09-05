#ifndef STATISTICSDIALOG_H
#define STATISTICSDIALOG_H

#include "compat34.h"
#include <qdialog.h>
#include <vector>

class QPushButton;
class QLabel;

class StatisticsDialog : public QDialog {
    Q_OBJECT
public:
    explicit StatisticsDialog(QWidget* parent = nullptr);

private slots:
    void refreshStats();
    void openDataDir();
    void copyStats();
    void onTabClicked();

private:
    void buildOverviewPage(QWidget* inner);
    void buildResultGrid(QWidget* host);
    QString dataDirText() const;

    StackedWidget* m_pageStack = nullptr;
    QWidget* m_tabBar = nullptr;
    std::vector<QPushButton*> m_tabButtons;
    std::vector<QWidget*> m_pages;
    QLabel* m_dirLabel = nullptr;
    QWidget* m_resultBox = nullptr;
    std::vector<QLabel*> m_valueLabels;
    QString m_resultText;
};

#endif