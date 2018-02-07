#ifndef ENUMERATOR_DISTRIBUTED_MPI_H
#define ENUMERATOR_DISTRIBUTED_MPI_H

#include <chrono>
#include <limits.h>
#include <stdint.h>
#include <vector>
#include "third_party/mpi/mpi.h"
#include "enumerator/parallel_pthreads_steal.hpp"

using namespace std::chrono;
#undef DEBUG
#ifdef DEBUG_DISTRIBUTED_MPI
#define DEBUG(x) do { int rank; MPI_Comm_rank(MPI_COMM_WORLD, &rank); std::cerr << "[DistributedMPI][" << (rank==0?"Master":("Worker " + std::to_string(rank - 1))) << "] " << x << std::endl; } while (0)
#else
#define DEBUG(x)
#endif

typedef enum{
  WORKER_STATE_ROOTS = 0,           // Normal execution, searching for roots
  WORKER_STATE_STEAL_WAIT,        // Roots terminated, waiting for a subtree
  WORKER_STATE_STEAL_PROCESSING,  // Roots terminated, processing a subtree
}WorkerState;

/**
 * The MPI message we send between nodes is an array of elements of type 'size_t'.
 * Let us call this array 'm'.
 *
 * m[0] is the message type.
 * - if m[0] == MPI_MESSAGE_RANGE_REQUEST, no other elements will be present.
 * - if m[0] == MPI_MESSAGE_RANGE_RESPONSE, m[1] is the first index of the range and m[2] is the last index (excluded).
 * - if m[0] == MPI_MESSAGE_STEAL_REQUEST, no other elements will be present.
 * - if m[0] == MPI_MESSAGE_STEAL_RESPONSE_LEN_ROOTS, m[1] is the length of the serialized data. If such message is received,
 *   the receiver should perform another receive after that to receive m[1] elements corresponding to the serialized roots chunk.
 * - if m[0] == MPI_MESSAGE_STEAL_RESPONSE_LEN_SUBTREE, m[1] is the length of the serialized data. If such message is received,
 *   the receiver should perform another receive after that to receive m[1] elements corresponding to the serialized subtree.
 * - if m[0] == MPI_MESSAGE_TERMINATION, no other elements will be present
 * - if m[0] == MPI_MESSAGE_STATS, m[1] will contain the cliques/kplexes count.
 **/
typedef enum{
  MPI_MESSAGE_RANGE_REQUEST = 0,
  MPI_MESSAGE_RANGE_RESPONSE,
  MPI_MESSAGE_STEAL_REQUEST,
  MPI_MESSAGE_STEAL_RESPONSE_LEN_ROOTS,
  MPI_MESSAGE_STEAL_RESPONSE_LEN_SUBTREE,
  MPI_MESSAGE_TERMINATION,
  MPI_MESSAGE_STATS
}MpiMessageType;

#if SIZE_MAX == UCHAR_MAX
   #define my_MPI_SIZE_T MPI_UNSIGNED_CHAR
#elif SIZE_MAX == USHRT_MAX
   #define my_MPI_SIZE_T MPI_UNSIGNED_SHORT
#elif SIZE_MAX == UINT_MAX
   #define my_MPI_SIZE_T MPI_UNSIGNED
#elif SIZE_MAX == ULONG_MAX
   #define my_MPI_SIZE_T MPI_UNSIGNED_LONG
#elif SIZE_MAX == ULLONG_MAX
   #define my_MPI_SIZE_T MPI_UNSIGNED_LONG_LONG
#else
   #error "what is happening here?"
#endif

static size_t RankToWorkerId(int rank){
  return rank - 1;
}

static int WorkerIdToRank(size_t workerId){
  return workerId + 1;
}

template <typename Node, typename Item>
class Master {
  Enumerable<Node, Item>* _system;
  size_t _numWorkers;
  int _chunksPerNode;
  std::vector<WorkerState> _workersStates;
  std::vector<std::chrono::high_resolution_clock::time_point> _lastRequest;
public:
  Master(Enumerable<Node, Item>* system, size_t numWorkers, int chunksPerNode):
    _system(system), _numWorkers(numWorkers), _chunksPerNode(chunksPerNode){;}

