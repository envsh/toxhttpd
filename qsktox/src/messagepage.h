#ifndef MESSAGE_PAGE_H
#define MESSAGE_PAGE_H

#include "page.h"

class MessageListWidget;
class QskTextLabel;
class QskTextField;

class MessagePage : public Page
{
    Q_OBJECT
public:
    MessagePage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;

private:
    void sendMessage();

    MessageListWidget* m_messageList = nullptr;
    QskTextField* m_input = nullptr;
};

#endif
