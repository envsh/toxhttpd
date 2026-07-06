#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include "page.h"
#include <QskComboBox.h>
#include <QskSwitchButton.h>
#include <memory>
#include <functional>

struct FontSizes {
    int body = 21, title = 29, caption = 19, global = 16;
};

class SettingsPage : public Page
{
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

    // Shared state accessible from main.cpp
    static std::shared_ptr<FontSizes> sharedFontSizes;
    static std::function<void()>     applyAndroidFonts;

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;

private:
    bool m_signalsConnected = false;
    int m_currentAnimatorIdx = 3;   // 当前 animator 类型索引，避免重复分配
    QskComboBox* m_transitionCombo = nullptr;
    QskComboBox* m_skinCombo = nullptr;
    QskSwitchButton* m_darkSwitch = nullptr;
    QskComboBox* m_fontScaleCombo = nullptr;
};

#endif
