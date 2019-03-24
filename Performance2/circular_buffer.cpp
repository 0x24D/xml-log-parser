#include "stdafx.h"
#include <memory>
#include <mutex>

template <class T>
class circular_buffer {
    public:
        circular_buffer(size_t size) : 
            m_buf(std::unique_ptr<T[]>(new T[size])),
            m_max_size(size) {}
        void put (T item) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_buf[m_head] = item;
            if (m_full) {
                m_tail = (m_tail + 1) % m_max_size;
            }
            m_head = (m_head + 1) % m_max_size;
            m_full = m_head == m_tail;
        }
        T get() {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (empty()) {
                return T();
            }
            auto val = m_buf[m_tail];
            m_full = false;
            m_tail = (m_tail + 1) % m_max_size;
            return val;
        }
        void reset() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_head = m_tail;
            m_full = false;
        }
        bool empty() const {
            return (!m_full && (m_head == m_tail));
        }
        bool full() const {
            return m_full;
        }
        size_t capacity() const {
            return m_max_size;
        }
        size_t size() const {
            size_t size = m_max_size;
            if (!m_full) {
                if (m_head >= m_tail) {
                    size = m_head - m_tail;
                } else {
                    size = m_max_size + m_head - m_tail;
                }
            }
            return size;
        }
    private:
        std::mutex m_mutex;
        std::unique_ptr<T[]> m_buf;
        size_t m_head = 0;
        size_t m_tail = 0;
        const size_t m_max_size;
        bool m_full = 0;
};
