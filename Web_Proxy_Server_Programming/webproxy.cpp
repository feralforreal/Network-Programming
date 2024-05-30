/*
 * webproxy.cpp - A simple, multithreaded HTTP proxy
 */

#include <sys/socket.h> /* for socket use */

#include <iostream>
#include <mutex>
#include <thread>

#include "Prefetcher.h"
#include "ProxyConnection.h"
#include "Signaler.h"

int  open_listenfd(int port);
void sigint_handler(int) { Signaler::done = true; }

int main(int argc, char **argv) {
  int                listenfd, port, timeout_sec;
  socklen_t          clientlen = sizeof(struct sockaddr_in);
  struct sockaddr_in clientaddr;
  struct sigaction   act = {0};
  std::uint64_t      id  = 0;

  // Read command line arguments
  if (argc > 3 || argc < 2) {
    fprintf(stderr, "usage: %s <port> [cache_timeout, default=60]\n", argv[0]);
    exit(0);
  }
  port        = atoi(argv[1]);
  timeout_sec = argc == 3 ? atoi(argv[2]) : 60;

  // Set up signal handler
  act.sa_handler = &sigint_handler;
  sigignore(SIGPIPE);
  sigaction(SIGINT, &act, NULL);

  // Prevent buffering of stdout
  std::ios::sync_with_stdio(true);

  // Set up global caches
  ProxyConnection::load_blacklist("blacklist.txt");
  auto page_cache = std::make_shared<Cache<HTTPResponse, ProxyURI>>(std::chrono::seconds(timeout_sec));
  auto ip_cache   = std::make_shared<Cache<AddrInfo>>();

  // Set prefetcher callback
  page_cache->set_insertion_callback(
      [&ip_cache, &page_cache](ProxyURI uri, HTTPResponse resp) { start_prefetcher(ip_cache, page_cache, uri, resp); });

  // Open listening socket
  listenfd = open_listenfd(port);
  while (!Signaler::done) {
    // Accept connections
    int connfdp = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
    if (connfdp < 0) continue;
    Signaler::num_threads++;
    ProxyConnection proxy_conn(id++, connfdp, ip_cache, page_cache, 20);
    std::thread(std::move(proxy_conn)).detach();
  }
  log("Waiting for %d threads to finish...", Signaler::num_threads.load());

  time_point start = myclock::now();
  while (Signaler::num_threads > 0 && myclock::now() - start < std::chrono::seconds(5)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  if (Signaler::num_threads > 0) {
    log("Timed out waiting for threads to finish...killing them");
  }
  page_cache.reset();
  ip_cache.reset();
  log("Num references on page_cache: %d", page_cache.use_count());
  log("Main thread exiting...goodbye!");
}

/**
  @brief Create a listening socket on a given port

  @param[in]  port  Port to listen on

  @return  Created socket file descriptor
**/
int open_listenfd(int port) {
  int                listenfd, optval = 1;
  struct sockaddr_in serveraddr;

  /* Create a socket descriptor */
  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;

  /* Eliminates "Address already in use" error from bind. */
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0) return -1;

  /* listenfd will be an endpoint for all requests to port
     on any IP address for this host */
  bzero((char *)&serveraddr, sizeof(serveraddr));
  serveraddr.sin_family      = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port        = htons((unsigned short)port);
  if (bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) return -1;

  /* Make it a listening socket ready to accept connection requests */
  if (listen(listenfd, LISTENQ) < 0) return -1;
  return listenfd;
}
