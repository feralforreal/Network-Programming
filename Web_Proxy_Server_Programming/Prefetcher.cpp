#include "Prefetcher.h"

void Prefetcher::operator()(const ProxyURI& proxy_uri, const HTTPResponse& response) {
  sigignore(SIGPIPE);

  std::vector<ProxyURI>          links = parse_links(response);
  std::vector<std::future<bool>> futures;
  time_point                     start = myclock::now();

  for (auto& link : links) {
    futures.push_back(std::async(std::launch::async, &Prefetcher::fetch, this, link));
    if (Signaler::done) break;
  }
  if (Signaler::done) {
    Signaler::num_threads--;
    return;
  }

  while (!futures.empty() && (myclock::now() - start) < prefetch_timeout_ && !Signaler::done) {
    for (auto future_it = futures.begin(); futures.size() > 0 && future_it < futures.end();) {
      if (future_it->wait_for(std::chrono::milliseconds(1)) == std::future_status::ready) {
        // Remove finished future
        future_it = futures.erase(future_it);
        links.erase(links.begin() + (future_it - futures.begin()));
        if (Signaler::done) break;
        continue;
      }
      future_it++;
      if (Signaler::done) break;
    }
  }
  Signaler::num_threads--;
}

bool Prefetcher::fetch(ProxyURI proxy_uri) {
  std::string buf;
  Connection  server(ip_cache_);
  server.set_name("Prefetcher for '" + proxy_uri.absolute() + "'");

  buf.resize(MAXBUF);

  if (page_cache_->contains(proxy_uri)) {
    log("Prefetcher: Cache hit for %s", proxy_uri.absolute().c_str());
    return true;
  }

  if (server.connect(&proxy_uri) > 0) {
    std::string request = "GET " + proxy_uri.uri + " HTTP/1.1\r\nHost: " + proxy_uri.host + ":" + proxy_uri.port + "\r\n\r\n";
    log("Prefetcher: Sending request to %s\n%s", proxy_uri.absolute().c_str(), request.c_str());
    int n_sent = server.send_n(request);
    if (n_sent <= 0) {
      log("Prefetcher: Error sending request to %s", proxy_uri.absolute().c_str());
      return false;
    }
    auto opt_response = server.read_http_response(buf, proxy_uri);
    if (opt_response) {
      if (opt_response->code() == ResponseCode::OK) {
        page_cache_->put(proxy_uri, *opt_response);
        log("Prefetcher: Cached %s", proxy_uri.absolute().c_str());
        return true;
      } else {
        log("Prefetcher: Fetching %s returned code %lu", proxy_uri.absolute().c_str(), (size_t)opt_response->code());
      }
    } else {
      log("Prefetcher: Error fetching %s", proxy_uri.absolute().c_str());
    }
  }
  return false;
}

std::vector<ProxyURI> Prefetcher::parse_links(const HTTPResponse& response) {
  std::vector<ProxyURI> links;
  size_t                start = 0;

  // Only parse HTML pages
  if (response.content_type() != "text/html") return links;
  const std::string& body = response.body();

  while (start != std::string::npos && !Signaler::done) {
    start = body.find("href=\"", start);
    if (start == std::string::npos) break;
    start += 6;
    size_t href_end = body.find("\"", start);
    if (href_end != std::string::npos) {
      std::string link = body.substr(start, href_end - start);
      if (link.find("https://") == std::string::npos) {
        // Don't waste time prefetching HTTPS links
        ProxyURI uri = parse_uri(link, response.proxy_uri());
        if (!page_cache_->contains(uri)) links.push_back(uri);
      }
      start = href_end;
    }
    start += 1;
  }

  if (Signaler::done) {
    links.clear();
    return links;
  }

  log_vector("Found %lu links in %s:", links, "\n", links.size(), response.proxy_uri().absolute().c_str());

  return links;
}
