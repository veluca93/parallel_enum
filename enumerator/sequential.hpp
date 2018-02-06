#ifndef ENUMERATOR_SEQUENTIAL_H
#define ENUMERATOR_SEQUENTIAL_H

#include <stack>

#include "enumerator/enumerator.hpp"

template <typename Node, typename Item>
class Sequential : public Enumerator<Node, Item> {
 protected:
  void RunInternal(Enumerable<Node, Item>* system) override {
    system->SetUp();
    size_t max_roots = system->MaxRoots();
    if (!system->CanUseRecursion()) {
      std::stack<Node> nodes;
      auto solution_cb = [this, &nodes, system](const Node& node) {
        nodes.push(node);
        Enumerator<Node, Item>::ReportSolution(system, node);
        return true;
      };

      for (size_t i = 0; i < max_roots; i++) {
        system->GetRoot(i, solution_cb);
        while (!nodes.empty()) {
          Node node = std::move(nodes.top());
          nodes.pop();
          system->ListChildren(node, solution_cb);
        }
      }
    } else {
      std::function<bool(const Node&)> solution_cb =
          [this, system, &solution_cb](const Node& node) {
            Enumerator<Node, Item>::ReportSolution(system, node);
            system->ListChildren(node, solution_cb);
            return true;
          };
      for (size_t i = 0; i < max_roots; i++) {
        system->GetRoot(i, solution_cb);
      }
    }
    system->CleanUp();
  }
};
#endif
