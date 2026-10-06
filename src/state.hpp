#pragma once
#include <cstdint>
#include <mutex>

// Runtime state tracking
class Performance {
public:
    enum class Phase { idle, cutting, awaiting_click };
private:
    mutable std::mutex mutex_;
    Phase phase_ = Phase::idle;
    uint64_t expires_ = 0;
    uint64_t failureAt_ = 0;
    bool failureSent_ = false;
    void expire(uint64_t now) {
        if (phase_ != Phase::idle && now >= expires_) phase_ = Phase::idle;
    }
public:
    bool begin(uint64_t now, unsigned seconds, unsigned failureDelay = 0) {
        std::lock_guard lock(mutex_); expire(now);
        if (phase_ != Phase::idle) return false;
        phase_ = Phase::cutting; expires_ = now + uint64_t(seconds) * 1000;
        failureAt_ = failureDelay ? now + uint64_t(failureDelay) * 1000 : 0;
        failureSent_ = false;
        return true;
    }
    bool takeFailure(uint64_t now) {
        std::lock_guard lock(mutex_); expire(now);
        if(phase_ != Phase::cutting || !failureAt_ || failureSent_ || now < failureAt_)return false;
        failureSent_ = true; return true;
    }
    bool failureDue(uint64_t now) {
        std::lock_guard lock(mutex_); expire(now);
        return phase_ == Phase::cutting && failureAt_ && !failureSent_ && now >= failureAt_;
    }
    bool blocked(uint64_t now) {
        std::lock_guard lock(mutex_); expire(now); return phase_ == Phase::cutting;
    }
    bool login(uint64_t now) {
        std::lock_guard lock(mutex_); expire(now);
        if (phase_ != Phase::cutting) return false;
        phase_ = Phase::awaiting_click;
        // Expire armed state on timeout
        expires_ = now + 5 * 60 * 1000;
        return true;
    }
    bool click(uint64_t now) {
        std::lock_guard lock(mutex_); expire(now);
        if (phase_ != Phase::awaiting_click) return false;
        phase_ = Phase::idle; return true;
    }
    bool cancel() {
        std::lock_guard lock(mutex_);
        bool changed = phase_ != Phase::idle; phase_ = Phase::idle; return changed;
    }
    Phase phase(uint64_t now) {
        std::lock_guard lock(mutex_); expire(now); return phase_;
    }
};