  // Returns the worker id of the victim
  size_t GetStealVictim(){
    std::chrono::high_resolution_clock::time_point minimum = std::chrono::time_point<std::chrono::system_clock>::max();
    size_t toReturn = 0;
    for(size_t i = 0; i < _lastRequest.size(); i++){
      if(_lastRequest[i] < minimum && _workersStates[i] != WORKER_STATE_STEAL_WAIT){
        minimum = _lastRequest[i];
        toReturn = i;
      }
    }
    return toReturn;
  }

  bool terminated(){
    for(size_t i = 0; i < _numWorkers; i++){
      if(_workersStates[i] != WORKER_STATE_STEAL_WAIT){
        return false;
      }
    }
    return true;
  }

  // Returns true if everyone terminated.
  bool WaitRangeRequest(size_t begin, size_t end){
    MPI_Status status;
    size_t req;
    MPI_Recv(&req, 1, my_MPI_SIZE_T, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
    assert(req == MPI_MESSAGE_RANGE_REQUEST);
    size_t stealerRank = status.MPI_SOURCE;
    int stealerId = RankToWorkerId(status.MPI_SOURCE);
    _lastRequest[stealerId] = std::chrono::high_resolution_clock::now();
    
    if(begin != _system->MaxRoots()){
      // Send a root
      size_t range[3];
      range[0] = MPI_MESSAGE_RANGE_RESPONSE;
      range[1] = begin;
      range[2] = end;
      MPI_Send(range, 3, my_MPI_SIZE_T, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
    }else{
      _workersStates[stealerId] = WORKER_STATE_STEAL_WAIT;
      bool somethingSent = false;

      while(!somethingSent && !terminated()){
        // Take something from the victim
        int victimId = GetStealVictim();
        int victimRank = WorkerIdToRank(victimId);
        assert(victimId != stealerId);
        DEBUG(stealerId << " needs something and we try to steal from " << victimId);

        size_t dummy = MPI_MESSAGE_STEAL_REQUEST;
        MPI_Send(&dummy, 1, my_MPI_SIZE_T, victimRank, 0, MPI_COMM_WORLD);

        size_t response[2];
        MPI_Recv(response, 2, my_MPI_SIZE_T, victimRank, 0, MPI_COMM_WORLD, &status);
        
        if(response[0] == MPI_MESSAGE_STEAL_RESPONSE_LEN_SUBTREE ||
           response[0] == MPI_MESSAGE_STEAL_RESPONSE_LEN_ROOTS){
          // If subtree present, receive it.
          size_t subtreeLength = response[1];
          size_t* subtree = (size_t*) malloc(sizeof(size_t) * subtreeLength);
          MPI_Recv(subtree, subtreeLength, my_MPI_SIZE_T, victimRank, 0, MPI_COMM_WORLD, &status);
          size_t range[2];
          range[0] = response[0];
          range[1] = subtreeLength;

          DEBUG("Worker " << victimId << " provided a subtree serialized with an array of length " << subtreeLength);
          MPI_Send(range, 2, my_MPI_SIZE_T, stealerRank, 0, MPI_COMM_WORLD);
          // Send the subtree to the worker which requested the range
          MPI_Send(subtree, subtreeLength, my_MPI_SIZE_T, stealerRank, 0, MPI_COMM_WORLD); 
          free(subtree);
          somethingSent = true;
          DEBUG("Subtree sent");
          _workersStates[stealerId] = WORKER_STATE_STEAL_PROCESSING;
        }else if(response[0] == MPI_MESSAGE_RANGE_REQUEST){
          DEBUG("The victim needs some data itself, we skip it (" << victimId << ")");
          _workersStates[victimId] = WORKER_STATE_STEAL_WAIT;
          _lastRequest[victimId] = std::chrono::high_resolution_clock::now();
        }else{
          throw std::runtime_error("Unexpected message.");
        }
      }

      if(terminated()){
        DEBUG("Broadcasting termination message.");
        size_t termination = MPI_MESSAGE_TERMINATION;
        for(size_t i = 0; i < _numWorkers; i++){
          MPI_Send(&termination, 1, my_MPI_SIZE_T, WorkerIdToRank(i), 0, MPI_COMM_WORLD);
        }
        return true;
      }
    }
    return false;
  }

  void Run(){
    size_t maxRoots = _system->MaxRoots();
    size_t rootsPerNode, nextRangeStart = 0, rangeEnd;
    _lastRequest.resize(_numWorkers);
    for(size_t i = 0; i < _numWorkers; i++){
      _workersStates.push_back(WORKER_STATE_ROOTS);
    }
    if(maxRoots < (size_t) (_chunksPerNode * _numWorkers)){
      std::cerr << "WARNING: too high chunks_per_node. Set to maximum allowed for this graph (" << maxRoots / _numWorkers << ")" << std::endl;
      rootsPerNode = maxRoots / _numWorkers;
    }else{
      rootsPerNode = maxRoots / (_chunksPerNode * _numWorkers); 
    }
#ifdef INVERTED_SCHEDULING
    nextRangeStart = maxRoots;
    while(nextRangeStart > 0){
      if(nextRangeStart >= rootsPerNode){
        nextRangeStart -= rootsPerNode;
        rangeEnd = nextRangeStart + rootsPerNode;
      }else{
        rangeEnd = nextRangeStart;
        nextRangeStart = 0;
      }
      DEBUG("Sending range starting at " << nextRangeStart);
      WaitRangeRequest(nextRangeStart, rangeEnd);
    }
#else
    while(nextRangeStart < maxRoots){
      rangeEnd = nextRangeStart + rootsPerNode;
      if(rangeEnd > maxRoots){
        rangeEnd = maxRoots;
      }
      DEBUG("Sending range starting at " << nextRangeStart);
      WaitRangeRequest(nextRangeStart, rangeEnd);
      nextRangeStart = rangeEnd;
    }
    assert(nextRangeStart == maxRoots);
#endif
    // Wait for termination while performing stealing.
    while(!WaitRangeRequest(maxRoots, 0)){
      ;
    }
  }
};

template <typename Node, typename Item>
class Worker {
  Enumerable<Node, Item>* _system;
  int _nthreads;
  int _rank;
public:
  Worker(Enumerable<Node, Item>* system, int nthreads, int rank):_system(system), _nthreads(nthreads), _rank(rank){;}

  MoreWorkData GetMoreWork(){
    MoreWorkData mwd;
    MPI_Status status;
    size_t req = MPI_MESSAGE_RANGE_REQUEST; 

    // In a loop to discard the possible steal request received while asking for more data.    
    size_t response[3];
    response[0] = MPI_MESSAGE_STEAL_REQUEST;
    while(response[0] == MPI_MESSAGE_STEAL_REQUEST){
      MPI_Send(&req, 1, my_MPI_SIZE_T, 0, 0, MPI_COMM_WORLD);
      MPI_Recv(response, 3, my_MPI_SIZE_T, 0, 0, MPI_COMM_WORLD, &status);

      if (response[0] == MPI_MESSAGE_STEAL_RESPONSE_LEN_ROOTS || 
          response[0] == MPI_MESSAGE_STEAL_RESPONSE_LEN_SUBTREE) {
        // range[1] is the length of the subtree I'm going to receive.
        // Now I'll receive a subtree since roots are finished.
        size_t subtreeLength = response[1];
        size_t* subtree = (size_t*) malloc(sizeof(size_t) * subtreeLength);
        MPI_Recv(subtree, subtreeLength, my_MPI_SIZE_T, 0, 0, MPI_COMM_WORLD, &status);
        if (response[0] == MPI_MESSAGE_STEAL_RESPONSE_LEN_ROOTS) {
          mwd.info = MORE_WORK_ROOTS;
        } else {
          mwd.info = MORE_WORK_SUBTREE;
        }
        mwd.subtree = subtree;
        mwd.subtreeLength = subtreeLength;      
      } else if(response[0] == MPI_MESSAGE_RANGE_RESPONSE) {
        mwd.info = MORE_WORK_RANGE;
        mwd.range.first = response[1];
        mwd.range.second = response[2];
      } else if(response[0] == MPI_MESSAGE_TERMINATION) {
        DEBUG("Received termination message.");
        mwd.info = MORE_WORK_NOTHING;
      } else if(response[0] != MPI_MESSAGE_STEAL_REQUEST) {
        throw std::runtime_error("Unexpected message.");
      }
    }
    return mwd;
  }

  /**
   * Checks if a steal request was present. If so, passes
   * the node nodeId to the stealer.
   * @param nodeId The node which could be stolen.
   * @return true if the node was stolen, false otherwise.
   **/
  bool CheckStealRequest(){  
    MPI_Status status;
    int flag;
    MPI_Iprobe(0, 0, MPI_COMM_WORLD, &flag, &status);
    if(flag){
      return true;
    }else{
      return false;
    }
  }

  ssize_t Run(){
    // Request the first batch of roots.
    MoreWorkData mwd = GetMoreWork();
    assert(mwd.info == MORE_WORK_RANGE);
    auto tmp = absl::make_unique<ParallelPthreadsSteal<Node, Item>>(_nthreads, mwd.range.first, mwd.range.second);
    MoreWork mr = std::bind(&Worker::GetMoreWork, this);
    CheckSteal cs = std::bind(&Worker::CheckStealRequest, this);
    SendSteal ss = [&](std::vector<size_t>& serializedData, bool areRoots){
      size_t dummy;
      MPI_Status status;
      MPI_Recv(&dummy, 1, my_MPI_SIZE_T, 0, 0, MPI_COMM_WORLD, &status);
      assert(dummy == MPI_MESSAGE_STEAL_REQUEST);
      unsigned subtreeLength = serializedData.size();
      size_t msg[2];
      if (areRoots) {
        msg[0] = MPI_MESSAGE_STEAL_RESPONSE_LEN_ROOTS;
      } else {
        msg[0] = MPI_MESSAGE_STEAL_RESPONSE_LEN_SUBTREE;
      }
      msg[1] = subtreeLength;
      MPI_Send(msg, 2, my_MPI_SIZE_T, 0, 0, MPI_COMM_WORLD);
      MPI_Send(serializedData.data(), subtreeLength, my_MPI_SIZE_T, 0, 0, MPI_COMM_WORLD);
    };
    tmp->SetCallbacks(&mr, &cs, &ss);
    tmp->Run(_system);
    return tmp->GetSolutionsFound();
  }
};

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
                     _worldSize - 1 << " workers and " << _nthreads << " threads for " <<
                    " each node." << std::endl;
    }

