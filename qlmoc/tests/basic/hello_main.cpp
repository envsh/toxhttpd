#ifdef MODULE_BUILD
import hellomod;
#else
#include <qobject.h>
#include <qapplication.h>
#include "hello.h"
#endif

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
#ifdef MODULE_BUILD
    Hello h;
    h.greet();
    return 0;
#else
    QApplication app(argc, argv);
    Hello h;
    h.greet();
    return 0;
#endif
}
