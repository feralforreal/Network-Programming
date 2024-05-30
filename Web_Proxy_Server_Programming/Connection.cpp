#include "Connection.h"

Connection::Connection(Connection&& other) : sockfd_(other.sockfd_), ip_cache_(other.ip_cache_) { other.sockfd_ = -1; }

Connection::~Connection() { close(); }

int Connection::connect(ProxyURI* proxy_info) {
  struct addrinfo hints, *server_info = NULL;
  std::string     key = proxy_info->host + ":" + proxy_info->port;
  // Check cache for Host/IP mapping
  auto addr           = ip_cache_->get(key);
  if (addr) {
    sockfd_ = connect(proxy_info, addr.get());
    if (sockfd_ >= 0) return sockfd_;
    else {
      log("Error connecting to server at cached IP: %s\nRemoving cached value and finding new IP", strerror(errno));
      ip_cache_->remove(key);
    }
  }

  // Cache value not found, get IP from DNS
  memset(&hints, 0, sizeof(hints));

  log("Getting server info for %s:%s", proxy_info->host.c_str(), proxy_info->port.c_str());
  hints.ai_protocol = IPPROTO_TCP;
  int ret           = getaddrinfo(proxy_info->host.c_str(), proxy_info->port.c_str(), &hints, &server_info);
  if (ret != 0) {
    log("getaddrinfo failed: host=%s:%s, error=%s", proxy_info->host.c_str(), proxy_info->port.c_str(), gai_strerror(ret));
    return -1;
  }
  for (struct addrinfo* rp = server_info; rp != NULL; rp = rp->ai_next) {
    sockfd_ = connect(proxy_info, rp);
    if (sockfd_ == -1) continue;

    // Cache IP
    ip_cache_->put(key, AddrInfo{rp});
    break;
  }
  freeaddrinfo(server_info);
  return sockfd_;
}

void Connection::close() {
  if (sockfd_ > 0) ::close(sockfd_);
  sockfd_ = -1;
}

int Connection::recv(std::string& buf, size_t n, int flags, bool autoclose) {
  size_t max_len = n > 0 ? n : buf.capacity();
  return recv(&buf[0], max_len, flags, autoclose);
}

int Connection::recv(char* buf, size_t n, int flags, bool autoclose) {
  if (!is_connected()) return -1;
  int read = ::recv(sockfd_, &buf[0], n, flags);
  if (read == 0) {
    if (autoclose) {
      int old_errno = errno;
      close();
      errno = old_errno;
    }
  } else if (read < 0 && !(errno == EWOULDBLOCK || errno == EAGAIN)) {
    if (autoclose) {
      int old_errno = errno;
      close();
      errno = old_errno;
    }
  }
  return read;
}

int Connection::send_n(const std::string& data, size_t len, bool autoclose) {
  int n_send_total = 0;
  int n_send       = 0;
  int size         = len > 0 ? len : data.size();

  if (!is_connected()) return -1;
  while (n_send_total < size && !Signaler::done) {
    n_send = write(sockfd_, &data[n_send_total], size - n_send_total);
    if (n_send < 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        continue;
      } else {
        if (autoclose) {
          int old_errno = errno;
          close();
          errno = old_errno;
        }
        return n_send;
      }
    } else if (n_send == 0) {
      if (autoclose) {
        int old_errno = errno;
        close();
        errno = old_errno;
      }
      return n_send;
    }
    n_send_total += n_send;
  }
  return n_send_total;
}

int Connection::read_n(std::string& buf, int n, bool autoclose) { return read_n(&buf[0], n, autoclose); }

int Connection::read_n(char* buf, int n, bool autoclose) {
  int n_read_total = 0;
  int n_read       = 0;

  if (!is_connected()) return -1;
  while (n_read_total < n && is_connected() && !Signaler::done) {
    n_read = recv(&buf[n_read_total], n - n_read_total, 0, autoclose);
    if (n_read <= 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) continue;
      return n_read;
    }
    n_read_total += n_read;
  }
  return n_read_total;
}

