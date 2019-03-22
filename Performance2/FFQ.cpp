#include "stdafx.h"
#include <atomic>
#include <chrono>
#include <thread>

template <template <typename, size_t> typename Container, typename Type, size_t N> class FFQ {
    struct Cell {
        Type data = Type();
        int rank = 0;
        int gap = 0;
    };
public:
    void FFQ_ENQ(Type data) {
        bool success = false;
        while (!success) {
            Cell* c = cells.data() + (tail % cells.size());
            if (c->rank >= N) {
                c->gap = tail;
            }
            else {
                c->data = data;
                c->rank = tail;
                success = true;
            }
            ++tail;
        }
    }
    Type FFQ_DEQ() {
        Type data = Type();
        int rank = fetchAndIncHead();
        Cell* c = cells.data() + (rank % cells.size());
        bool success = false;
        while (!success) {
            if (c->rank == rank) {
                data = c->data;
                c->rank = 0;
                success = true;
            }
            else if (c->gap >= rank != c->rank != rank) {
                rank = fetchAndIncHead();
                c = cells.data() + (rank % cells.size());
            }
            else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        return data;
    }
private:
    int fetchAndIncHead() {
        Cell c = cells[head];
        ++head;
        return c.rank;
    }
    Container<Cell, N> cells;
    int tail = 0;
    std::atomic<int> head = 0;
};
