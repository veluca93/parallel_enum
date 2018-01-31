package(default_visibility = ["//visibility:public"])

cc_library(
    name = "enumerator",
    hdrs = ["enumerator.hpp"],
    deps = [
        "//enumerable",
        "@com_google_absl//absl/memory",
    ],
)

cc_library(
    name = "sequential",
    hdrs = ["sequential.hpp"],
    deps = [":enumerator"],
)

cc_library(
    name = "parallel",
    hdrs = [
        "parallel_pthreads.hpp",
        "parallel_pthreads_steal.hpp",
        "parallel_tbb.hpp",
    ],
    deps = [":enumerator"],
)

cc_library(
    name = "distributed",
    hdrs = ["distributed_mpi.hpp"],
    deps = [
        ":enumerator",
        ":parallel",
        "//third_party/mpi",
    ],
)