/**
  @brief Read HTTP header from a connection

  @param[out]  buf     Buffer to temporarily store read data, may be garbage after call
  @param[out]  header  Output string to store header in

  @return  Number of bytes read, or -1 on error
**/
int Connection::read_http_header(std::string& buf, std::string& header) {
  int  n_src      = 0;
  bool body_found = false;

  if (header.capacity() < MAXLINE) header.reserve(MAXLINE);
  if (!header.empty()) header.clear();

  // make sure connection is alive
  n_src = recv(&buf[0], 1, MSG_PEEK | MSG_DONTWAIT);
  if (n_src <= 0 && !(errno == EWOULDBLOCK || errno == EAGAIN)) {
    return n_src;
  }

  int err_count = 0;
  while (!body_found && is_connected() && !Signaler::done) {
    err_count++;
    if (err_count > 100) {
      log("Can't find body! Current header:\n%s", header.c_str());
      return header.size() > 0 ? -1 : 0;
    }

    // Read from connections
    n_src = recv(buf, buf.capacity(), MSG_PEEK);
    if (n_src <= 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) continue;
      return n_src;
    }

    // Find message body
    size_t body_index     = buf.find("\r\n\r\n");
    body_found            = body_index != std::string::npos;
    size_t header_len_buf = body_found ? body_index + 4 : n_src;

    // Read and append headers (possibly only a segment)
    n_src = read_n(buf, header_len_buf);
    if (n_src <= 0) return n_src;
    header.append(buf.begin(), buf.begin() + n_src);
  }
  if (!body_found) return -1;

  return header.size();
}

std::unique_ptr<HTTPResponse> Connection::read_http_response(std::string& buf, ProxyURI proxy_info) {
  int n_src = 0;

  // Read response header
  std::string header;
  n_src = read_http_header(buf, header);
  if (n_src <= 0) return nullptr;

  // Parse response header
  auto response = std::unique_ptr<HTTPResponse>(new HTTPResponse(header, proxy_info));
  log("Received response from server:\n%s", response->dump().c_str());

  // Read response body
  n_src = read_http_response_body(buf, *response);
  if (response->content_length() > 0 && n_src <= 0) return nullptr;
  else if (response->content_length() > 0 && n_src < 0) return nullptr;

  return response;
}

/**
  @brief Read response body with chunked encoding from a connection

  @param[inout]  buf       Buffer to temporarily store read data, may be garbage after call
  @param[inout]  response  Response object to store body in

  @return  Number of bytes read, or -1 on error
**/
int Connection::read_http_response_body_chunked(std::string& buf, HTTPResponse& response) {
  int n_src = 0;

  // Read all chunks and compile into body
  do {
    // Read chunk size
    n_src = recv(&buf[0], buf.capacity(), MSG_PEEK);
    if (n_src <= 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        continue;
      } else {
        return n_src;
      }
    }

    // Parse chunk size
    size_t chunk_size_index = buf.find("\r\n");
    if (chunk_size_index == std::string::npos) {
      log("Error: chunk size not found");
      return -1;
    }
    n_src = read_n(buf, chunk_size_index + 2);
    if (n_src <= 0) return n_src;

    // Check for extensions
    size_t        chunk_extension_index = buf.find(";");
    std::uint64_t chunk_size            = std::stoull(buf.substr(0, std::min(chunk_extension_index, chunk_size_index)), nullptr, 16);
    log("Chunk size: %llu bytes\n%s", chunk_size, buf.substr(0, chunk_size_index + 2).data());
    if (chunk_size == 0) {
      log("Reached end of chunked encoding");
      // End of chunked encoding, read final CRLF
      n_src = read_n(&buf[chunk_size_index + 2], 2);
      if (n_src <= 0) return n_src;
      break;
    }

    // Read chunk
    std::uint64_t bytes_read = 0;
    while (bytes_read < chunk_size + 2 && !Signaler::done) {
      int bytes_to_read = std::min(buf.capacity(), chunk_size + 2 - bytes_read);
      n_src             = read_n(&buf[0], bytes_to_read);
      if (n_src <= 0) return n_src;
      bytes_read += n_src;

      // Sanity check
      if (bytes_read == chunk_size + 2) {
        log("Finished reading chunk of size %llu", chunk_size);
        if (buf[n_src - 2] != '\r' || buf[n_src - 1] != '\n') {
          log("Error: chunk does not end with CRLF");
        }
        response.append_to_body(buf, n_src - 2);
      } else {
        response.append_to_body(buf, n_src);
      }
    }
    log("Appended chunk of size %llu to body", chunk_size);
  } while (n_src > 0 && !Signaler::done);
  return response.body().size();
}

