// hjson-cpp 2.6
#include "hjson_wrap.h"
#include <cstdio>

Result<Hjson::Value, std::string> hjsonLoad(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) {
        return Err(std::string("cannot open: ") + path);
    }
    fclose(f);
    try {
        Hjson::DecoderOptions opt;
        opt.comments = false;
        return Ok(Hjson::UnmarshalFromFile(path, opt));
    } catch (const std::exception& e) {
        return Err(std::string("parse error: ") + e.what());
    }
}

Result<void, std::string> hjsonSave(const Hjson::Value& root, const std::string& path) {
    Hjson::EncoderOptions opt;
    opt.bracesSameLine = true;
    opt.quoteAlways = true;
    opt.quoteKeys = true;
    opt.separator = true;
    opt.comments = false;
    opt.indentBy = "    ";
    std::string json;
    try {
        json = Hjson::Marshal(root, opt);
    } catch (const std::exception& e) {
        return Err(std::string("marshal error: ") + e.what());
    }
    FILE* f = fopen(path.c_str(), "w");
    if (!f) {
        return Err(std::string("cannot write: ") + path);
    }
    fwrite(json.data(), 1, json.size(), f);
    fclose(f);
    return Ok();
}
