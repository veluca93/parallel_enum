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
    name = "diam2kplex",
    srcs = ["diam2kplex.cpp"],
    hdrs = ["diam2kplex.hpp"],
    deps = [
        ":enumerable",
        "//permute",
        "@com_google_absl//absl/strings",
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
    name = "connected_hereditary",
    hdrs = ["connected_hereditary.hpp"],
    deps = [
        ":commutable",
    ],
)

cc_library(
    name = "ckplex",
    srcs = ["ckplex.cpp"],
    hdrs = ["ckplex.hpp"],
    deps = [
        ":connected_hereditary",
        ":enumerable",
        "//permute",
        "//util:graph",
    ],
)
