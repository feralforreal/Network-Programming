#include "ProxyConnection.h"

std::unordered_map<std::string, bool> ProxyConnection::blacklist_;

ProxyConnection::ProxyConnection(uint64_t id, int client_fd, std::shared_ptr<Cache<AddrInfo>> ip_cache,
                                 std::shared_ptr<Cache<HTTPResponse, ProxyURI>> page_cache)
    : id_(id), client_(client_fd, ip_cache), server_(ip_cache), ip_cache_(ip_cache), page_cache_(page_cache) {
  std::string name = "Proxy Connection " + std::to_string(id_);
  client_.set_name(name + " (client)");
  server_.set_name(name + " (server)");
}

ProxyConnection::ProxyConnection(uint64_t id, int client_fd, std::shared_ptr<Cache<AddrInfo>> ip_cache,
                                 std::shared_ptr<Cache<HTTPResponse, ProxyURI>> page_cache, int connection_timeout_seconds)
    : ProxyConnection(id, client_fd, ip_cache, page_cache) {
  proxy_timeout_   = std::chrono::seconds{connection_timeout_seconds};
  gateway_timeout_ = std::chrono::seconds{std::max(connection_timeout_seconds / 4, 1)};
}

ProxyConnection::ProxyConnection(ProxyConnection&& other)
    : id_(other.id_),
      client_(std::move(other.client_)),
      server_(std::move(other.server_)),
      proxy_timeout_(other.proxy_timeout_),
      gateway_timeout_(other.gateway_timeout_),
      page_cache_(other.page_cache_),
      ip_cache_(other.ip_cache_) {
  client_.set_name(other.client_.name());
  server_.set_name(other.server_.name());
}

ProxyConnection::~ProxyConnection() {
  client_.close();
  server_.close();
}

