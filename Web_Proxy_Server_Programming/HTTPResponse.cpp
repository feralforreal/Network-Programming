#include "HTTPResponse.h"

/**
  @brief Write a HTTPResponse to a string

  @return std::string  HTTPResponse as a string
**/
HTTPResponse::HTTPResponse(const std::string headers, const ProxyURI& proxy_uri) : proxy_uri_(proxy_uri) {
  std::stringstream response(headers);
  std::string       line;
  std::getline(response, line);
  std::stringstream status_line(line);
  std::string       version, code;
  status_line >> version >> code;
  version_ = version;
  code_    = static_cast<ResponseCode>(std::stoi(code));
  std::getline(status_line, msg_);  // Get the rest of the line (message)
  msg_ = strip(msg_, " \r");
  while (std::getline(response, line) && line != "\r") {
    std::stringstream header_line(line);
    std::string       key, value;
    std::getline(header_line, key, ':');
    std::getline(header_line, value);
    headers_[normalize_field_name(key)] = strip(value, " \r");
  }

  // Add default headers
  if (!contains(headers_, "Proxy-Connection")) {
    headers_["Proxy-Connection"] = "keep-alive";
  }
  if (version_ == "HTTP/1.0" && !contains(headers_, "Connection")) {
    headers_["Connection"] = "close";
  } else if (version_ == "HTTP/1.1" && !contains(headers_, "Connection")) {
    headers_["Connection"] = "keep-alive";
  }
  headers_["Host"] = proxy_uri_.host + ":" + (proxy_uri_.port.empty() ? "80" : proxy_uri_.port);

  // Read content length
  if (contains(headers_, "Content-Length")) {
    content_length_ = std::stoull(headers_["Content-Length"]);
    headers_.erase("Content-Length");
    // Preallocate space for body
    body_.reserve(content_length_);
  } else if (contains(headers_, "Transfer-Encoding") && headers_["Transfer-Encoding"] == "chunked") {
    chunked_ = true;
  } else {
    log("Warning: No content length or chunked encoding specified");
  }
  // Read content type
  if (contains(headers_, "Content-Type")) {
    content_type_  = headers_.at("Content-Type");
    size_t ext_pos = content_type_.find(';');
    if (ext_pos != std::string::npos) content_type_ = content_type_.substr(0, ext_pos);
  }
}

/**
  @brief Appends a string to the body of the HTTPResponse

  @param[in]  data  String to append to body
  @param[in]  size  Number of characters to append from `data`

  @return  True if string was successfully appended, false if it would exceed the content length
**/
bool HTTPResponse::append_to_body(const std::string& data, std::uint64_t size) {
  if (!chunked_ && body_.length() + size > content_length_) {
    return false;
  }
  body_.append(data.begin(), data.begin() + size);
  if (chunked_) {
    content_length_ += size;
  }
  return true;
}

/**
  @brief Write a HTTPResponse to a string

  @return String representation of HTTPResponse
**/
std::string HTTPResponse::dump() const {
  std::stringstream response;

  // Dump response to buffer
  response << version_ << " " << static_cast<int>(code_) << " " << msg_ << "\r\n";
  for (const auto& kv : headers_) {
    if (kv.first == "Transfer-Encoding" && kv.second == "chunked") {
      continue;
    }
    response << kv.first << ": " << kv.second << "\r\n";
  }
  if (has_content_length_) response << "Content-Length: " << content_length_ << "\r\n";
  response << "\r\n";
  if (content_length_ > 0) {
    response << body_;
  }

  // Convert to string
  return response.str();
}

/**
  @brief Write a HTTPResponse to an output stream

  @param[inout]  os        Output stream
  @param[in]     response  HTTPResponse to write

  @return  std::ostream&  Output stream
**/
std::ostream& operator<<(std::ostream& os, const HTTPResponse& response) {
  os << response.dump();
  return os;
}
