#ifndef RATELIMITER_HPP
#define RATELIMITER_HPP

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace miniCDN{

class RateLimiter {
public:
    // capacity : max tokens a bucket can hold  (= max burst size)
    // refill_rate : tokens added per second
    RateLimiter(double capacity, double refill_rate);

    bool allow(const std::string& client_ip);

private:
    struct Bucket {
        double   tokens; // current token count (fractional ok)
        std::chrono::steady_clock::time_point last_refill;
    };

    double m_capacity; // maximum tokens per bucket
    double m_refill_rate; // tokens added per second

    std::mutex m_mutex;
    std::unordered_map<std::string, Bucket> m_buckets;

    // Refill the bucket based on elapsed time and return the updated token count.
    double refill(Bucket& bucket);
};

} // namespace miniCDN

#endif
