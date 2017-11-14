workspace(name = "parallel_enum")

git_repository(
    name = "com_google_absl",
    commit = "dedb4eec6cf0addc26cc27b67c270aa5a478fcc5",
    remote = "https://github.com/abseil/abseil-cpp.git",
)

git_repository(
    name = "com_github_gflags_gflags",
    remote = "https://github.com/gflags/gflags.git",
    tag = "v2.2.1",
)

bind(
    name = "gflags",
    actual = "@com_github_gflags_gflags//:gflags",
)

bind(
    name = "gflags_nothreads",
    actual = "@com_github_gflags_gflags//:gflags_nothreads",
)
