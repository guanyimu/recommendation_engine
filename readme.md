# 简易推荐系统引擎

C++ 练手项目：多服务协作的推荐链路（智能体 → 主服务 → 排序 / 拉视频），Bazel 构建。

## 快速开始

需两个终端，均在项目根目录：

```bash
# 终端 1：启动 Ranking gRPC 服务（阻塞监听）
bazel run //src/servers/ranking:ranking_main

# 终端 2：10 个 Agent 并发刷视频（需 ranking 已启动）
bazel run //examples/browse_demo:browse_demo
```

Ranking 每次请求会随机 sleep 1～2 秒模拟耗时；10 个 Agent 并发时总耗时应接近 1～2 秒而非 10 秒+。

`configs/ranking.conf` 示例：

```ini
ranking_address=127.0.0.1:50051
recommend_count=8
```

- **browse_demo** 与 **ranking_main** 均从该文件读 `ranking_address`、`recommend_count`
- 修改 conf 后，对 ranking 进程发 `kill -HUP <pid>` 触发 Reload（排空在途 RPC 后重读文件）

日志：`[ranking]` 服务端处理耗时；`[ranking_client]` RPC 往返耗时与连接/RPC 失败原因。

首次运行会拉取 Bazel 依赖。

## 文档

| 文档 | 内容 |
|------|------|
| [docs/学习笔记-配置与并发.md](docs/学习笔记-配置与并发.md) | **设计思路与优缺点**（快照/Config 分离、Gate/Guard、并发与集群） |
| [docs/Config设计.md](docs/Config设计.md) | 本仓库 Config 模块的实现说明 |
| [docs/项目结构说明.md](docs/项目结构说明.md) | 目录结构与 Bazel 命令 |

## 当前进度

- Ranking gRPC 独立进程 `ranking_main`；conf 含 `ranking_address` / `recommend_count`；SIGHUP Reload
- `ServiceGate` 已接入 ranking 服务端 RPC 入口

## 后续方向

- 排序 / 拉视频接数据库，去掉随机占位
- 配置中心 watch 替代 demo 里手动改 conf
- 单元测试（Mock `ConfigReader`）
