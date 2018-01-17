#ifndef ENUMERATOR_PARALLEL_TBB2_H
#define ENUMERATOR_PARALLEL_TBB2_H

#include <math.h>
#include <tbb/task.h>
#include <tbb/task_scheduler_init.h>
#include <iostream>
#include <stack>
#include <vector>
#include "enumerator/enumerator.hpp"

template <typename Node, typename Item>

class ParallelTBB: public Enumerator<Node, Item> {

	class ParallelTask : public tbb::task {
	public:
		ParallelTask(Enumerator<Node, Item>* enumerator,
					 Enumerable<Node, Item>* system, Node node)
			: _node(node), _enumerator(enumerator), _system(system) {}

		// execute method required by tbb
		tbb::task* execute() {
			_system->SetUp();  // questa setup andrebbe fatta fuori


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
	/*		this->set_ref_count(_nodes.size()+1);
			while(!_nodes.empty())
			{

				Node node = std::move(_nodes.front());
				_nodes.pop_front();

				//spawn task...
				tbb::task *t=new (tbb::task::allocate_child()) ParallelTask(_enumerator,_system,node);
				spawn(*t);
				//list.push_back(*t);

			}
			//spawn(list);
			tbb::task::wait_for_all();
			return nullptr;
*/

			// continuation as child (to reduce the number of generated tasks)

			if (_nodes.size() == 0) return nullptr;
			int size = _nodes.size();

			Node nodecont = std::move(_nodes.back());
			_nodes.pop_back();
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
			this->_node = nodecont;
			recycle_as_child_of(*tcont);
			return this;

		}

		Node _node;

	private:
		Enumerator<Node, Item>* _enumerator;
		Enumerable<Node, Item>* _system;
	};



	class ParallelRootTask : public tbb::task {
	public:

		ParallelRootTask(Enumerator<Node, Item>* enumerator, Enumerable<Node, Item>* system):
			_enumerator(enumerator), _system(system)
		{
		}

		// execute method required by tbb
		tbb::task* execute()
		{

			int count=0;
			//spawn a task for each root
			auto solution_root_cb = [this, &count](const Node& node) {

				//				//spawn task
				this->add_ref_count(1);
				tbb::task *t=new (tbb::task::allocate_child()) ParallelTask(_enumerator,_system,node);
				spawn(*t);
				count++;
				_enumerator->ReportSolution(_system, node);
				return true;
			};

			size_t max_roots = _system->MaxRoots();
			this->set_ref_count(1);

			for (size_t i = 0; i < max_roots; i++) {
				_system->GetRoot(i, solution_root_cb);
			}



			//decrease ref_count
			/*for(int i=count;i<max_roots;i++)
				this->decrement_ref_count();
*/
			tbb::task::wait_for_all();


			return nullptr;
		}


	private:
		Enumerator<Node, Item>* _enumerator;
		Enumerable<Node, Item>* _system;
	};

public:
	ParallelTBB(int nthreads) : _nthreads(nthreads), _task_scheduler(nthreads) {
		std::cout << "TBB: Parallel enumerator: running with " << _nthreads
				  << " threads." << std::endl;
	}

protected:
	void RunInternal(Enumerable<Node, Item>* system) override {
		system->SetUp();

		tbb::task *t =new (tbb::task::allocate_root())  ParallelRootTask(this,system);





		tbb::task::spawn_root_and_wait(*t);
		_task_scheduler.terminate();
		system->CleanUp();
	}

	int _nthreads;                             // number of threads used
	tbb::task_scheduler_init _task_scheduler;  // needed to set par degree
};
#endif  // PARALLEL_HPP
