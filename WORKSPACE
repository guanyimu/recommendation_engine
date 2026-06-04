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
