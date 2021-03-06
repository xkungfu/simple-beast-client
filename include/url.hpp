#ifndef URL_HPP
#define URL_HPP

#include <boost/regex.hpp>
#include <boost/utility/string_view.hpp>

namespace simple_http {
/**
 * @brief The url class a representation of an HTTP url with defaults for port and scheme
 */
class url {
public:
  url() {
  }

  url(boost::string_view url) : m_representation{url.to_string()} {
    parseRepresentation();
  }

  url(boost::string_view host, boost::string_view target,
      boost::string_view scheme = boost::string_view(),
      boost::string_view port = boost::string_view(),
      boost::string_view username = boost::string_view(),
      boost::string_view password = boost::string_view()) {
    size_t host_off{};
    size_t port_off{};
    size_t username_off{};
    size_t password_off{};
    size_t target_off{};
    if (!scheme.empty()) {
      m_representation = scheme.to_string();
      m_representation += "://";
    }
    if (!username.empty() && !password.empty()) {
      username_off = m_representation.length();
      m_representation += username.to_string() + ':';
      password_off = m_representation.length();
      m_representation += password.to_string();
      m_representation += '@';
    }
    host_off = m_representation.length();
    m_representation += host.to_string();
    if (!port.empty()) {
      m_representation += ':';
      port_off = m_representation.length();
      m_representation += port.to_string();
    }
    target_off = m_representation.length();
    m_representation += target.to_string();
    m_host = boost::string_view{m_representation.data() + host_off, host.length()};
    m_target = boost::string_view{m_representation.data() + target_off, target.length()};
    if (!scheme.empty()) {
      m_scheme = boost::string_view{m_representation.data(), scheme.length()};
    }
    if (!username.empty() && !password.empty()) {
      m_username = boost::string_view{m_representation.data() + username_off, username.length()};
      m_password = boost::string_view{m_representation.data() + password_off, password.length()};
    }
    if (!port.empty()) {
      m_port = boost::string_view{m_representation.data() + port_off, port.length()};
    }
  }

  url(const url &other) : m_representation{other.m_representation} {
    // TODO: find better solution.
    parseRepresentation();
  }

  url &operator=(boost::string_view url) {
    m_representation = url.to_string();
    parseRepresentation();
    return *this;
  }

  url &operator=(const url &other) {
    m_representation = other.m_representation;
    parseRepresentation();
    return *this;
  }

  boost::string_view scheme() const {
    if (!m_scheme.empty()) {
      return m_scheme;
    }
    return "http";
  }
  boost::string_view host() const {
    return m_host;
  }
  boost::string_view port() const {
    if (!m_port.empty()) {
      return m_port;
    }
    return m_scheme == "https" ? "443" : "80";
  }
  boost::string_view username() const {
    return m_username;
  }
  boost::string_view password() const {
    return m_password;
  }
  boost::string_view target() const {
    if (!m_target.empty()) {
      return m_target;
    }
    return "/";
  }
  boost::string_view query() const {
    return m_query;
  }

  bool hasAuthentication() const {
    return !m_username.empty() && !m_password.empty();
  }

  void setUsername(boost::string_view username) {
    m_username = username;
  }
  void setPassword(boost::string_view password) {
    m_password = password;
  }
  void setScheme(boost::string_view scheme) {
    m_scheme = scheme;
  }

private:
  std::string m_representation;
  boost::string_view m_scheme;
  boost::string_view m_host;
  boost::string_view m_port;
  boost::string_view m_username;
  boost::string_view m_password;
  boost::string_view m_target;
  boost::string_view m_query;

  void parseRepresentation() {
    static const size_t SchemeLoc = 2;
    static const size_t UserLoc = 4;
    static const size_t PassLoc = 5;
    static const size_t HostLoc = 6;
    static const size_t PortLoc = 8;
    static const size_t TargetLoc = 9;
    static const size_t QueryLoc = 13;

    static const boost::regex http_reg("^((https?|ftp):\\/\\/)?"                         // scheme
                                       "(([^\\s$.?#].?[^\\s\\/]*):([^\\s$.?#].?[^\\s\\/]*)@)?" // auth
                                       "([^\\s$.?#].[^\\s\\/:]*)"                              // host
                                       "(:([0-9]+))?"                                          // port
                                       "(([^\\s?#]*)?(([\\?#])([^\\s]*))?)?$");                // target
    boost::string_view represent{m_representation};
    boost::string_view::const_iterator start, end;
    start = represent.cbegin();
    end = represent.cend();
    boost::match_results<boost::string_view::const_iterator> matches;
    boost::match_flag_type flags = boost::match_default;
    if (boost::regex_match(start, end, matches, http_reg, flags)) {
      boost::string_view::size_type size = static_cast<boost::string_view::size_type>(
            std::distance(matches[SchemeLoc].first, matches[SchemeLoc].second));
      m_scheme = boost::string_view{matches[SchemeLoc].first, size};
      size = static_cast<boost::string_view::size_type>(
            std::distance(matches[UserLoc].first, matches[UserLoc].second));
      m_username = boost::string_view{matches[UserLoc].first, size};
      size = static_cast<boost::string_view::size_type>(
            std::distance(matches[PassLoc].first, matches[PassLoc].second));
      m_password = boost::string_view{matches[PassLoc].first, size};
      size = static_cast<boost::string_view::size_type>(
            std::distance(matches[HostLoc].first, matches[HostLoc].second));
      m_host = boost::string_view{matches[HostLoc].first, size};
      size = static_cast<boost::string_view::size_type>(
            std::distance(matches[PortLoc].first, matches[PortLoc].second));
      m_port = boost::string_view{matches[PortLoc].first, size};
      size = static_cast<boost::string_view::size_type>(
            std::distance(matches[TargetLoc].first, matches[TargetLoc].second));
      m_target = boost::string_view{matches[TargetLoc].first, size};
      size = static_cast<boost::string_view::size_type>(
            std::distance(matches[QueryLoc].first, matches[QueryLoc].second));
      m_query = boost::string_view{matches[QueryLoc].first, size};
    }
  }
};

} // namespace simple_http

#endif // URL_HPP
