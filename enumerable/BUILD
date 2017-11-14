package(default_visibility = ["//visibility:public"])

cc_library(
    name = "enumerable",
    hdrs = ["enumerable.hpp"],
)

cc_library(
    name = "clique",
    srcs = ["clique.cpp"],
    hdrs = ["clique.hpp"],
    deps = [
        ":enumerable",
        "//permute",
        "//util:graph",
    ],
)
