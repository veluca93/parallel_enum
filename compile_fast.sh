#!/bin/bash

PROFILE_WITH="--system d2kplex  --k 2 --q 100 ../interdom.nde"

SRCS="./ui/text_ui.cpp ./permute/permute.cpp ./enumerable/clique.cpp ./enumerable/ckplex.cpp ./enumerable/diam2kplex.cpp ./util/graph.cpp ./util/cuckoo.cpp ./util/fastio.cpp ./util/dynarray.cpp ./util/binary_search.cpp"

GFLAG_SRCS="./bazel-parallel_enum/external/com_github_gflags_gflags/src/gflags.cc ./bazel-parallel_enum/external/com_github_gflags_gflags/src/gflags_reporting.cc ./bazel-parallel_enum/external/com_github_gflags_gflags/src/gflags_completions.cc"

OPTS="-flto -O3 -DNDEBUG -pthread -g -std=c++11 -march=native -ltcmalloc -fdevirtualize-at-ltrans -fdevirtualize-speculatively -fipa-pta -fipa-icf -floop-block -ftree-vectorize"

INCLUDES="$(cat .syntastic_cpp_config | xargs echo) -Ibazel-genfiles/external/com_github_gflags_gflags/"

OTHER="-DHAVE_STDINT_H -DHAVE_SYS_TYPES_H -DHAVE_INTTYPES_H -DHAVE_SYS_STAT_H -DHAVE_UNISTD_H -DHAVE_FNMATCH_H -DHAVE_STRTOLL -DHAVE_STRTOQ -DHAVE_PTHREAD -DHAVE_RWLOCK -DGFLAGS_INTTYPES_FORMAT_C99"

OUTPUT=parallel_enum

TEMP=$(mktemp)

set -x

g++ -fprofile-generate ${OPTS} ${SRCS} ${GFLAG_SRCS} ${INCLUDES} ${OTHER} -o $TEMP

$TEMP $PROFILE_WITH

g++ -fprofile-use ${OPTS} ${SRCS} ${GFLAG_SRCS} ${INCLUDES} ${OTHER} -o $OUTPUT
