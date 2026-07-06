#include <QGuiApplication>
#include <QskWindow.h>
#include <QskBox.h>
#include <QskTextLabel.h>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    auto* box = new QskBox();
    auto* label = new QskTextLabel("Hello qsktox", box);
    label->setAlignment(Qt::AlignCenter);

    QskWindow window;
    window.addItem(box);
    window.setPreferredSize({400, 300});
    window.show();

    return app.exec();
}
