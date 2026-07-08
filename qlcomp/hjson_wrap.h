#ifndef HJSON_WRAP_H
#define HJSON_WRAP_H

#include "result.h"
#include <hjson/hjson.h>
#include <string>

Result<Hjson::Value, std::string> hjsonLoad(const std::string& path);

Result<void, std::string> hjsonSave(const Hjson::Value& root, const std::string& path);

#endif
