licenses(["restricted"])

config_setting(
    name = "with_mpi_support",
    values = {"define": "with_mpi_support=true"},
    visibility = ["//visibility:public"],
)

filegroup(
    name = "all_files",
    srcs = glob(
        ["**/*"],
        exclude = [
            "**/METADATA",
            "**/OWNERS",
        ],
    ),
    visibility = ["//visibility:public"],
)

load("//third_party/mpi:mpi.bzl", "mpi_hdr")
load("//third_party/mpi:mpi.bzl", "if_mpi")

cc_library(
    name = "mpi",
    srcs = if_mpi(["libmpi.so",]),
    hdrs = if_mpi(mpi_hdr()),
    visibility = ["//visibility:public"],
)