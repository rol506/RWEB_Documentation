#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace rweb
{ 

  class HTMLTemplate
  {
  public:
    HTMLTemplate(const std::string& html);
    const std::string& getHTML() const;
    const std::string& getFileName() const;
    const std::string& getStatusResponce() const;
    const std::string& getEncoding() const;
    const std::string& getContentType() const;

    //Renderes a template with specified json
    void renderJSON(const nlohmann::json& json);
  private:
    std::string m_html;
    std::string m_templateFileName;
    std::string m_responce;
    std::string m_encoding;
    std::string m_contentType;

    friend HTMLTemplate createTemplate(const std::string&, const std::string&);
  };
}
