#ifndef ENUMERATOR_DISTRIBUTED_MPI_H
#define ENUMERATOR_DISTRIBUTED_MPI_H

#include <vector>
#include "third_party/mpi/mpi.h"
#include "enumerator/parallel_pthreads_steal.hpp"

#undef DEBUG
#ifdef DEBUG_DISTRIBUTED_MPI
#define DEBUG(x) do { std::cerr << "[DistributedMPI] " << x << std::endl; } while (0)
#else
#define DEBUG(x)
#endif

template <typename Node, typename Item>
class DistributedMPI : public Enumerator<Node, Item> {
private:
    int _nthreads;
    int _rank;
    int _worldSize;
public:
    DistributedMPI(int nthreads): _nthreads(nthreads), _rank(0), _worldSize(0){
        ;
    }
protected:
  void RunInternal(Enumerable<Node, Item>* system) override {
    // Divide potential roots by nodes
    system->SetUp();
    MPI_Init(NULL, NULL);
    // Find out rank, size
    MPI_Comm_rank(MPI_COMM_WORLD, &_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &_worldSize);
    // We are assuming at least 2 processes for this task
    if (_worldSize < 2) {
      fprintf(stderr, "At least two computing nodes are needed to run the --distributed enumerator.\n");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }

    if(_rank == 0){
        std::cout << "Distributed enumerator (MPI): running with " <<
                     _worldSize << " nodes and " << _nthreads << " threads for " <<
                    " each node." << std::endl;
    }

    size_t maxRoots = system->MaxRoots();
    DEBUG("MaxRoots: " << maxRoots);
    size_t rootsPerNode = maxRoots / _worldSize;
    size_t surplus = maxRoots % _worldSize;
    size_t nextRangeStart = 0;
    size_t rangeEnd;
    for(int i = 0; i < _worldSize; i++){
        rangeEnd = nextRangeStart + rootsPerNode;
        if(surplus){
            ++rangeEnd;
            --surplus;
        }
        // If I am this one..
        if(i == _rank){
            DEBUG("Rank " << i << " pid " << getpid() << " processing between " << nextRangeStart << " and " << rangeEnd);
            auto tmp = absl::make_unique<ParallelPthreadsSteal<Node, Item>>(_nthreads, nextRangeStart, rangeEnd);
            tmp->Run(system);
            Enumerator<Node, Item>::solutions_found_ = tmp->GetSolutionsFound();
        }
        nextRangeStart = rangeEnd;
    }
    assert(rangeEnd = system->MaxRoots());

    // Send solutions count.
    if(_rank){
        ssize_t numSolutions = Enumerator<Node, Item>::solutions_found_;
        MPI_Send(&numSolutions, 1, MPI_UNSIGNED, 0, 0, MPI_COMM_WORLD);
    }else{
        for(int i = 1; i < _worldSize; i++){
            ssize_t numSolutions;
            MPI_Recv(&numSolutions, 1, MPI_UNSIGNED, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            Enumerator<Node, Item>::solutions_found_ += numSolutions;
        }
    }

    system->CleanUp();
    MPI_Finalize();
  }

  void PrintStats(FILE* out = stdout) override {
    if(_rank == 0){
        Enumerator<Node, Item>::PrintStats(out);
    }
  }
};
#endif // ENUMERATOR_DISTRIBUTED_MPI_H
