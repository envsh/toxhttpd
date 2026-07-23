#ifndef MESSAGE_PAGE_H
#define MESSAGE_PAGE_H

#include "page.h"

class MessageListWidget;
class QskTextLabel;

class MessagePage : public Page
{
    Q_OBJECT
public:
    MessagePage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;

private:
    MessageListWidget* m_messageList = nullptr;
};

#endif
