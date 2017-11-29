#ifndef ENUMERATOR_PARALLEL_TBB_H
#define ENUMERATOR_PARALLEL_TBB_H

#include <math.h>
#include <tbb/task.h>
#include <tbb/task_scheduler_init.h>
#include <iostream>
#include <stack>
#include <vector>
#include "enumerator/enumerator.hpp"

template <typename Node, typename Item>

class ParallelTBB: public Enumerator<Node, Item> {
  /**
   * Version in which each task is related to more than one node
   */
  class ParallelListTask : public tbb::task {
   public:
    ParallelListTask(Enumerator<Node, Item>* enumerator,
                     Enumerable<Node, Item>* system, std::vector<Node>&& nodes)
        : _nodes(std::move(nodes)), _enumerator(enumerator), _system(system) {}

    // execute method required by tbb
    tbb::task* execute() {
      _system->SetUp();  // questa setup andrebbe fatta fiorifatta fuori

      std::deque<Node> _nodes_deque;
      auto solution_cb = [this, &_nodes_deque](const Node& node) {
        _nodes_deque.push_back(node);
        _enumerator->ReportSolution(_system, node);
        return true;
      };

      // NOTA: qui non possiamo avere le continuation visto che non sappiamo
      // quanti sono i figli
      tbb::task_list list;

      // Node nodecont = std::move(_nodes.front());
      //_nodes.pop_front();
      // tbb::empty_task *tcont= new (allocate_continuation()) tbb::empty_task;
      // //empty task
      for (Node _node : _nodes) {
        // get all children of the current node
        _system->ListChildren(_node, solution_cb);
      }

      // if we have children spawn task (by grouping a certain number of
      // nodes)

      int num_spawned_task =
          ceil((double)_nodes_deque.size() / max_nodes_per_task);
      this->set_ref_count(num_spawned_task + 1);
      while (!_nodes_deque.empty()) {
        // group tasks
        std::vector<Node> nodes_for_task;
        nodes_for_task.reserve(max_nodes_per_task);
        while (!_nodes_deque.empty() &&
               nodes_for_task.size() < max_nodes_per_task) {
          Node node = std::move(_nodes_deque.front());
          _nodes_deque.pop_front();
          nodes_for_task.push_back(node);
        }

        // spawn task...
        tbb::task* t = new (tbb::task::allocate_child())
            ParallelListTask(_enumerator, _system, std::move(nodes_for_task));
        spawn(*t);
        list.push_back(*t);
      }
      // spawn(list);
      tbb::task::wait_for_all();
      return this;
    }

    std::vector<Node> _nodes;

   private:
    const int max_nodes_per_task = 3;
    Enumerator<Node, Item>* _enumerator;
    Enumerable<Node, Item>* _system;
  };

  class ParallelTask : public tbb::task {
   public:
    ParallelTask(Enumerator<Node, Item>* enumerator,
                 Enumerable<Node, Item>* system, Node node)
        : _node(node), _enumerator(enumerator), _system(system) {}

    // execute method required by tbb
    tbb::task* execute() {
      _system->SetUp();  // questa setup andrebbe fatta fuori

      /*std::stack<Node> _nodes;
      auto solution_cb = [this, &_nodes](const Node& node) {
              _nodes.push(node);
              return _enumerator->ReportSolution(_system, node);
      };*/

      std::deque<Node> _nodes;
      auto solution_cb = [this, &_nodes](const Node& node) {
        _nodes.push_back(node);
        _enumerator->ReportSolution(_system, node);
        return true;
      };

      // tbb::task_list list;
      // get all children of the current node
      _system->ListChildren(_node, solution_cb);
      // if we have children spawn task
      /*this->set_ref_count(_nodes.size()+1);
      while(!_nodes.empty())
      {
              Node node = std::move(_nodes.top());
              _nodes.pop();

              //spawn task...
              tbb::task *t=new (tbb::task::allocate_child())
      ParallelTask(_enumerator,_system,node); spawn(*t);
              //list.push_back(*t);

      }
      //spawn(list);
      tbb::task::wait_for_all();
*/

      // continuation as child (per ridurre il numero di task generati)

      if (_nodes.size() == 0) return nullptr;
      int size = _nodes.size();

      Node nodecont = std::move(_nodes.front());
      _nodes.pop_front();
      tbb::empty_task* tcont =
          new (allocate_continuation()) tbb::empty_task;  // empty task
      tbb::task_list list;
      tcont->set_ref_count(size);  // this must be done here!!
      while (!_nodes.empty()) {
        Node node = std::move(_nodes.front());
        _nodes.pop_front();

        // spawn task...
        ParallelTask* t = new (tcont->allocate_child())
            ParallelTask(_enumerator, _system, node);
        //	spawn(*t);
        list.push_back(*t);
      }
      spawn(list);
      // printf("Nodes size: %d\n",_nodes.size());
      this->_node = nodecont;
      recycle_as_child_of(*tcont);
      return this;
    }

    Node _node;

   private:
    Enumerator<Node, Item>* _enumerator;
    Enumerable<Node, Item>* _system;
  };

 public:
  ParallelTBB(int nthreads) : _nthreads(nthreads), _task_scheduler(nthreads) {
    std::cout << "Parallel enumerator: running with " << _nthreads
              << " threads." << std::endl;
  }

 protected:
  void RunInternal(Enumerable<Node, Item>* system) override {
    system->SetUp();
    std::stack<Node> nodes;
    auto solution_root_cb = [this, &nodes, system](const Node& node) {
      nodes.push(node);
      Enumerator<Node, Item>::ReportSolution(system, node);
      return true;
    };

    tbb::task_list list;

    size_t max_roots = system->MaxRoots();
    for (size_t i = 0; i < max_roots; i++) {
      system->GetRoot(i, solution_root_cb);
    }

    while (!nodes.empty()) {
      // put all the root into a task list
      Node node = std::move(nodes.top());
      nodes.pop();
      //				std::vector<Node> nodes_for_task;
      //				nodes_for_task.push_back(node);
      //				tbb::task *t =new
      //(tbb::task::allocate_root())  ParallelListTask(this,system,
      // std::move(nodes_for_task));
      tbb::task* t =
          new (tbb::task::allocate_root()) ParallelTask(this, system, node);
      list.push_back(*t);
    }

    // spawn
    tbb::task::spawn_root_and_wait(list);
    _task_scheduler.terminate();
    system->CleanUp();
  }

  int _nthreads;                             // number of threads used
  tbb::task_scheduler_init _task_scheduler;  // needed to set par degree
};
#endif  // PARALLEL_HPP