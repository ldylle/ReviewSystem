#ifndef JSON_FWD_H
#define JSON_FWD_H

// 使用nlohmann/json简化声明
// 在实际编译时需要安装: sudo apt-get install nlohmann-json3-dev
// 或下载单头文件: https://github.com/nlohmann/json/releases

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#endif // JSON_FWD_H
