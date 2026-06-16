workspace(name = "recommendation_engine")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# gRPC 1.41.x 与 Bazel 3.5 兼容性较好；版本需与 grpc_deps 配套
http_archive(
    name = "com_github_grpc_grpc",
    strip_prefix = "grpc-1.41.1",
    sha256 = "12a4a6f8c06b96e38f8576ded76d0b79bce13efd7560ed22134c2f433bc496ad",
    urls = [
        "https://github.com/grpc/grpc/archive/refs/tags/v1.41.1.tar.gz",
        "https://mirror.ghproxy.com/https://github.com/grpc/grpc/archive/refs/tags/v1.41.1.tar.gz",
    ],
)

load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")

grpc_extra_deps()

http_archive(
    name = "com_github_gflags_gflags",
    sha256 = "34af2f15cf7367513b352bdcd2493ab14ce43692d2dcd9dfc499492966c64dcf",
    strip_prefix = "gflags-2.2.2",
    urls = [
        "https://github.com/gflags/gflags/archive/refs/tags/v2.2.2.tar.gz",
    ],
)

http_archive(
    name = "com_github_google_glog",
    sha256 = "00e4a87e87b7e7612f519a41e491f16623b12423620006f59f5688bfd8d13b08",
    strip_prefix = "glog-0.7.1",
    urls = [
        "https://github.com/google/glog/archive/refs/tags/v0.7.1.tar.gz",
    ],
)
