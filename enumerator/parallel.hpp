#ifndef PARALLEL_HPP
#define PARALLEL_HPP

#include <iostream>
#include <stack>
#include <tbb/task_scheduler_init.h>
#include <tbb/task.h>
#define __TBB_STATISTICS 1
#include "enumerator/enumerator.hpp"

template <typename Node, typename Item>

class Parallel : public Enumerator<Node, Item> {

	class WarmupTask : public tbb::task{

	public:
		WarmupTask(Enumerable<Node, Item>* system) :_system(system){}

		tbb::task *execute()
		{
			_system->SetUp();
		}

	private:

		Enumerable<Node, Item>* _system;
	};


	class ParallelTask : public tbb::task {
	public:
		ParallelTask(Enumerator<Node, Item> *enumerator,Enumerable<Node, Item>* system,Node node):
			_node(node),_enumerator(enumerator),_system(system)
		{
		}

		//execute method required by tbb
		tbb::task *execute()
		{
			_system->SetUp();	//questa setup andrebbe fatta fiorifatta fuori

			std::stack<Node> _nodes;
			auto solution_cb = [this, &_nodes](const Node& node) {
				_nodes.push(node);
				return _enumerator->ReportSolution(_system, node);
			};

			//printf("Excute\n");

			//tbb::task_list list;
			//get all children of the current node
			if (_system->ListChildren(_node, solution_cb)) {
				//if we have children spawn task
			this->set_ref_count(_nodes.size()+1);	//TODO continuation??
				while(!_nodes.empty())
				{
					Node node = std::move(_nodes.top());
					_nodes.pop();

					//spawn task...
					tbb::task *t=new (tbb::task::allocate_child()) ParallelTask(_enumerator,_system,node);
					spawn(*t);
					//list.push_back(*t);

				}
				//spawn(list);
				tbb::task::wait_for_all();



				//TODO: spawnare task con piu' figli tutti assieme...


				//continuation as child (per ridurre il numero di task generati)
/*
				if(_nodes.size()==0) return nullptr;
				int size=_nodes.size();

				Node nodecont = std::move(_nodes.top());
				_nodes.pop();
				tbb::empty_task *tcont= new (allocate_continuation()) tbb::empty_task;	//empty task
				tcont->set_ref_count(size);			//this must be done here!!
				while(!_nodes.empty())
				{

					Node node = std::move(_nodes.top());
					_nodes.pop();

					//spawn task...
					ParallelTask *t=new (tcont->allocate_child()) ParallelTask(_enumerator,_system,node);
					//list.push_back(*t);
					spawn(*t);
				}
				//spawn(list);
				//printf("Nodes size: %d\n",_nodes.size());
				this->_node= nodecont;
				recycle_as_child_of(*tcont);
				return this;
*/

		}


			return nullptr;
		}

		Node _node;
	private:

		Enumerator<Node, Item>* _enumerator;
		Enumerable<Node, Item>* _system;

	};



public:
	Parallel(int nthreads) : _nthreads(nthreads), _task_scheduler(nthreads)
	{
		std::cout << "Parallel enumerator: running with " << _nthreads <<" threads."<<std::endl;

	}

protected:
	void RunInternal(Enumerable<Node, Item>* system) override {
		system->SetUp();
		std::stack<Node> nodes;
		auto solution_root_cb = [this, &nodes, system](const Node& node) {
			nodes.push(node);
			return Enumerator<Node, Item>::ReportSolution(system, node);
		};


		tbb::task_list list;

		if (system->ListRoots(solution_root_cb)) {

			while (!nodes.empty()) {

				//put all the root into a task list
				Node node = std::move(nodes.top());
				nodes.pop();

				tbb::task *t =new (tbb::task::allocate_root()) ParallelTask(this,system, node);
				list.push_back(*t);
			}
		}

		//spawn
		tbb::task::spawn_root_and_wait(list);
		_task_scheduler.terminate();
		system->CleanUp();
	}




	int _nthreads;									//number of threads used
	tbb::task_scheduler_init _task_scheduler;		//needed to set par degree
};
#endif // PARALLEL_HPP
