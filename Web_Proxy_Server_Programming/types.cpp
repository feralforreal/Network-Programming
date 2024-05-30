#include "types.h"

/**
  @brief Write a ProxyURI to an output stream

  @param[out]  os   Output stream
  @param[in]   uri  URI to write

  @return std::ostream& Modified output stream
**/
std::ostream &operator<<(std::ostream &os, const ProxyURI &uri) {
  os << uri.absolute();
  return os;
};

/**
  @brief Determine the content type of a file based on its extension

  @param[in]  extension The extension of the file

  @return std::string The content type of the file
**/
std::string extension_to_content_type(std::string extension) {
  if (extension == "html") {
    return std::string("text/html");
  } else if (extension == "txt") {
    return std::string("text/plain");
  } else if (extension == "png") {
    return std::string("image/png");
  } else if (extension == "gif") {
    return std::string("image/gif");
  } else if (extension == "jpg") {
    return std::string("image/jpg");
  } else if (extension == "css") {
    return std::string("text/css");
  } else if (extension == "js") {
    return std::string("text/javascript");
  } else if (extension == "ico") {
    return std::string("image/x-icon");
  } else {
    return std::string("text/plain");
  }
}

/**
  @brief Check if a given content type is ascii

  @param[in]  content_type  String containing content type

  @return bool True if content type is ascii, false otherwise
**/
bool is_text(const std::string &content_type) { return lower(content_type).find("text") != std::string::npos; }

/**
  @brief Convert a ResponseCode enum to a string

  @param[in]  code  Input response code

  @return  std::string  The response code as a string with message
**/
std::string to_string(ResponseCode code) {
  switch (code) {
    case ResponseCode::OK:
      return std::string("OK");
    case ResponseCode::BadRequest:
      return std::string("Bad Request");
    case ResponseCode::Forbidden:
      return std::string("Forbidden");
    case ResponseCode::NotFound:
      return std::string("Not Found");
    case ResponseCode::InternalServerError:
      return std::string("Internal Server Error");
    case ResponseCode::GatewayTimeout:
      return std::string("Gateway Timeout");
  }
}

/**
  @brief Convert a string to a ConnectionType enum

  @param[in]  str  Input string

  @return  ConnectionType  The connection type
**/
ConnectionType from_string(const std::string &str) {
  std::string str_lower = lower(strip(str, " \r\n"));
  if (str_lower == "keep-alive") {
    return ConnectionType::KeepAlive;
  } else if (str_lower == "close") {
    return ConnectionType::Close;
  } else {
    return ConnectionType::Close;
  }
}

/**
  @brief Convert a ConnectionType enum to a string

  @param[in]  connection  Input connection type

  @return  std::string  The connection type as a string
**/
std::string to_string(ConnectionType connection) {
  switch (connection) {
    case ConnectionType::KeepAlive:
      return std::string("keep-alive");
    case ConnectionType::Close:
      return std::string("close");
  }
}

/**
  @brief Read a ConnectionType from an input stream

  @param[inout]  is    Input stream
  @param[out]    type  Read connection type

  @return std::istream& Input stream
**/
std::istream &operator>>(std::istream &is, ConnectionType &type) {
  std::string connection;

  is >> connection;
  type = from_string(connection);

  return is;
}

/**
  @brief Write a ConnectionType to an output stream

  @param[inout]  os    Output stream
  @param[in]     type  Connection type to write

  @return std::ostream& Output stream
**/
std::ostream &operator<<(std::ostream &os, ConnectionType type) {
  switch (type) {
    case ConnectionType::KeepAlive:
      os << "keep-alive";
      break;
    case ConnectionType::Close:
      os << "close";
      break;
  }

  return os;
}

/**
  @brief Read a RequestMethod from an input stream

  @param[inout]  is      Input stream
  @param[out]    method  Read request method

  @return std::istream& Input stream
**/
std::istream &operator>>(std::istream &is, RequestMethod &method) {
  std::string str_method;

  is >> str_method;
  if (str_method == "GET") {
    method = RequestMethod::GET;
  } else if (str_method == "HEAD") {
    method = RequestMethod::HEAD;
  } else if (str_method == "POST") {
    method = RequestMethod::POST;
  } else if (str_method == "CONNECT") {
    method = RequestMethod::CONNECT;
  } else {
    method = RequestMethod::UNKNOWN;
  }

  return is;
}

/**
  @brief Write a RequestMethod to an output stream

  @param[inout]  os      Output stream
  @param[in]     method  Request method to write

  @return std::ostream& Output stream
**/
std::ostream &operator<<(std::ostream &os, RequestMethod method) {
  switch (method) {
    case RequestMethod::GET:
      os << "GET";
      break;
    case RequestMethod::HEAD:
      os << "HEAD";
      break;
    case RequestMethod::POST:
      os << "POST";
      break;
    case RequestMethod::CONNECT:
      os << "CONNECT";
      break;
    case RequestMethod::UNKNOWN:
      os << "UNKNOWN";
      break;
  }

  return os;
}

/**
  @brief Write repeated characters to an output stream

  @param[out]  os      Output stream
  @param[in]   repeat  Repeat object containing char and number of times to repeat

  @return std::ostream& Output stream
**/
std::ostream &operator<<(std::ostream &os, Repeat repeat) {
  std::fill_n(std::ostreambuf_iterator<char>(os), repeat.n, repeat.c);
  return os;
}

/**
  @brief Parse URI and host info from an absolute URI

  @param[in]  absolute_uri  Absolute URI to parse

  @return ProxyURI Constructed ProxyURI struct
**/
ProxyURI parse_uri(const std::string &absolute_uri, const ProxyURI &base) {
  std::string url;
  ProxyURI    uri_info;

  log("Parsing URI %s", absolute_uri.c_str());

  if (absolute_uri.empty()) {
    uri_info.uri = "/";
    return uri_info;
  }

  // Split section, if present
  size_t section_idx = absolute_uri.find("#");
  if (section_idx != std::string::npos) {
    // uri_info.section = url.substr(section_idx);
    url = absolute_uri.substr(0, section_idx);
  } else {
    url = absolute_uri;
  }

  // Split the URI into scheme, host, port, and path
  size_t split_idx = url.find("://");
  if (split_idx != std::string::npos) {
    // Absolute URI
    url = url.substr(split_idx + 3);

    // Find first slash
    split_idx = url.find("/");
    if (split_idx == std::string::npos) {
      uri_info.uri = "/";
    } else {
      uri_info.uri = url.substr(split_idx);
      url          = url.substr(0, split_idx);
    }

    // Split host and port
    split_idx = url.find(":");
    if (split_idx == std::string::npos) {
      uri_info.host = url;
      uri_info.port = "80";
    } else {
      uri_info.host = url.substr(0, split_idx);
      uri_info.port = url.substr(split_idx + 1);
    }
  } else {
    // Relative URI

    // Find first slash
    if (uri_info.uri[0] != '/') {
      // Try to parse the URI as a relative URI
      split_idx = base.uri.find_last_of("/");
      if (split_idx != std::string::npos) {
        uri_info.uri = base.uri.substr(0, split_idx + 1) + url;
      } else {
        uri_info.uri = "/" + url;
      }
    } else {
      uri_info.uri = url;
    }
    uri_info.host = base.host;
    uri_info.port = base.port;
    uri_info.ip   = base.ip;
    log("Parsed relative URI %s", uri_info.absolute().c_str());
  }

  return uri_info;
}
