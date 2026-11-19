#include <yugawara/util/sharded_shared_mutex.h>

#include <cerrno>
#include <cstdlib>
#include <thread>

#include <glog/logging.h>

namespace yugawara::util {

namespace {

[[nodiscard]] bool is_prime(std::size_t n) noexcept {
    if (n < 2) {
        return false;
    }
    for (std::size_t i = 2; i * i <= n; ++i) {
        if (n % i == 0) {
            return false;
        }
    }
    return true;
}

} // namespace

std::size_t sharded_shared_mutex::compute_default_shard_count(
        char const* environment_value,
        std::size_t hardware_threads) noexcept {
    // PoC: the environment variable is only for measuring the proof-of-concept code
    if (environment_value != nullptr && *environment_value != '\0') {
        char* end {};
        errno = 0;
        auto value = std::strtoull(environment_value, &end, 10); // NOLINT(*-magic-numbers)
        if (errno == 0 && end != nullptr && *end == '\0'
                && value >= 1 && value <= max_shard_count_from_environment) {
            return static_cast<std::size_t>(value);
        }
    }
    std::size_t n = hardware_threads < 2 ? 2 : hardware_threads;
    while (!is_prime(n)) {
        ++n;
    }
    return n;
}

std::size_t sharded_shared_mutex::default_shard_count() noexcept {
    static std::size_t const result = [] {
        // PoC: the environment variable is only for measuring the proof-of-concept code
        char const* env = std::getenv(shard_count_environment_variable); // NOLINT(concurrency-mt-unsafe)
        auto hardware_threads = static_cast<std::size_t>(std::thread::hardware_concurrency());
        auto count = compute_default_shard_count(env, hardware_threads);
        LOG(INFO) << "yugawara::util::sharded_shared_mutex (PoC): "
                  << "shard_count=" << count << ", "
                  << shard_count_environment_variable << "=" << (env == nullptr ? "(unset)" : env) << ", "
                  << "hardware_concurrency=" << hardware_threads;
        return count;
    }();
    return result;
}

} // namespace yugawara::util
