#ifndef ENUMERATOR_PARALLEL_PTHREADS_H
#define ENUMERATOR_PARALLEL_PTHREADS_H

#include <vector>
#include "third_party/mpi/mpi.h"

template <typename Node, typename Item>
class DistributedMPI : public Enumerator<Node, Item> {
private:
    int _nthreads;
public:
    DistributedMPI(int nthreads): _nthreads(nthreads){
        ;
    }
protected:
  void RunInternal(Enumerable<Node, Item>* system) override {
    // Divide potential roots by nodes
    system->SetUp();
    MPI_Init(NULL, NULL);
    // Find out rank, size
    int worldRank;
    MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);
    int worldSize;
    MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
    // We are assuming at least 2 processes for this task
    if (worldSize < 2) {
      fprintf(stderr, "At least two computing nodes are needed to run the --distributed enumerator.");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }

    std::cout << "Distributed enumerator (MPI): running with " <<
                 worldSize << " nodes and " << _nthreads << " threads for " <<
                " each node." << std::endl;

    size_t maxRoots = system->MaxRoots();
    size_t rootsPerNode = maxRoots / worldSize;
    size_t surplus = maxRoots % worldSize;
    size_t nextRangeStart = 0;
    size_t rangeEnd;
    for(int i = 0; i < worldSize; i++){
        rangeEnd = nextRangeStart + rootsPerNode;
        if(surplus){
            ++rangeEnd;
            --surplus;
        }
        // If I am this one..
        if(i == worldRank){
            auto tmp = absl::make_unique<ParallelPthreadsSteal<Node, Item>>(_nthreads, nextRangeStart, rangeEnd);
            tmp->Run(system);
        }
        nextRangeStart = rangeEnd;
    }
    assert(rangeEnd = system->MaxRoots());
    system->CleanUp();
    MPI_Finalize();
  }
};
#endif // ENUMERATOR_PARALLEL_PTHREADS_H
