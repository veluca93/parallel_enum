#ifndef ENUMERABLE_CONNECTED_HEREDITARY
#define ENUMERABLE_CONNECTED_HEREDITARY

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <cassert>

#include "enumerable/commutable.hpp"

template <typename Graph, typename Aux>
class CHSystem : public CommutableSystem<Graph, Aux> {
 protected:
  CHSystem(size_t graph_size) : CommutableSystem<Graph, Aux>(graph_size) {}
  using node_t = typename Graph::node_t;
  virtual bool IsNeigh(node_t a, node_t b) = 0;

  virtual absl::Span<const node_t> Neighs(node_t a) = 0;
  bool IsSeed(node_t v, const std::vector<node_t>* s) override final {
    return true;
  }

  void ValidSeeds(const std::vector<node_t>& sol, node_t cand,
                  const std::function<bool(node_t)>& cb) override {
    node_t min = sol[0];
    for (node_t elem : sol) {
      if (elem < min) {
        min = elem;
      }
    }
    for (node_t seed : sol) {
      if (cand <= seed) continue;
      if (IsNeigh(min, seed)) continue;
      if (!cb(seed)) break;
    }
  }

  void Resort(std::vector<node_t>& s, std::vector<int32_t>& level,
              node_t seed) override {
    size_t seed_index = -1ULL;
    for (size_t i = 0; i < s.size(); i++) {
      if (s[i] == seed) {
        seed_index = i;
        break;
      }
    }
    assert(seed_index != -1ULL);
    thread_local std::vector<bool> visited;
    visited.clear();
    visited.resize(s.size());
    using PQEl = std::tuple<int32_t, node_t>;
    thread_local std::priority_queue<PQEl, std::vector<PQEl>,
                                     std::greater<PQEl>>
        q;
    clearpq(q);
    q.emplace(0, seed);
    visited[seed_index] = true;
    thread_local std::vector<node_t> sorted;
    thread_local std::unordered_map<node_t, size_t> idx;
    idx.clear();
    for (size_t i = 0; i < s.size(); i++) idx[s[i]] = i;
    sorted.clear();
    level.clear();
    while (!q.empty()) {
      auto p = q.top();
      q.pop();
      node_t el = std::get<1>(p);
      int32_t lv = std::get<0>(p);
      sorted.push_back(el);
      level.push_back(lv);
      if (Neighs(el).size() < s.size()) {
        for (node_t n : Neighs(el)) {
          if (idx.count(n) && !visited[idx[n]]) {
            q.emplace(lv + 1, n);
            visited[idx[n]] = true;
          }
        }
      } else {
        for (size_t i = 0; i < s.size(); i++) {
          if (!visited[i] && IsNeigh(el, s[i])) {
            q.emplace(lv + 1, s[i]);
            visited[i] = true;
          }
        }
      }
    }
    s = sorted;
  }
};

#endif  // ENUMERABLE_CONNECTED_HEREDITARY
