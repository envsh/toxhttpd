#ifndef PLUGIN_MANAGER_DIALOG_H
#define PLUGIN_MANAGER_DIALOG_H

#include "compat34.h"
#ifdef QT3_BUILD
#include <qdialog.h>
#include <qmap.h>
#else
#include <QDialog>
#include <QMap>
#endif

class QCheckBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

class PluginManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit PluginManagerDialog(QWidget* parent = 0);
    ~PluginManagerDialog();

private slots:
    void onRefresh();
    void onEnableAll();
    void onDisableAll();
    void onToggleEnabled();
    void onOpenPlugin();

private:
    void setupUi();
    void refreshList();
    void addPluginRow(QVBoxLayout* layout, int index, bool isUiApp);

    ScrollArea* m_scrollArea;
    QWidget* m_contentWidget;
    QVBoxLayout* m_contentLayout;
    QPushButton* m_refreshBtn;
    QPushButton* m_enableAllBtn;
    QPushButton* m_disableAllBtn;

    QMap<QCheckBox*, int> m_checkboxToIndex;
    QMap<QPushButton*, int> m_buttonToIndex;
};

#endif
