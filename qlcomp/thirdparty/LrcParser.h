/*
 * moonk5/lrc-parser — 单头文件 LRC 格式解析器 (MIT)
 * https://github.com/moonk5/lrc-parser
 *
 * === 备用实现 ===
 *
 * 当前项目使用 DesktopLyrics::parseLrc() (desktoplyrics.cpp) 解析 LRC。
 * 如果 parseLrc 出现 bug 或遇到兼容性问题，可用此文件替换。
 *
 * === 替换方法 ===
 *
 * 1. 在 desktoplyrics.cpp 顶部添加：
 *      #include "thirdparty/LrcParser.h"
 *
 * 2. 将 parseLrc 函数体替换为：
 *      void DesktopLyrics::parseLrc(const QString& content,
 *                                   std::vector<LrcLine>& out)
 *      {
 *          std::string utf8(content.toUtf8().data());
 *          lrc::parser p("");
 *          if (!p.parseString(utf8)) return;
 *          for (auto& t : p.get().time_tokens) {
 *              LrcLine l;
 *              l.time = static_cast<int>(t.elapsed_time);
 *              l.text = QString::fromUtf8(t.line_lyric.c_str());
 *              l.endTime = -1;
 *              out.push_back(l);
 *          }
 *      }
 *
 * 3. 注意：原库只提供 ParseFile(filename)，解析内存字符串
 *    需在 parser 类中添加 parseString 方法：
 *      bool parseString(const std::string& content) {
 *          // 复用 load() 中 regex 解析逻辑，从 string 而非文件读取
 *      }
 *
 * === 限制 ===
 *
 * 此文件依赖 C++11 <regex>。当前 Qt3 工具链 (g++ 4.4.x) 的
 * <regex> 支持不完整，可能导致编译失败。因此仅作为参考和备用保留，
 * 不直接纳入构建系统。
 */

