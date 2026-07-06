#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include <QskControl.h>
#include <QskComboBox.h>
#include <QskSwitchButton.h>

class SettingsPage : public QskControl
{
    Q_OBJECT
public:
    SettingsPage(QQuickItem* parent = nullptr);

    int transitionIndex() const { return m_transitionCombo->currentIndex(); }
    int skinIndex() const { return m_skinCombo->currentIndex(); }
    bool isDarkMode() const { return m_darkSwitch->isChecked(); }
    int fontScaleIndex() const { return m_fontScaleCombo->currentIndex(); }

    QskComboBox* transitionCombo() const { return m_transitionCombo; }
    QskComboBox* skinCombo() const { return m_skinCombo; }
    QskSwitchButton* darkModeSwitch() const { return m_darkSwitch; }
    QskComboBox* fontScaleCombo() const { return m_fontScaleCombo; }

Q_SIGNALS:
    void backRequested();

private:
    QskComboBox* m_transitionCombo = nullptr;
    QskComboBox* m_skinCombo = nullptr;
    QskSwitchButton* m_darkSwitch = nullptr;
    QskComboBox* m_fontScaleCombo = nullptr;
};

#endif
