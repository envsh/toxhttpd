#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#ifdef __cplusplus
extern "C" {
#endif

const char* plugin_name(void);
const char* plugin_version(void);
const char* plugin_description(void);
void* plugin_create(void* parent);
void  plugin_destroy(void* widget);

#ifdef __cplusplus
}
#endif

#endif
