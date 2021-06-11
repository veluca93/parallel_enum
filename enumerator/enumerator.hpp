#ifndef ENUMERATOR_ENUMERATOR_H
#define ENUMERATOR_ENUMERATOR_H

#include <atomic>
#include <chrono>
#include <cstdio>
#include <iomanip>
#include <memory>
#include <mutex>

#include "absl/memory/memory.h"
#include "enumerable/enumerable.hpp"

template <typename Node, typename Item>
class Enumerator {
 public:
  Enumerator() { start_time_ = std::chrono::high_resolution_clock::now(); }

  // To be called when reading input is done.
  void ReadDone() {
    read_done_time_ = std::chrono::high_resolution_clock::now();
    ReadDoneInternal();
  }

  // Call this to start the enumeration.
  void Run(Enumerable<Node, Item>* system) {
    run_start_time_ = std::chrono::high_resolution_clock::now();
    print_time_ = run_start_time_;
    RunInternal(system);
    run_done_time_ = std::chrono::high_resolution_clock::now();
  }

  // Call this when the enumeration is done to print statistics.
  virtual void PrintStats(FILE* out = stdout) {
    fprintf(out, "Reading time: %ld ms\n",
            std::chrono::duration_cast<std::chrono::milliseconds>(
                read_done_time_ - start_time_)
                .count());
    fprintf(out, "Setup time: %ld ms\n",
            std::chrono::duration_cast<std::chrono::milliseconds>(
                run_start_time_ - read_done_time_)
                .count());
    fprintf(out, "Run time: %ld ms\n",
            std::chrono::duration_cast<std::chrono::milliseconds>(
                run_done_time_ - run_start_time_)
                .count());
    fprintf(out, "Solutions found: %lu\n", (ssize_t)solutions_found_);
    fprintf(out, "Computational tree size: %lu\n", (ssize_t)tree_size_);
    fprintf(out, "Solutions per ms: %f\n",
            (float)solutions_found_ /
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    run_done_time_ - run_start_time_)
                    .count());
    PrintStatsInternal();
  }

  // Creates an enumerable system and sets it.
  template <class Enumerable, typename... Args>
  void MakeEnumerableSystemAndRun(Args&&... args) {
    Run(absl::make_unique<Enumerable>(args...).get());
  }

  // Sets a function to be called whenever a solution is found.
  void SetItemFoundCallback(const std::function<void(const Item&)>& cb) {
    cb_ = cb;
  }

  // Call to have the solutions printed on stdout
  void PrintSolutionsOnStdout() {

    std::function<void(const Item&)> print_cb =
      [](const Item& item) {
        for(auto n : item ){ printf("%lu ", (long unsigned) n); }
        printf("\n");
      };

      SetItemFoundCallback(print_cb);
  }

  virtual void ReportSolution(Enumerable<Node, Item>* system,
                              const Node& node) {
    tree_size_++;
    if (!system->IsSolution(node)) return;
    solutions_found_++;
#ifdef PRINT_PROGRESS
    if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - print_time_).count() > 1000) {
        std::unique_lock<std::mutex> lck(m_);
        print_time_ = std::chrono::high_resolution_clock::now();
        std::cout << std::setw(30) << solutions_found_ << " solutions found\r" << std::flush;
    }
#endif
    if (cb_) {
      return cb_(system->NodeToItem(node));
    }
  }

  ssize_t GetSolutionsFound() { return solutions_found_; }

 protected:
  virtual void RunInternal(Enumerable<Node, Item>* system) = 0;
  virtual void ReadDoneInternal() {}
  virtual void PrintStatsInternal() {}

  std::function<void(const Item&)> cb_{nullptr};
  std::chrono::high_resolution_clock::time_point start_time_;
  std::chrono::high_resolution_clock::time_point read_done_time_;
  std::chrono::high_resolution_clock::time_point run_start_time_;
  std::chrono::high_resolution_clock::time_point print_time_;
  std::chrono::high_resolution_clock::time_point run_done_time_;
  std::atomic<ssize_t> solutions_found_{0};
  std::atomic<ssize_t> tree_size_{0};
  std::mutex m_;
};

#endif  // ENUMERATOR_ENUMERATOR_H