/* *
 *
 * A simple LRC parser for C++
 *
 * Copyright (C) 2018 moonk5
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef LRC_PARSER_H
#define LRC_PARSER_H

#include <chrono>
#include <fstream>
#include <regex>
#include <string>
#include <vector>

#define LRC_PARSER_MAJOR 1
#define LRC_PARSER_MINOR 1
#define LRC_PARSER_PATCH 0

namespace lrc
{
  const std::string LRC_PARSER_VERSION =
    std::to_string(LRC_PARSER_MAJOR) + "." +
    std::to_string(LRC_PARSER_MINOR) + "." +
    std::to_string(LRC_PARSER_PATCH);

  const std::string REGEX_ID_TAGS =
    "\\[(ar|al|ti|au|length|by|offset|re|ve):(.*)\\]";
  const std::string REGEX_TIME_TAG =
    "\\[[0-5][0-9]:[0-5][0-9].[0-9][0-9]\\]";

  namespace time_conversion
  {
    std::string to_simple_string(unsigned int time_in_ms) {
      const char time_fmt[] = "%02d:%02d.%02d";
      std::vector<char> buff(sizeof(time_fmt));

      if (time_in_ms <= 0)
        return "";

      unsigned int minutes = 0;
      unsigned int seconds = 0;
      unsigned int milliseconds = time_in_ms % 1000;
      time_in_ms -= milliseconds;
      minutes = time_in_ms / (60 * 1000);
      time_in_ms -= minutes * (60 * 1000);
      seconds = time_in_ms / 1000;

      std::snprintf(&buff[0], buff.size(), time_fmt,
          minutes, seconds, milliseconds);

      return &buff[0];
    }

    unsigned int to_milliseconds(const std::string& time_in_str) {
      unsigned int milliseconds = 0;

      std::regex r(lrc::REGEX_TIME_TAG);
      if (!std::regex_match(time_in_str, r))
        return milliseconds;

      milliseconds += stoi(time_in_str.substr(1, 2)) * 60 * 1000;
      milliseconds += stoi(time_in_str.substr(4, 2)) * 1000;
      milliseconds += stoi(time_in_str.substr(7, 2));

      return milliseconds;
    }
  }

  struct time_tag
  {
    unsigned int elapsed_time = 0;
    std::string line_lyric = "";

    std::string to_json(bool pretty=false) {
      std::string str_json = "{\"time\":\""
        + time_conversion::to_simple_string(elapsed_time)
        + "\",\"lyric\":\"" + line_lyric + "\"}";

      return str_json;
    }
  };

  struct collection
  {
    void add_time(const time_tag& time_token) {
      time_tokens.push_back(time_token);
    }

    std::string id_tags_to_json_string() {
      std::string str_json = "";

      if (!ar.empty())
        str_json += "\"ar\":\"" + ar + "\",";
      if (!al.empty())
        str_json += "\"al\":\"" + al + "\",";
      if (!ti.empty())
        str_json += "\"ti\":\"" + ti + "\",";
      if (!au.empty())
        str_json += "\"au\":\"" + au + "\",";
      if (!by.empty())
        str_json += "\"by\":\"" + by + "\",";
      if (!re.empty())
        str_json += "\"re\":\"" + re + "\",";
      if (!ve.empty())
        str_json += "\"ve\":\"" + ve + "\",";
      if (!length.empty())
        str_json += "\"length\":\"" + length + "\",";
      if (!offset.empty())
        str_json += "\"offset\":\"" + offset + "\",";

      return str_json;
    }

    std::string time_tags_to_json_string() {
      std::string str_json = "\"time_tags\": [";

      for (time_tag t : time_tokens)
        str_json += t.to_json() + ",";

      return str_json.replace(str_json.size()-1, 1, 1, ']');
    }

    std::string to_json_string() {

      std::string str_json = "{";
      str_json += id_tags_to_json_string();
      str_json += time_tags_to_json_string();
      str_json += "}";

      return str_json;
    }

    void add_id_tag(const std::string& id, const std::string& value) {
      if (id == "ar")
        ar = value;
      else if (id == "al")
        al = value;
      else if (id == "ti")
        ti = value;
      else if (id == "au")
        au = value;
      else if (id == "by")
        by = value;
      else if (id == "re")
        re = value;
      else if (id == "ve")
        ve = value;
      else if (id == "length")
        length = value;
      else if (id == "offset")
        offset = value;
    }

    std::vector<time_tag> time_tokens;

    std::string ar = "";
    std::string al = "";
    std::string ti = "";
    std::string au = "";
    std::string by = "";
    std::string re = "";
    std::string ve = "";
    std::string offset = "";
    std::string length = "";
  };

  class parser
  {
    public:
      parser(const std::string& filePath) {
        file_path = filePath;
      }

      bool load() {
        if (file_path.empty())
          return false;

        std::ifstream ifs(file_path);
        std::string lrc_content(
            (std::istreambuf_iterator<char>(ifs)),
            (std::istreambuf_iterator<char>()));

        std::regex reg_ex(lrc::REGEX_TIME_TAG);
        std::smatch match;
        time_tag time_token;

        if (!regex_search(lrc_content, match, reg_ex))
          return false;

        if (match.prefix().str().length() > 0)
          parse_id_tags(match.prefix().str());

        time_token.elapsed_time =
          time_conversion::to_milliseconds(match[0]);
        lrc_content = match.suffix().str();

        while (std::regex_search(lrc_content, match, reg_ex)) {
          time_token.line_lyric =
            match.prefix().str().substr(0,
                match.prefix().str().find_last_not_of("\n") + 1);
          lrc_collection.add_time(time_token);

          time_token.elapsed_time =
            time_conversion::to_milliseconds(match[0]);

          lrc_content = match.suffix().str();
        }

        time_token.line_lyric = lrc_content.substr(0,
            lrc_content.find_last_not_of("\n") + 1);
        lrc_collection.add_time(time_token);

        return true;
      }

      collection &get() {
        return lrc_collection;
      }

    private:
      void parse_id_tags(std::string id_tags) {
        std::regex reg_ex(lrc::REGEX_ID_TAGS);
        std::regex_token_iterator<std::string::iterator> rend;

        int submatches[] = {1, 2};
        std::regex_token_iterator<std::string::iterator> itr (id_tags.begin(),
            id_tags.end(), reg_ex, submatches);

        int i = 0;
        std::string id = "";
        while (itr != rend) {
          if (i % 2)
            lrc_collection.add_id_tag(id, *itr++);
          else
            id = *itr++;
          ++i;
        }
      }

      std::string file_path;
      collection lrc_collection;
  };
}

#endif // LRC_PARSER_H
