#ifndef RTYPE_ENGINE_TEXTTEMPLATE_HPP
#define RTYPE_ENGINE_TEXTTEMPLATE_HPP

#include <string>
#include <unordered_map>

namespace rtype::engine {
  using TemplateParams = std::unordered_map<std::string, std::string>;

  inline std::string formatTemplate(const std::string& templ, const TemplateParams& params) {
    std::string result = templ;
    for (const auto& [key, value] : params) {
      const std::string token = "{" + key + "}";
      for (auto pos = result.find(token); pos != std::string::npos; pos = result.find(token, pos + value.size()))
        result.replace(pos, token.size(), value);
    }
    return result;
  }
}

#endif // !RTYPE_ENGINE_TEXTTEMPLATE_HPP
