#include <cstdint>
#include <cstring>
#include "json.h"

namespace oasis {

struct P {
  const char* p;
  const char* end;
  void ws() {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
  }
  bool lit(const char* s) {
    size_t n = strlen(s);
    if ((size_t)(end - p) >= n && memcmp(p, s, n) == 0) {
      p += n;
      return true;
    }
    return false;
  }
  bool value(Json& j);
  bool str(std::string& out) {
    if (p >= end || *p != '"') return false;
    p++;
    while (p < end && *p != '"') {
      if (*p == '\\') {
        p++;
        if (p >= end) return false;
        switch (*p) {
          case 'n': out += '\n'; break;
          case 't': out += '\t'; break;
          case 'r': out += '\r'; break;
          case 'b': out += '\b'; break;
          case 'f': out += '\f'; break;
          case 'u': {
            if (end - p < 5) return false;
            unsigned cp = 0;
            for (int i = 0; i < 4; i++) {
              char c = *++p;
              cp <<= 4;
              cp |= c <= '9' ? c - '0' : (c | 32) - 'a' + 10;
            }
            if (cp < 0x80) out += (char)cp;
            else if (cp < 0x800) {
              out += (char)(0xc0 | cp >> 6);
              out += (char)(0x80 | (cp & 63));
            } else {
              out += (char)(0xe0 | cp >> 12);
              out += (char)(0x80 | ((cp >> 6) & 63));
              out += (char)(0x80 | (cp & 63));
            }
            break;
          }
          default: out += *p;
        }
      } else {
        out += *p;
      }
      p++;
    }
    if (p >= end) return false;
    p++;
    return true;
  }
  bool num(Json& j) {
    const char* s = p;
    if (p < end && *p == '-') p++;
    bool real = false;
    while (p < end && (isdigit(*p) || *p == '.' || *p == 'e' || *p == 'E' ||
                       *p == '+' || *p == '-')) {
      if (*p == '.' || *p == 'e' || *p == 'E') real = true;
      p++;
    }
    if (p == s) return false;
    std::string t(s, p);
    if (real) {
      j.type = Json::Real;
      j.d = strtod(t.c_str(), nullptr);
    } else {
      j.type = Json::Int;
      j.i = strtoll(t.c_str(), nullptr, 10);
    }
    return true;
  }
};

bool P::value(Json& j) {
  ws();
  if (p >= end) return false;
  switch (*p) {
    case '{': {
      p++;
      j.type = Json::Obj;
      ws();
      if (p < end && *p == '}') { p++; return true; }
      while (true) {
        ws();
        std::string k;
        if (!str(k)) return false;
        ws();
        if (p >= end || *p != ':') return false;
        p++;
        if (!value(j.o[k])) return false;
        ws();
        if (p >= end) return false;
        if (*p == '}') { p++; return true; }
        if (*p != ',') return false;
        p++;
      }
    }
    case '[': {
      p++;
      j.type = Json::Arr;
      ws();
      if (p < end && *p == ']') { p++; return true; }
      while (true) {
        Json e;
        if (!value(e)) return false;
        j.a.push_back(std::move(e));
        ws();
        if (p >= end) return false;
        if (*p == ']') { p++; return true; }
        if (*p != ',') return false;
        p++;
      }
    }
    case '"':
      j.type = Json::Str;
      return str(j.s);
    case 't': j.type = Json::Bool; j.b = true; return lit("true");
    case 'f': j.type = Json::Bool; j.b = false; return lit("false");
    case 'n': return lit("null");
    default: return num(j);
  }
}

bool json_parse(const std::string& text, Json& out) {
  P p{text.data(), text.data() + text.size()};
  if (!p.value(out)) return false;
  p.ws();
  return true;
}

std::string json_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) {
          char tmp[8];
          snprintf(tmp, sizeof(tmp), "\\u%04x", c);
          out += tmp;
        } else {
          out += c;
        }
    }
  }
  return out;
}

}  // namespace oasis
