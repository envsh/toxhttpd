#include "hello.h"

Hello::Hello(QObject *parent) : QObject(parent) {}

void Hello::greet() {
    emit greeted("world");
}
