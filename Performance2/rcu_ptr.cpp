#include "stdafx.h"
#include <memory>

template <typename T> class rcu_ptr {
    std::shared_ptr<const T> sp;

public:
    rcu_ptr() = default;
    ~rcu_ptr() = default;

    rcu_ptr(const rcu_ptr &rhs) = delete;
    rcu_ptr &operator=(const rcu_ptr &rhs) = delete;
    rcu_ptr(rcu_ptr &&) = delete;
    rcu_ptr &operator=(rcu_ptr &&) = delete;

    rcu_ptr(const std::shared_ptr<const T> &sp_) : sp(sp_) {}
    rcu_ptr(std::shared_ptr<const T> &&sp_) : sp(std::move(sp_)) {}

    std::shared_ptr<const T> read() const {
        return std::atomic_load_explicit(&sp, std::memory_order_consume);
    }

    void reset(const std::shared_ptr<const T> &r) {
        std::atomic_store_explicit(&sp, r, std::memory_order_release);
    }
    void reset(std::shared_ptr<const T> &&r) {
        std::atomic_store_explicit(&sp, std::move(r), std::memory_order_release);
    }

    template <typename R>
    void copy_update(R &&fun) {
        std::shared_ptr<const T> sp_l = std::atomic_load_explicit(&sp, std::memory_order_consume);

        std::shared_ptr<T> r;
        do {
            if (sp_l) {
                // deep copy 
                r = std::make_shared<T>(*sp_l);
            }

            // update 
            std::forward<R>(fun)(r.get());

        } while (!std::atomic_compare_exchange_strong_explicit(&sp, &sp_l, std::shared_ptr<const T>(std::move(r)), std::memory_order_release, std::memory_order_consume));
    }
};
