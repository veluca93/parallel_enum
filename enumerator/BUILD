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
	hdrs = ["parallel.hpp"],
	deps = [":enumerator"],
	
)