void ProxyConnection::operator()() {
  sigignore(SIGPIPE);

  // Run the proxy connection
  int         num_messages = 0;
  int         n = MAXLINE, n_response = MAXLINE;
  std::string reason;
  std::string request_buf, response_buf, header;
  ProxyURI    last_uri;
  time_point  thread_start = myclock::now(), start = myclock::now();

  header.reserve(MAXLINE);
  request_buf.resize(MAXLINE);
  response_buf.resize(MAXLINE);

  log("Starting proxy connection on socket %d", client_.fd());

  do {
    start = myclock::now();

    // Wait for request from client, or timeout
    if (n > 0) bzero(&request_buf[0], n);
    do {
      n = client_.recv(request_buf, 1, MSG_PEEK | MSG_DONTWAIT);
      if ((myclock::now() - start) > proxy_timeout_) {
        reason = "Timeout";
        break;
      }
      if (n <= 0 && !(errno == EWOULDBLOCK || errno == EAGAIN)) {
        reason = std::string("read from client: ") + strerror(errno);
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (n <= 0 && !Signaler::done);
    if (!reason.empty()) break;

    // Read request from client
    n = client_.read_http_header(request_buf, header);
    if (n <= 0) {
      reason = std::string("read from client: ") + strerror(errno);
      break;
    }
    num_messages++;

    // Parse request
    HTTPRequest request(header);
    log("Received request from client:\n%s", request.dump().c_str());

    // Check blacklist, URL
    if (!allowed(request.proxy_uri.host)) {
      HTTPResponse response(request, ResponseCode::Forbidden);
      log("Sending response to client:", response);
      if (client_.send_n(response.dump()) <= 0) {
        reason = std::string("write to client: ") + strerror(errno);
        break;
      }
      continue;
    }

    // Check request type
    if (request.method != RequestMethod::GET && request.method != RequestMethod::CONNECT) {
      HTTPResponse response(request, ResponseCode::BadRequest);
      log("Sending response to client:", response);
      if (client_.send_n(response.dump()) <= 0) {
        reason = std::string("write to client: ") + strerror(errno);
        break;
      }
      continue;
    }
    if (request.method == RequestMethod::CONNECT) {
      log("CONNECT Request, initializing tunnel");
      tunnel(request);
      reason = "CONNECT Tunneling Complete";
      break;
    }

    // Check cache
    auto response = page_cache_->get(request.proxy_uri);
    if (response) {
      if (client_.send_n(response->dump()) <= 0) {
        reason = std::string("write to client: ") + strerror(errno);
        break;
      }
      log("Sending cached response to client for '%s'", request.proxy_uri.absolute().c_str());
      continue;
    }

    // Forward request to server, send cached response, or send error to client
    bool       response_sent     = false;
    time_point server_conn_start = myclock::now();
    do {
      // Reuse connection if possible
      if (!server_.is_connected() || request.proxy_uri.host != last_uri.host || request.proxy_uri.port != last_uri.port) {
        server_.close();
        server_.connect(&request.proxy_uri);
        if (!server_.is_connected()) {
          HTTPResponse response(request, ResponseCode::NotFound);
          log("Sending response to client:", response);
          if (client_.send_n(response.dump()) <= 0) {
            reason = std::string("write to client: ") + strerror(errno);
          } else response_sent = true;
          break;
        }
        // Check blacklist, IP
        if (!allowed(request.proxy_uri.ip)) {
          HTTPResponse response(request, ResponseCode::Forbidden);
          log("Sending response to client:", response);
          if (client_.send_n(response.dump()) <= 0) {
            reason = std::string("write to client: ") + strerror(errno);
          } else response_sent = true;
          break;
        }
        last_uri = request.proxy_uri;
      }

      // Send request to server
      log("Sending request to server");
      n_response = server_.send_n(request.dump());
      if (n_response <= 0) {
        log("Server closed connection, reconnecting...");
        server_.close();
        continue;
      }

      // Forward server response to client
      bzero(&response_buf[0], response_buf.capacity());

      // Read response header
      auto opt_response = server_.read_http_response(response_buf, request.proxy_uri);
      if (!opt_response) {
        log("Error reading response from server");
        server_.close();
        continue;
      }

      // Send (and cache) response
      log("Sending server response to client");
      n_response = client_.send_n(opt_response->dump());
      if (opt_response->code() == ResponseCode::OK) {
        log("Added response to cache.");
        page_cache_->put(opt_response->proxy_uri(), *opt_response);
      }
      if (n_response <= 0) {
        reason = std::string("write to client: ") + strerror(errno);
        break;
      } else response_sent = true;

    } while (!response_sent && (myclock::now() - server_conn_start < gateway_timeout_) && !Signaler::done);
    if (!response_sent) {
      HTTPResponse response(request, ResponseCode::GatewayTimeout);
      log("Sending response to client:", response);
      if (client_.send_n(response.dump()) <= 0) {
        reason = std::string("write to client: ") + strerror(errno);
      }
    }
  } while (reason.empty() && (myclock::now() - start < proxy_timeout_) && !Signaler::done);
  if (Signaler::done) {
    reason = "User terminated proxy server";
  }
  log("Closing proxy connection on socket %d\nReason: %s\nProcessed %d messages\nAlive for %f seconds", client_.fd(), reason.c_str(), num_messages,
      std::chrono::duration<double>(myclock::now() - thread_start).count());
  Signaler::num_threads--;
}

void ProxyConnection::tunnel(HTTPRequest& request) {
  // Should have a CONNECT request
  if (request.method != RequestMethod::CONNECT) {
    log("Error: tunnel() called with non-CONNECT request");
    return;
  }

  // Connect to server
  server_.connect(&request.proxy_uri);
  if (!server_.is_connected()) {
    HTTPResponse response(request, ResponseCode::NotFound);
    log("Sending response to client:", response);
    client_.send_n(response.dump());
    return;
  }
  // Check blacklist, IP
  if (!allowed(request.proxy_uri.ip)) {
    HTTPResponse response(request, ResponseCode::Forbidden);
    log("Sending response to client:", response);
    client_.send_n(response.dump());
    return;
  }

  // Send OK response to client
  std::string response = "HTTP/1.1 200 OK\r\n\r\n";
  // HTTPResponse response(request, ResponseCode::OK);
  // log("Sending response to client:\n%s", response.c_str());
  client_.send_n(response);

  // Enter tunneling mode
  log("Entering tunneling mode");
  std::string buf;
  buf.resize(MAXBUF);

  struct pollfd fds[2];
  Connection*   conns[2];
  conns[0]       = &client_;
  conns[1]       = &server_;
  const int nfds = 2;
  memset(fds, 0, sizeof(fds));
  for (int i = 0; i < nfds; i++) {
    fds[i].fd     = conns[i]->fd();
    fds[i].events = POLLIN;
  };

  time_point start = myclock::now();
  while (!Signaler::done && client_.is_connected() && server_.is_connected() && myclock::now() - start < std::chrono::seconds{50}) {
    bool close[2] = {false, false};
    // Poll fds
    int err       = poll(fds, nfds, 200);

    // Check for errors
    if (err < 0) {
      log("Error polling fds");
      break;
    } else if (err == 0) {
      // Check for closed connections
      for (int i = 0; i < nfds; i++) {
        int n = conns[i]->recv(buf, 1, MSG_PEEK | MSG_DONTWAIT, false);
        if (n == 0 || n < 0 && !(errno == EWOULDBLOCK || errno == EAGAIN)) {
          log("Client closed connection");
          conns[i]->close();
          conns[1 - i]->close();
          break;
        }
      }
      continue;
    }

    // log("Poll returned %d", err);

    // Check for events
    for (int i = 0; !Signaler::done && i < nfds; i++) {
      int total_sent = 0;
      if (!(fds[i].revents)) continue;
      if (fds[i].revents & POLLIN) {
        // log("Tunneling data from %s to %s", i == 0 ? "client" : "server", i == 0 ? "server" : "client");
        do {
          // Read from fd[i]
          int n_read = conns[i]->recv(buf, buf.capacity(), MSG_DONTWAIT, false);
          if (n_read == 0) {
            // Connection closed
            log("Connection closed on fd %d", fds[i].fd);
            close[i] = true;
            break;
          } else if (n_read < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
            // Finished reading
            break;
          } else if (n_read < 0) {
            // EOF
            log("Error reading from fd %d: %s", fds[i].fd, strerror(errno));
            close[i] = true;
            break;
          }

          // Write to fd[1-i]
          int n_sent = conns[1 - i]->send_n(buf, n_read, false);
          if (n_sent <= 0) {
            log("Error writing to fd %d", fds[1 - i].fd);
            close[1 - i] = true;
          }
          start = myclock::now();
          total_sent += n_sent;
        } while (!Signaler::done);
      }
      // if (total_sent > 0) log("Sent %d bytes from %s to %s", total_sent, i == 0 ? "client" : "server", i == 0 ? "server" : "client");
      if (fds[i].revents & POLLHUP) {
        log("Connection closed on fd %d", fds[i].fd);
        close[i] = true;
        break;
      }
      if (fds[i].revents != POLLIN) {
        log("Unhandled event on fd %d: %d", fds[i].fd, fds[i].revents);
        close[i] = true;
      }
    }
    if (close[0] || close[1]) {
      server_.close();
    }
  }
  log("Exiting tunneling mode");
  return;
}

/**
  @brief Check if a host is allowed by the proxy blacklist

  @param[in]  host  Hostname or IP to check

  @return  True if host is allowed, false otherwise
**/
bool ProxyConnection::allowed(const std::string& host) { return !blacklist_.count(host); }

/**
  @brief Static function to load the proxy blacklist from a file, should be called once before any ProxyConnection objects are created

  @param[in]  filename  File to load blacklist from
**/
void ProxyConnection::load_blacklist(const std::string& filename) {
  std::ifstream in(filename);
  // Regex to match IP addresses in blacklist file
  std::regex ip_addr_regex(R"((\d{1,3}|\*)\.(\d{1,3}|\*)\.(\d{1,3}|\*)\.(\d{1,3}|\*))");
  std::regex comment_regex(R"(#.*$)");
  if (!in) return;

  std::string line;
  while (std::getline(in, line) && !Signaler::done) {
    if (std::regex_match(line, comment_regex)) continue;
    if (std::regex_match(line, ip_addr_regex)) {
      // IP address
      ::log("Adding %s to blacklist", line.c_str());
      int num_star = std::count(line.begin(), line.end(), '*');
      if (num_star == 0) {
        blacklist_[line] = true;
        continue;
      }
      for (int i = 0; i < (1 << (8 * num_star)); i++) {
        std::string ip = line;
        for (int j = 0; j < num_star; j++) {
          int pos = ip.find('*');
          ip.replace(pos, 1, std::to_string((i >> (8 * j)) & 0xFF));
        }
        if (i == 0) ::log("Adding %s to blacklist", ip.c_str());
        if (i == (1 << (8 * num_star)) - 1) ::log("Adding %s to blacklist", ip.c_str());
        blacklist_[ip] = true;
      }
    } else {
      blacklist_[line] = true;
    }
  }
}
