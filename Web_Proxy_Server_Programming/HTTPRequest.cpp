#include "HTTPRequest.h"

/**
  @brief Construct a HTTPRequest from a string

  @param[in]  message  Input string to parse
**/
HTTPRequest::HTTPRequest(const std::string &message) {
  std::stringstream stream(message);
  std::string       line, uri;

  std::getline(stream, line);

  // Split first line
  {
    strip(line, "\r");
    std::stringstream tmp_stream(line);
    tmp_stream >> method;
    tmp_stream >> uri;
    tmp_stream >> version;
  }
  while (std::getline(stream, line) && line != "\r") {
    std::stringstream header_line(line);
    std::string       key, value;
    std::getline(header_line, key, ':');
    std::getline(header_line, value);
    headers[normalize_field_name(key)] = strip(value, " \r");
  }

  // Add default headers
  if (!contains(headers, "Connection")) {
    headers["Connection"] = version == "HTTP/1.1" ? "Keep-Alive" : "Close";
  }
  if (!contains(headers, "Proxy-Connection")) {
    headers["Proxy-Connection"] = "Keep-Alive";
  }

  // Remove bad headers
  headers.erase("Upgrade-Insecure-Requests");

  if (method == RequestMethod::CONNECT) {
    uri = "http://" + uri;
  }
  proxy_uri = parse_uri(uri);
  if (proxy_uri.uri.empty()) {
    proxy_uri.uri = "/";
  }
  if (proxy_uri.port.empty()) {
    proxy_uri.port = "80";
  }
  if (proxy_uri.host.empty()) {
    proxy_uri.host = headers["Host"];
  } else {
    headers["Host"] = proxy_uri.host + ":" + proxy_uri.port;
  }
}

/**
  @brief Write a HTTPRequest to an output stream

  @param[out]  os       Output stream
  @param[in]   request  Request to write

  @return  std::ostream&  Output stream
**/
std::ostream &operator<<(std::ostream &os, HTTPRequest request) {
  int max_line_width = get_terminal_width() - 3;
  os << request.method << " " << request.proxy_uri.uri << " " << request.version << std::endl;

  for (const auto &kv : request.headers) {
    os << kv.first << ": ";
    if (kv.first.length() + kv.second.length() + 2 > (size_t)max_line_width) {
      os << kv.second.substr(0, max_line_width - (kv.first.length() + 2)) << "..." << std::endl;
    } else {
      os << kv.second << std::endl;
    }
  }
  return os;
}

/**
  @brief Dump a HTTPRequest to a string

  @return  std::string  String representation of HTTPRequest
**/
std::string HTTPRequest::dump() const {
  std::stringstream stream;

  stream << method << " " << proxy_uri.uri << " " << version << "\r\n";
  // stream << "Host: " << proxy_uri.host << ":" << (proxy_uri.port.empty() ? "80" : proxy_uri.port) << "\r\n";
  for (const auto &kv : headers) {
    stream << kv.first << ": " << kv.second << "\r\n";
  }
  stream << "\r\n";
  return stream.str();
}
