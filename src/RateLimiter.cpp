#include "RateLimiter.hpp"

#include <algorithm>  // std::min

namespace miniCDN {

RateLimiter::RateLimiter(double capacity, double refill_rate)
    : m_capacity(capacity), m_refill_rate(refill_rate) {}

double RateLimiter::refill(Bucket& bucket) {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - bucket.last_refill).count();

    // Add tokens proportional to elapsed time, but never exceed capacity
    bucket.tokens = std::min(m_capacity, bucket.tokens + elapsed * m_refill_rate);
    bucket.last_refill = now;

    return bucket.tokens;
}

bool RateLimiter::allow(const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // First time we see this IP: create a full bucket
    auto [it, inserted] = m_buckets.emplace(
        client_ip,
        Bucket{ m_capacity, std::chrono::steady_clock::now() }
    );

    Bucket& bucket = it->second;

    if(!inserted){
        // Existing bucket: top it up for elapsed time
        refill(bucket);
    }

    if(bucket.tokens >= 1.0){
        bucket.tokens -= 1.0; // consume one token
        return true; // request allowed
    }

    return false; // bucket empty → reject
}

} // namespace miniCDN
