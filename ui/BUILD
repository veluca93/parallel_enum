package(default_visibility = ["//visibility:public"])

load(
    "//third_party/mpi:mpi.bzl",
    "additional_mpi_lib_defines",
)

MPI_DEFINES = (additional_mpi_lib_defines())

cc_binary(
    name = "text_ui",
    srcs = ["text_ui.cpp"],
    defines = MPI_DEFINES,
    deps = [
        "//enumerable:clique",
        "//enumerable:diam2kplex",
        "//enumerator:distributed",
        "//enumerator:parallel",
        "//enumerator:sequential",
        "//external:gflags",
        "//util:concurrentqueue",
        "//util:graph",
        "@com_google_absl//absl/memory",
    ],
)

cc_binary(
    name = "stats",
    srcs = ["stats.cpp"],
    deps = [
        "//external:gflags",
        "//permute",
        "//util:graph",
    ],
)
