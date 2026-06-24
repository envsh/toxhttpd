#ifndef CONFIGDIALOG_H
#define CONFIGDIALOG_H

#include "compat34.h"
#ifdef QT3_BUILD
#include <qgroupbox.h>
#include <qspinbox.h>
#include <qsplitter.h>
#include <qlistbox.h>
#include <qwidgetstack.h>
#include <qcombobox.h>
#include <qsettings.h>
#include <qdialog.h>
#include <qvariant.h>
#include <qmap.h>
#else
#include <QGroupBox>
#include <QSpinBox>
#include <QSplitter>
#include <QListWidget>
#include <QStackedWidget>
#include <QComboBox>
#include <QSettings>
#include <QDialog>
#include <QVariant>
#include <QMap>
#endif

class ConfigItemBase {
protected:
    ConfigItemBase() : m_settings(0) {}
    QSettings* m_settings;
public:
    void setSettings(QSettings* s) { m_settings = s; }
    virtual void load() {}
    virtual void save() {}
    virtual QString itemKey() const = 0;
    virtual QVariant widgetValue() const = 0;
    virtual bool isModified() const = 0;
};

class BoolConfigItem : public ConfigItemBase {
public:
    BoolConfigItem(const QString& key, bool defaultValue, const QString& label, QWidget* parent = 0);
    ~BoolConfigItem();
    void load();
    void save();
    QString itemKey() const { return m_key; }
    QVariant widgetValue() const { return QVariant(checkBox()->isChecked()); }
    bool isModified() const { return checkBox()->isChecked() != m_value; }
    QString key() const { return m_key; }
    QString label() const { return m_label; }
    bool value() const { return m_value; }
    QCheckBox* checkBox() const { return m_checkbox; }
    
private:
    QCheckBox* m_checkbox;
    QString    m_key;
    QString    m_label;
    bool       m_default;
    bool       m_value;
    QWidget*   m_parent;
};

class StringConfigItem : public ConfigItemBase {
public:
    StringConfigItem(const QString& key, const QString& defaultValue, const QString& label, QWidget* parent = 0);
    ~StringConfigItem();
    void load();
    void save();
    QString itemKey() const { return m_key; }
    QVariant widgetValue() const { return QVariant(lineEdit()->text()); }
    bool isModified() const { return lineEdit()->text() != m_value; }
    QString key() const { return m_key; }
    QString label() const { return m_label; }
    QString value() const { return m_value; }
    QLineEdit* lineEdit() const { return m_lineEdit; }
    
private:
    QLineEdit* m_lineEdit;
    QString    m_key;
    QString    m_label;
    QString    m_default;
    QString    m_value;
    QWidget*   m_parent;
};

class IntConfigItem : public ConfigItemBase {
public:
    IntConfigItem(const QString& key, int defaultValue, const QString& label, 
                  int min = 0, int max = 999999, QWidget* parent = 0);
    ~IntConfigItem();
    void load();
    void save();
    QString itemKey() const { return m_key; }
    QVariant widgetValue() const { return QVariant(spinBox()->value()); }
    bool isModified() const { return spinBox()->value() != m_value; }
    QString key() const { return m_key; }
    QString label() const { return m_label; }
    int value() const { return m_value; }
    QSpinBox* spinBox() const { return m_spinBox; }
    
private:
    QSpinBox* m_spinBox;
    QString   m_key;
    QString   m_label;
    int       m_default;
    int       m_min;
    int       m_max;
    int       m_value;
    QWidget*   m_parent;
};

class SelectConfigItem : public ConfigItemBase {
public:
    SelectConfigItem(const QString& key, const QString& defaultValue, const QString& label,
                     const QStringList& options, QWidget* parent = 0);
    ~SelectConfigItem();
    void load();
    void save();
    QString itemKey() const { return m_key; }
    QVariant widgetValue() const { return QVariant(comboBox()->currentText()); }
    bool isModified() const { return comboBox()->currentText() != m_value; }
    QString key() const { return m_key; }
    QString label() const { return m_label; }
    QString value();
    QComboBox* comboBox() const { return m_comboBox; }
    
private:
    QComboBox*  m_comboBox;
    QString    m_key;
    QString    m_label;
    QString    m_default;
    QString    m_value;
    QStringList m_options;
    QWidget*   m_parent;
};

typedef QMap<QString, QVariant> SettingsChangedMap;

class ConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConfigDialog(const QString& title, QWidget* parent = 0);
    ~ConfigDialog();
    
    void addCategory(const QString& name, QWidget* page);
    int categoryCount() const;
    int currentCategory() const;
    void setCurrentCategory(int index);
    QString categoryName(int index) const;
    
    void loadSettings();
    void saveSettings();
    void setSettingsFile(const QString& filePath);
    void registerConfigItem(ConfigItemBase* item);
    
    static QString defaultSettingsFile();
    
protected:
    void resizeEvent(QResizeEvent* event);
    
private slots:
    void onCategoryChanged(int index);
    void onAccepted();
    void onApply();
    
signals:
    void settingsSaved(const SettingsChangedMap& changed);
    
private:
    void setupUi();
    QWidget* createButtonBox();
    
#ifdef QT3_BUILD
    QListBox*      m_categoryList;
    QWidgetStack*  m_pageStack;
    QValueList<int> m_pageIds;
#else
    QListWidget*     m_categoryList;
    QStackedWidget* m_pageStack;
#endif
    QSettings*      m_settings;
    QString        m_settingsFile;
    QPtrList<ConfigItemBase> m_configItems;
};

class ConfigGroupBox;

class CategoryPage : public QWidget {
    Q_OBJECT
public:
    explicit CategoryPage(const QString& title, QWidget* parent = 0);
    ~CategoryPage();
    
    void addWidget(QWidget* widget, int stretch = 0);
    void addLayout(QLayout* layout);
    void addSpacing(int size);
    void addStretch(int stretch = 0);
    void addLabeledControl(const QString& label, QWidget* control);
    
private:
    QVBoxLayout* m_mainLayout;
    ConfigGroupBox* m_groupBox;
};

class ConfigGroupBox : public QGroupBox {
    Q_OBJECT
public:
    explicit ConfigGroupBox(const QString& title, QWidget* parent = 0);
    ~ConfigGroupBox();
    
    void addWidget(QWidget* widget, int stretch = 0);
    void addLayout(QLayout* layout);
    void addSpacing(int size);
    void addStretch(int stretch = 0);
    void addLabeledControl(const QString& label, QWidget* control);
    
private:
    QGridLayout* m_grid;
};

#endif