    int numWorkers;
    if(_chunksPerNode > 1){
      numWorkers = _worldSize - 1; // -1 because rank 0 doesn't perform computation
      if(_rank == 0){
        /**********************************/
        /*            Master              */
        /**********************************/
        Master<Node, Item> m(system, numWorkers, _chunksPerNode);
        m.Run();
      }else{
        /**********************************/
        /*           Workers              */
        /**********************************/
        Worker<Node, Item> w(system, _nthreads, _rank);
        Enumerator<Node, Item>::solutions_found_ = w.Run();
      }
    }else{
      // _chunksPerNode <= 1
      // TODO We could remove this special case.
      size_t maxRoots = system->MaxRoots();
      size_t nextRangeStart = 0, rangeEnd;
      numWorkers = _worldSize;
      size_t rootsPerNode = maxRoots / numWorkers;
      size_t surplus = maxRoots % numWorkers;
      for(int i = 0; i < numWorkers; i++){
        rangeEnd = nextRangeStart + rootsPerNode;
        if(surplus){
          ++rangeEnd;
          --surplus;
        }
        // If I am this one..
        if(i == _rank){
          auto tmp = absl::make_unique<ParallelPthreadsSteal<Node, Item>>(_nthreads, nextRangeStart, rangeEnd);
          tmp->Run(system);
          Enumerator<Node, Item>::solutions_found_ = tmp->GetSolutionsFound();
        }
        nextRangeStart = rangeEnd;
      }
      assert(rangeEnd == system->MaxRoots());
    }
    std::cout << "Rank " << _rank << " executed in " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startms).count() << " milliseconds." << std::endl;

    // Send solutions count.
    if(_rank){
      size_t message[2];
      message[0] = MPI_MESSAGE_STATS;
      message[1] = Enumerator<Node, Item>::solutions_found_;
      MPI_Send(message, 2, my_MPI_SIZE_T, 0, 0, MPI_COMM_WORLD);
    }else{
      for(int i = 1; i < _worldSize; i++){
        size_t message[2];
        do{
          MPI_Recv(message, 2, my_MPI_SIZE_T, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }while(message[0] != MPI_MESSAGE_STATS); // In a loop to clean possible messages not yet received.
        size_t numSolutions = message[1];
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
