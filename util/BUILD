package(default_visibility = ["//visibility:public"])

cc_library(
    name = "cuckoo",
    hdrs = ["cuckoo.hpp"],
)

cc_library(
    name = "fastio",
    srcs = ["fastio.cpp"],
    hdrs = ["fastio.hpp"],
)

cc_library(
    name = "graph",
    hdrs = ["graph.hpp"],
    deps = ["@com_google_absl//absl/container:fixed_array"],
)

cc_library(
    name = "binary_search",
    hdrs = ["binary_search.hpp"],
    deps = ["@com_google_absl//absl/container:fixed_array"],
)