/**
  @brief Read response body with known content length from a connection

  @param[inout]  buf       Buffer to temporarily store read data, may be garbage after call
  @param[inout]  response  Response object to store body in

  @return  Number of bytes read, or -1 on error
**/
int Connection::read_http_response_body(std::string& buf, HTTPResponse& response) {
  int body_len = 0, n_src = 0;

  if (response.is_chunked()) return read_http_response_body_chunked(buf, response);

  // Read response.content_length() bytes from connection
  while (body_len < response.content_length() && !Signaler::done) {
    if (n_src > 0) bzero(&buf[0], n_src);
    n_src = recv(&buf[0], std::min(MAXLINE, (int)(response.content_length() - body_len)));
    if (n_src <= 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        continue;
      } else {
        return n_src;
      }
    }

    response.append_to_body(buf, n_src);
    body_len += n_src;
    log("Continuing to read response body...read %d bytes", n_src);
  }
  return body_len;
}

int Connection::connect(ProxyURI* proxy_info, addrinfo const* addr_info) {
  sockfd_ = socket(addr_info->ai_family, addr_info->ai_socktype, addr_info->ai_protocol);
  if (sockfd_ == -1) return -1;
  if (::connect(sockfd_, addr_info->ai_addr, addr_info->ai_addrlen) == 0) {
    switch (addr_info->ai_family) {
      case AF_INET: {
        char dst[INET_ADDRSTRLEN];
        log("Connected via IPv4: %s", inet_ntop(addr_info->ai_family, &addr_info->ai_addr, dst, sizeof(dst)));
        proxy_info->ip = std::string(dst);
        break;
      }
      case AF_INET6: {
        char dst[INET6_ADDRSTRLEN];
        log("Connected via IPv6: %s", inet_ntop(addr_info->ai_family, &addr_info->ai_addr, dst, sizeof(dst)));
        proxy_info->ip = std::string(dst);
        break;
      }
      default: {
        log("Connected via unknown protocol");
        break;
      }
    }
    return sockfd_;
  }
  ::close(sockfd_);
  return sockfd_ = -1;
}

int Connection::connect(ProxyURI* proxy_info, AddrInfo const* addr_info) {
  sockfd_ = socket(addr_info->ai_family, addr_info->ai_socktype, addr_info->ai_protocol);
  if (sockfd_ == -1) return -1;
  if (::connect(sockfd_, addr_info->ai_addr.get(), addr_info->ai_addrlen) == 0) {
    switch (addr_info->ai_family) {
      case AF_INET: {
        char dst[INET_ADDRSTRLEN];
        log("Connected via IPv4: %s", inet_ntop(addr_info->ai_family, &addr_info->ai_addr, dst, sizeof(dst)));
        proxy_info->ip = std::string(dst);
        break;
      }
      case AF_INET6: {
        char dst[INET6_ADDRSTRLEN];
        log("Connected via IPv6: %s", inet_ntop(addr_info->ai_family, &addr_info->ai_addr, dst, sizeof(dst)));
        proxy_info->ip = std::string(dst);
        break;
      }
      default: {
        log("Connected via unknown protocol");
        break;
      }
    }
    return sockfd_;
  }
  ::close(sockfd_);
  return sockfd_ = -1;
}
