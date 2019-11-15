# Building
To build the project, just run the command ```./build.sh```. It will ask if you
want to build the project with the support for distributed memory, for example
to run the code on a cluster of homogeneous interconnected computing nodes. If
this is the case, please read the ["MPI"](#mpi) section, otherwise you jump
directly to the [Using](#using) section.

## MPI
MPI is the de-facto standard for implementing high performance applications
running on a cluster of computing nodes. If you need to run the code on a
cluster, while building the project you need to specify the path where MPI is
installed. The build script will try to infer by itself the path where MPI is
installed. If this path is wrong, you need to specify it manually. If you do
not have MPI already installed on your machine, please install it before
running the build script, for example by following
[this](https://www.open-mpi.org/faq/?category=building#easy-build) guide. The
script assumes that the folder you specified contains one  ```include```
subfolders containing the MPI headers and one ```lib``` subfolder containing
the MPI library. If this is not the case, you may need to modify either the
folder structure or the ```build.py``` script. The code was only tested with
OpenMPI but should work with other MPI installations as well.

# Graph format

By default, the application accepts graphs in the NDE (Nodes-Degrees-Edges)
format.  The graph file should have .nde extension and contain the following
information:

- The first line should contain the number of nodes.
- Then, for each node, one line containing the index of the node and its degree.
- Then, for each edge, one line containing the indices of its extremes.

# Using
To execute the code, you need to run the following executable file:

```
./build/text_ui graph.nde
```

This executable accepts the following optional parameters:
- -system: What should be enumerated. This can assume one of the following values:
    - d2kplex: Default value. To enumerate maximal, diameter-2 k-plexes (these include all maximal k-plexes of size at least 2k-1).
    - clique: To enumerate maximal cliques.

- -enumerator: This can assume one of the following values:
    - sequential: To run the code sequentially. This is the default value.
    - parallel: To run the code in parallel on a multicore computing node.
    - distributed: To run the code on a cluster of multicore computing nodes. If you specify this value, please refer to the [Using with MPI](#using-with-mpi) section for more information on how to run the code.

- -k: This is the k value for k-plex enumeration. By default it is set to 2.

- -q: This is the minimum size of the d2kplexes to be found. By default it is set to 1.

- -n: Number of cores to use when enumerator=parallel. By default this is set to the number of cores available on the computing node.

- -graph_format: The format of the graph provided in input. It can assume one of the following values:
    - nde: This is the default format. See the description above in Section [Graph format](#graph-format)
    - oly: One line containing the number of nodes and the number of edges, then one line for each edge containing the indices of its extremes

- -fast_graph: Use the faster but more memory hungry graph format. By default it is set to true.

- -huge_graph: Use 64 bit integers to count nodes. By default it is set to false.

- -one_based: Whether the graph is one based. Used only by oly format. By default it is set to false.

- -quiet: Do not show any non-fatal output. By default it is set to false.

- -enable_pivoting: Enables pivoting in d2kplex. By default it is set to true.


For example, to count 3-plexes bigger than 2 on the 'graph.nde' graph, in parallel by using 10 cores, you should execute the following command:

```
./build/text_ui -k 3 -q 2 -enumerator="parallel" -n 10 graph.nde
```


## Using with MPI
To run this code on a cluster of nodes, first of all we assume that these nodes
are using NFS so that all of them have the same directory structure. Before
executing the code, you need to specify a ```hosts.txt``` file. For example,
supposing that you have 32 nodes (from node00 to node031), the file should look
like the following one:

```
node00 slots=1 max-slots=1
node01 slots=1 max-slots=1
node02 slots=1 max-slots=1
...
node030 slots=1 max-slots=1
node031 slots=1 max-slots=1
```

Basically, we have one row for each computing node. Each row has three space
separated fields. The first field is the hostname of the node, while the second
and third fields force MPI to run only one process for each node. This is
needed to avoid oversubscription of the node, since each process will spawn
multiple threads (a number of threads equal to the one specified through the
```-n``` parameter).

> ATTENTION: This is the format for OpenMPI installations. Other MPI versions
> should have a similar format. Please refer to the corresponding user manual
> to check how to force only one process for each node.

After you built the ```hosts.txt``` file, you need to run the application by
using the ```mpiexec``` tool.



For example, to count 3-plexes bigger than 2 on the 'graph.nde' graph, by using
15 nodes of the cluster, each of which uses 10 cores, you should execute the
following command:

```
mpiexec --hostfile hosts.txt -np 15 ./build/text_ui -k 3 -q 2 -enumerator="distributed" -n 10 graph.nde
```

In a nutshell, you need to run the same command you would run in the
sequential/parallel case, by replacing the ```distributed``` value for the
```enumerator``` flag and by prepending the command with ```mpiexec --hostfile
hosts.txt -np 15``` where the value of the ```-np``` flag is the number of
computing nodes you want to use.




# Macros
The following macros can be specified at compile time, but are mostly for internal/debug use.
- DEGENERACY: If specified, graph is permuted in degeneracy ordering
- PARALLEL_NOPIN: Doesn't perform threads pinning
- PRINT_PROGRESS: Once per second prints the current number of cliques/d2kplexes found
