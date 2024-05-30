#include "Signaler.h"

volatile std::atomic<bool> Signaler::done{false};
volatile std::atomic<int>  Signaler::num_threads{0};
std::recursive_mutex       Signaler::io_mutex{};
