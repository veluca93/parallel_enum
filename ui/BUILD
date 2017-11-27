package(default_visibility = ["//visibility:public"])


cc_binary(
    name = "text_ui",
    srcs = ["text_ui.cpp"],
    deps = [
        "//enumerable:clique",
        "//enumerator:sequential",
		"//enumerator:parallel",
        "//external:gflags",
        "//util:graph",
        "//util:concurrentqueue",
        "@com_google_absl//absl/memory",
    ],
)
