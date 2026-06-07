module hellomod;

Hello::Hello(QObject *parent) : QObject(parent) {}

void Hello::greet() {
    greeted("world");
}
