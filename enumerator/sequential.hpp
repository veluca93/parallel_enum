#ifndef ENUMERATOR_SEQUENTIAL_H
#define ENUMERATOR_SEQUENTIAL_H

#include <stack>

#include "enumerator/enumerator.hpp"

template <typename Node, typename Item>
class Sequential : public Enumerator<Node, Item> {
 protected:
  void RunInternal(Enumerable<Node, Item>* system) override {
    system->SetUp();
    std::stack<Node> nodes;
    auto solution_cb = [this, &nodes, system](const Node& node) {
      nodes.push(node);
      return Enumerator<Node, Item>::ReportSolution(system, node);
    };
    if (system->ListRoots(solution_cb)) {
      while (!nodes.empty()) {
        Node&& node = std::move(nodes.top());
        nodes.pop();
        if (!system->ListChildren(node, solution_cb)) {
          break;
        }
      }
    }
    system->CleanUp();
  }
};
#endif
