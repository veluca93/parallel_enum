#ifndef ENUMERABLE_ENUMERABLE_H
#define ENUMERABLE_ENUMERABLE_H
#include <functional>

// This class represent a family that is enumerable by doing
// visit on a directed forest. Implementations should ensure
// that calls to ListRoots and ListChildren are thread-safe, but
// may assume that they are not recursive.
template <typename Node, typename Item>
class Enumerable {
 public:
  // This function is called for every item found. It stops the
  // function that calls it if it returns false.
  using NodeCallback = std::function<bool(const Node&)>;

  // This function should be called before calling either
  // ListRoots or ListChildren.
  virtual void SetUp() {}

  // This function should be called after all calls to
  // ListRoots and ListChildren are done to perform final
  // clean ups.
  virtual void CleanUp() {}

  // The maximum possible number of roots.
  virtual size_t MaxRoots() = 0;

  // Gets the i-th root, if it exists.
  virtual void GetRoot(size_t i, const NodeCallback& cb) = 0;

  // Lists the children of a given node.
  virtual void ListChildren(const Node& node, const NodeCallback& cb) = 0;

  // Converts an enumeration node into an output item.
  virtual Item NodeToItem(const Node& node) = 0;

  // Returns true if the node of the computational forest represents
  // a solution.
  virtual bool IsSolution(const Node& node) { return true; }
};

#endif  // ENUMERABLE_ENUMERABLE_H
