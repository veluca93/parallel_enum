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
	hdrs = ["parallel_tbb.hpp", "parallel_pthreads.hpp", "parallel_pthreads_steal.hpp"],
	deps = [":enumerator"],
)

cc_library(
    name = "distributed",
    hdrs = ["distributed_mpi.hpp"],
    deps = [":enumerator",
            "//third_party/mpi"],
)
