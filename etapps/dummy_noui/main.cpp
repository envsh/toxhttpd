#include "noui_api.h"
#include <stdio.h>

extern "C" const char* noui_name(void) {
    return "Dummy Noui";
}

extern "C" const char* noui_version(void) {
    return "1.0.0";
}

extern "C" const char* noui_description(void) {
    return "Noui 最小模板示例";
}

extern "C" int noui_init(void) {
    printf("[dummy_noui] initialized\n");
    return 0;
}

extern "C" void noui_uninit(void) {
    printf("[dummy_noui] uninitialized\n");
}
