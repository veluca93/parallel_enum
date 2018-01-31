#ifndef ENUMERATOR_DISTRIBUTED_MPI_H
#define ENUMERATOR_DISTRIBUTED_MPI_H

#include <chrono>
#include <vector>
#include "third_party/mpi/mpi.h"
#include "enumerator/parallel_pthreads_steal.hpp"

using namespace std::chrono;
#undef DEBUG
#ifdef DEBUG_DISTRIBUTED_MPI
#define DEBUG(x) do { int rank; MPI_Comm_rank(MPI_COMM_WORLD, &rank); std::cerr << "[DistributedMPI][Rank " << rank << "]" << x << std::endl; } while (0)
#else
#define DEBUG(x)
#endif

typedef enum{
    TAG_STATS = 0, // Termination stats
    TAG_RANGE_REQUEST = 1, // Range requests (if chunksPerNode != 1)
}DistributedEnumTags;

static std::pair<size_t, size_t> GetNewRange(){
    MPI_Status status;
    int dummy;
    unsigned range[2];
    //MPI_Send(&dummy, 1, MPI_INT, 0, TAG_RANGE_REQUEST, MPI_COMM_WORLD);
    //MPI_Recv(range, 2, MPI_UNSIGNED, 0, TAG_RANGE_REQUEST, MPI_COMM_WORLD, &status);
    MPI_Sendrecv(&dummy, 1, MPI_INT, 0, TAG_RANGE_REQUEST, 
                 range, 2, MPI_UNSIGNED, 0, TAG_RANGE_REQUEST, MPI_COMM_WORLD, &status);
    std::pair<size_t, size_t> roots;
    roots.first = range[0];
    roots.second = range[1];
    DEBUG("Range [" << roots.first << ", " << roots.second << "] received. Error: " << status.MPI_ERROR);
    return roots;
}


static void WaitRangeRequest(size_t begin, size_t end){
    MPI_Status status;
    int dummy;
    MPI_Recv(&dummy, 1, MPI_INT, MPI_ANY_SOURCE, TAG_RANGE_REQUEST, MPI_COMM_WORLD, &status);
    unsigned range[2];
    range[0] = begin;
    range[1] = end;
    DEBUG("Received range request from " << status.MPI_SOURCE << " error: " << status.MPI_ERROR);
    DEBUG("Sending range [" << begin << ", " << end << "] to " << status.MPI_SOURCE);
    MPI_Send(range, 2, MPI_UNSIGNED, status.MPI_SOURCE, TAG_RANGE_REQUEST, MPI_COMM_WORLD);
}

template <typename Node, typename Item>
class DistributedMPI : public Enumerator<Node, Item> {
private:
    int _nthreads;
    int _rank;
    int _worldSize;
    int _chunksPerNode;
public:
    DistributedMPI(int nthreads, uint chunksPerNode): _nthreads(nthreads), _rank(0), _worldSize(0), _chunksPerNode(chunksPerNode){
        ;
    }
protected:
  void RunInternal(Enumerable<Node, Item>* system) override {
    std::chrono::high_resolution_clock::time_point startms = std::chrono::high_resolution_clock::now();
    // Divide potential roots by nodes
    system->SetUp();
    MPI_Init(NULL, NULL);
    // Find out rank, size
    MPI_Comm_rank(MPI_COMM_WORLD, &_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &_worldSize);
    // We are assuming at least 2 processes for this task
    if(_worldSize < 2){
        fprintf(stderr, "At least two computing nodes are needed to run the --distributed enumerator.\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    if(_rank == 0){
        std::cout << "Distributed enumerator (MPI): running with " <<
                     _worldSize << " nodes and " << _nthreads << " threads for " <<
                    " each node." << std::endl;
    }

    size_t maxRoots, rootsPerNode, nextRangeStart = 0, rangeEnd;
    int numWorkers;
    maxRoots = system->MaxRoots();
    if(_chunksPerNode > 1){
      std::thread* t = NULL;
      numWorkers = _worldSize - 1; // -1 because rank 0 doesn't perform computation
      if(_rank == 0){
        // Spawn thread
        t = new std::thread([&](){
          if(maxRoots < (size_t) (_chunksPerNode * numWorkers)){
            std::cerr << "WARNING: too high chunks_per_node. Set to maximum allowed for this graph (" << maxRoots / numWorkers << ")" << std::endl;
            rootsPerNode = maxRoots / numWorkers;
          }else{
            rootsPerNode = maxRoots / (_chunksPerNode * numWorkers); 
          }
#define LB_HEURISTIC
#ifdef LB_HEURISTIC
          nextRangeStart = maxRoots;
          while(nextRangeStart > 0){
            if(nextRangeStart >= rootsPerNode){
	      nextRangeStart -= rootsPerNode;
	      rangeEnd = nextRangeStart + rootsPerNode;
	    }else{
	      rangeEnd = nextRangeStart;
	      nextRangeStart = 0;
	    }
            int requester = WaitRangeRequest(nextRangeStart, rangeEnd);
            lastRequest[requester] = std::chrono::high_resolution_clock::now();
          }
#else
          while(nextRangeStart < maxRoots){
            rangeEnd = nextRangeStart + rootsPerNode;
            if(rangeEnd > maxRoots){
              rangeEnd = maxRoots;
            }
            int requester = WaitRangeRequest(nextRangeStart, rangeEnd);
            lastRequest[requester] = std::chrono::high_resolution_clock::now();
            nextRangeStart = rangeEnd;
          }
          assert(nextRangeStart == maxRoots);
#endif           
          size_t terminationsToSend = numWorkers;
          while(terminationsToSend){
            // If start >= maxRoots it means that there are no more roots.
            WaitRangeRequest(maxRoots, 0);
            --terminationsToSend;
          }
        });
      }

      if(_rank != 0){
        // Request the first batch of roots.
        std::pair<size_t, size_t> range = GetNewRange();
        DEBUG("Rank " << _rank << " processing between " << range.first << " and " << range.second);
        auto tmp = absl::make_unique<ParallelPthreadsSteal<Node, Item>>(_nthreads, range.first, range.second);
        NewRange nr = GetNewRange;
        tmp->SetMoreRootsCallback(&nr);
        tmp->Run(system);
        Enumerator<Node, Item>::solutions_found_ = tmp->GetSolutionsFound();
        DEBUG("Rank " << _rank << " terminated at " << time(NULL));
      }
  
      if(_rank == 0){
        t->join();
      }
    }else{
      // TODO We could remove this special case.
      numWorkers = _worldSize;
      rootsPerNode = maxRoots / numWorkers;
      size_t surplus = maxRoots % numWorkers;
      for(int i = 0; i < numWorkers; i++){
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
          DEBUG("Rank " << i << " pid " << getpid() << " terminated at " << time(NULL));
        }
        nextRangeStart = rangeEnd;
      }
      assert(rangeEnd == system->MaxRoots());
    }
    std::cout << "Rank " << _rank << " executed in " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startms).count() << " milliseconds." << std::endl;

    // Send solutions count.
    if(_rank){
      unsigned numSolutions = Enumerator<Node, Item>::solutions_found_;
      MPI_Send(&numSolutions, 1, MPI_UNSIGNED, 0, TAG_STATS, MPI_COMM_WORLD);
    }else{
      for(int i = 1; i < _worldSize; i++){
        unsigned numSolutions = 0;
        MPI_Recv(&numSolutions, 1, MPI_UNSIGNED, i, TAG_STATS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
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
