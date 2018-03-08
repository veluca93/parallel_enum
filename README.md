# BC-Cliques
This branch holds the code to enumerate black connected cliques in the
graph obtained by computing the product between two labeled graphs.

While the interface takes as an input two graphs, this structure is never
explicitely relied on in the enumeration algorithm. As such, the code can
be easily modified to take as an input an arbitrary graph with two kinds
of edge labels.

To run the code, you should compile it with

```
bazel build -c opt ...
```

and then run it as follows:

```
./bazel-bin/ui/text_ui --system bcclique --graph_format oly g1.txt g2.txt
```

Both files `g1.txt` and `g2.txt` should be in an IOI-like format: the first
line should contain two numbers, the number of node and edges of the graph
respectively, the second line should contain the `N` labels of the vertices,
and the following `M` lines should contain a pair of numbers `i` and `j`,
meaning that there is an edge between node `i` and node `j`.

## Code location
The code that is relevant to the enumeration of BC-Cliques can be found
in the `enumerable/bccliques.hpp`, `enumerable/connected_hereditary.hpp` and 
`enumerable/commutable.hpp` files. In particular, these files contain the
necessary code to list the roots of the computational forest and to list the
children of a given node in this forest.

## Parallel and distributed version
To run the parallel and the distributed versions of the code, please reference
the `README.md` file in the master branch of this repository.
