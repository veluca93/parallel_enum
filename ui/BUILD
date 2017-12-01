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
        "//enumerator:sequential",
		"//enumerator:parallel",
        "//enumerator:distributed",
        "//external:gflags",
        "//util:graph",
        "//util:concurrentqueue",
        "@com_google_absl//absl/memory",
    ],
)
