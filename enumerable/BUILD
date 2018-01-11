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

cc_library(
    name = "commutable",
    hdrs = ["commutable.hpp"],
    deps = [
        ":enumerable",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "ckplex",
    srcs = ["ckplex.cpp"],
    hdrs = ["ckplex.hpp"],
    deps = [
        ":commutable",
        ":enumerable",
        "//permute",
        "//util:graph",
    ],
)
