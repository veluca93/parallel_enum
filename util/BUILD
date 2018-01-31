package(default_visibility = ["//visibility:public"])

cc_library(
    name = "concurrentqueue",
    hdrs = ["concurrentqueue.hpp"],
)

cc_library(
    name = "cuckoo",
    srcs = ["cuckoo.cpp"],
    hdrs = ["cuckoo.hpp"],
)

cc_library(
    name = "fastio",
    srcs = ["fastio.cpp"],
    hdrs = ["fastio.hpp"],
)

cc_library(
    name = "graph",
    srcs = ["graph.cpp"],
    hdrs = ["graph.hpp"],
    deps = [
        ":binary_search",
        ":cuckoo",
        ":dynarray",
        ":fastio",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "serialize",
    hdrs = ["serialize.hpp"],
    deps = ["@com_google_absl//absl/memory"],
)

cc_library(
    name = "bitset",
    hdrs = ["bitset.hpp"],
)

cc_library(
    name = "binary_search",
    srcs = ["binary_search.cpp"],
    hdrs = ["binary_search.hpp"],
    deps = [":dynarray"],
)

cc_library(
    name = "dynarray",
    srcs = ["dynarray.cpp"],
    hdrs = ["dynarray.hpp"],
)
