#pragma once
#include <map>
#include <string>
#include <vector>

namespace oasis {

struct Json {
  enum Type { Null, Bool, Int, Real, Str, Arr, Obj } type = Null;
  bool b = false;
  int64_t i = 0;
  double d = 0;
  std::string s;
  std::vector<Json> a;
  std::map<std::string, Json> o;

  const Json* get(const std::string& key) const {
    if (type != Obj) return nullptr;
    auto it = o.find(key);
    return it == o.end() ? nullptr : &it->second;
  }
  std::string str(const std::string& key, const std::string& def = "") const {
    const Json* v = get(key);
    return v && v->type == Str ? v->s : def;
  }
  int64_t integer(const std::string& key, int64_t def = 0) const {
    const Json* v = get(key);
    return v && v->type == Int ? v->i : def;
  }
};

bool json_parse(const std::string& text, Json& out);
std::string json_escape(const std::string& s);

}  // namespace oasis
