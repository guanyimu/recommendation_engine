# 简易推荐系统引擎

C++ 练手项目：多服务 gRPC 协作（Agent → Browse → Ranking / Video），Bazel 构建，可选 Nginx 负载均衡。

## 快速开始

需先启动服务，再跑 demo 或压测（均在项目根目录）：

```bash
# 启动 Ranking / Video / Browse（默认 2 实例 + Nginx LB）
./tools/run_servers.sh

# 单次 Browse 冒烟
./tools/run_demo.sh

# 多进程压测（参数见 --help）
./tools/run_load_test.sh --workers 1000 --duration 30

# 停止
./tools/stop_servers.sh
```

单实例、无 LB：`USE_LB=0 ./tools/run_servers.sh`

客户端与压测读 `configs/services.conf` 中的 **LB 端口**（`50052` / `50053` / `50054`）；后端实例由脚本绑定在 `15xxx`（详见 conf 注释）。

## 文档

| 文档 | 内容 |
|------|------|
| [docs/学习笔记-配置与并发.md](docs/学习笔记-配置与并发.md) | 设计思路（快照/Config、并发与集群） |
| [docs/Config设计.md](docs/Config设计.md) | Config 模块实现说明 |
| [docs/项目结构说明.md](docs/项目结构说明.md) | 目录结构与 Bazel 命令 |

## 当前进度

- 三服务独立进程：`ranking_main` / `video_main` / `browse_main`；SIGHUP Reload
- Nginx stream 四层 LB（`5005x` → `15xxx` 双实例）；`stop_servers.sh` 按端口/pid 清理
- `browse_load_test`：C++ 多进程压测，pipe 汇总成功率与延迟
- 客户端 conf 固定 LB 地址；服务端端口由 `run_servers.sh` + `RE_LISTEN_ADDRESS` 注入

## 下一步：名字服务 + 自注册（目标架构）

当前 **端口、upstream、pid** 都写在脚本和 conf 里，适合本地 demo。下一步改为 **服务自己注册、客户端/LB 从名字服务查地址**，去掉硬编码与 pid 编排。

### 核心思路

```text
                    ┌──────────────────────────────────┐
                    │  名字服务（etcd / Consul / …）      │
                    │  ranking  →  LB_IP:50052          │
                    │  video    →  LB_IP:50053          │
                    │  browse   →  LB_IP:50054          │
                    │  ranking/instances/<id> → host:port │  ← 实例自注册
                    └───────────────┬──────────────────┘
                                    │
         客户端 / browse 下游        │  LB 拉取或 watch 后端列表
              只查逻辑服务名         ▼
                    ┌──────────────────────────────────┐
                    │  负载均衡器（固定 LB_IP:5005x）    │
                    └───────────────┬──────────────────┘
                                    ▼
                              各服务实例（bind 空闲端口）
```

### 可以这样做：LB 地址也注册到名字服务

**可以。** 逻辑服务名指向 **LB 入口**，而不是某个后端实例：

| 名字服务 key | value（示例） |
|--------------|---------------|
| `ranking` | `10.0.0.1:50052` |
| `video` | `10.0.0.1:50053` |
| `browse` | `10.0.0.1:50054` |

客户端、`BrowseServer` 调 Ranking/Video 时：**只查名字服务拿 LB 地址**，不感知后面有几台实例、端口是多少。

实例级注册（供 LB 动态 upstream 用）另起 key，例如 `ranking/instances/<id> → 10.0.0.2:15087`。

### 「所有服务只保存一个裸 IP」

分两层理解：

1. **Bootstrap（每台机器一份）**  
   只存 **名字服务地址**（或 **LB 的 IP** + 固定端口约定），例如：
   ```ini
   registry=10.0.0.1:2379
   # 或：lb_host=10.0.0.1  （50052/50053/50054 端口约定不变）
   ```
   不再在 conf 里维护 `15052`、`15062` 等 **每个后端实例** 的地址。

2. **运行时**  
   - **对外调用**：`Resolve("browse")` → `LB_IP:50054`（从名字服务读，可缓存 + watch）  
   - **对内 bind**：进程 `listen(0.0.0.0:0)` 或试空闲端口 → 把 **实际** `host:port` 注册到名字服务  
   - **关闭**：`SIGTERM` → 从名字服务 **注销** 实例 → `Shutdown()`，**不必依赖 `.pid` 文件**

同一 LB 上一个 IP、三个端口（50052/50053/50054）是常见做法；换 LB 时只改名字服务里的注册，不改业务代码。

### 和现有组件的对应关系

| 现在 | 下一步 |
|------|--------|
| `services.conf` 写死 `*_address` | conf 只留 `registry` / 服务名；地址运行时查 |
| `run_servers.sh` 写死 `15xxx` | 脚本只起 registry + LB；实例自启自注册 |
| nginx 静态 upstream | watch 注册中心 → 动态 upstream，或换 Envoy |
| `stop_servers.sh` + pid | 查注册中心实例列表 → `SIGTERM`；LB 健康检查摘流 |

### 建议实施顺序

1. **Registry 抽象**：`Register` / `Deregister` / `Resolve`（可先 mock，再接 etcd/Consul）  
2. **服务端**：bind 随机端口 → 注册；Shutdown 注销  
3. **客户端**：从 `Resolve("browse")` 取 LB 地址  
4. **LB 入口**也写入名字服务；upstream 从实例注册列表生成  
5. **逐步去掉**：`RE_LISTEN_ADDRESS`、`.pids/`、脚本里的端口硬编码  

### 其它后续

- 排序 / 拉视频接真实数据，去掉随机 sleep 占位  
- 配置中心 watch 替代手动改 conf + SIGHUP  
- 单元测试（Mock `ConfigReader` / Registry）
