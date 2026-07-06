#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include <QskControl.h>

class QskComboBox;
class QskSwitchButton;

class SettingsPage : public QskControl
{
    Q_OBJECT
public:
    SettingsPage(QQuickItem* parent = nullptr);

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
