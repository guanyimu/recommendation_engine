# Config 设计

本文档描述推荐引擎配置模块的**当前实现**，对应 `src/common/` 与 ranking 服务用法。

设计思路与优缺点见 [学习笔记-配置与并发.md](./学习笔记-配置与并发.md)。

---

## 1. 设计目标

| 目标 | 做法 |
|------|------|
| 加配置项不改解析器 | 通用 `key=value` → 快照 map，新增 key 只改 conf |
| 读配置无静默默认值 | `RequireString` / `RequireSizeT`，缺 key 或非法即失败 |
| 可热更新 | 内部 `ConfigManager::Reload()` swap 快照（由 GatedConfig 持有） |
| 更新时不接新单 | 内部 `ServiceGate` drain，对外只暴露 `GatedConfig::Reload()` |
| 业务边界清晰 | **业务只依赖 `GatedConfig`**，不 include `ConfigReader` / `ServiceGate` |
| 构建层强制边界 | Bazel `config_internal` 禁止外部 deps |

---

## 2. 分层结构

```text
对外（业务 / 其它目录）
  GatedConfig          ← 读 conf、RequestGuard、Reload
  types.h              ← UserId / ItemId / Video

对内（仅 src/common 包内，config_internal）
  ConfigReader         ← 抽象 + RequireString / RequireSizeT
  ConfigSnapshot       ← 不可变 KV，LoadFromFile
  ConfigManager        ← 构造时 LoadFromFile，Reload 时 swap 快照
  ServiceGate          ← StopAccepting / WaitDrained / TryEnter（Guard 私有）
```

```text
src/common/
├── gated_config_reloader.h   # 对外类 GatedConfig
├── gated_config_reloader.cpp
├── types.h                   # 对外
├── config_reader.*           # 内部
├── config_snapshot.*         # 内部
├── config_manager.*          # 内部
├── service_gate.*            # 内部
└── BUILD.bazel               # 见 §3
```

### 2.1 GatedConfig（进程内单例入口）

```cpp
class GatedConfig {
 public:
  static GatedConfig& Instance();
  static bool Init(const std::string& path);
  // RequireString / RequireSizeT / Reload / RequestGuard ...
};
```

**一个 OS 进程一份 `GatedConfig::Instance()`**，内部**独占**一份 `ConfigManager` 与同一把 `ServiceGate`。进程内所有服务（Ranking / 以后的 Video 等）都走 `Instance()`，Reload 时整节点一起 drain、一起换快照，避免「同一进程里多个 Gate、多套配置视图」导致半更新。

| API | 作用 |
|-----|------|
| `Init(path)` | 构造并持有 `ConfigManager(path)` |
| `Instance()` | 取进程内唯一 GatedConfig |
| `HasSnapshot()` | 是否已成功加载快照（`Init` 成功后才为 true） |
| `Has` / `Get` / `Require*` | 无快照时打 `[config]` 错误日志并失败，不静默返回默认值 |
| `RequestGuard` | 构造时尝试接请求，Reload drain 期间 `ok()==false` |
| `Reload(validator)` | StopAccepting → drain → swap 快照 → 可选校验 → StartAccepting |

内部通过 `unique_ptr` 持有 `ConfigManager` 与 `ServiceGate`（头文件不暴露类型）。

### 2.2 ConfigSnapshot / ConfigManager（内部）

- **Snapshot**：从文件读入全部 `key=value`，**只读**；Reload 时新建一份再 swap
- **Manager**：`ConfigManager(path)` 构造时加载；`Get` / `Has` 走读锁；`Reload` 走写锁换快照

```text
ConfigManager(path)  →  构造时 LoadFromFile
Reload()             →  LoadFromFile → 写锁 swap snapshot_
Get(key)             →  读锁 → snapshot_->Get(key)
```

### 2.3 ServiceGate（内部）

```text
StopAccepting → WaitDrained → Reload → StartAccepting
```

`TryEnter` / `Leave` 为 private，仅 `ServiceGate::Guard` 与 `GatedConfig::RequestGuard` 使用。

---

## 3. Bazel 目标与可见性

`src/common/BUILD.bazel` 拆分如下：

| Target | 可见性 | 说明 |
|--------|--------|------|
| `:common` | public | 业务 deps 此 target；`hdrs` 仅 `gated_config_reloader.h` |
| `:types` | public | `types.h`，经 `:common` 传递 |
| `:config_internal` | private（包内） | Manager / Snapshot / Reader / Gate |
| `:gated_config_reloader_lib` | private（包内） | `GatedConfig` 实现，链到 internal |

其它目录 **不能** `deps //src/common:config_internal`，也无法合法 `#include "config_manager.h"`。

业务统一：

```python
deps = ["//src/common:common"]
```

```cpp
#include "gated_config_reloader.h"
#include "types.h"
```

---

## 4. 配置文件

路径：`configs/ranking.conf`（ranking 进程与 browse_demo 共用）

```ini
# ranking 服务配置
ranking_address=127.0.0.1:50051
recommend_count=8
```

| Key | 用途 |
|-----|------|
| `ranking_address` | gRPC 监听 / 客户端连接地址 |
| `recommend_count` | 每次 Ranking 返回的 item 数量 |

格式：`key=value`，`#` 为注释。新增 key 仍只需改 conf + 业务里 `Require*` 调用，**不必改解析器**。

---

## 5. 业务如何使用

### 5.1 使用方式

带 Gate 的服务**不注入** `GatedConfig&`，统一 `GatedConfig::Instance()`：

```cpp
// 启动
GatedConfig::Instance().Init("configs/ranking.conf");
GatedConfig& config = GatedConfig::Instance();
config.RequireString("ranking_address", &addr);

// 业务 / RPC
GatedConfig::Instance().RequireSizeT("recommend_count", &count);
GatedConfig::RequestGuard guard;   // 默认走 Instance()
```

### 5.2 RPC 入口

```cpp
GatedConfig::RequestGuard guard;
if (!guard.ok()) {
  return grpc::Status(UNAVAILABLE, "service not accepting requests");
}
```

### 5.3 客户端进程（browse_demo）

```cpp
GatedConfig::Instance().Init("configs/ranking.conf");
GatedConfig::Instance().RequireString("ranking_address", &addr);
```

---

## 6. Reload 流程（当前 ranking 进程）

Reload **不在 main 里写 drain 逻辑**，由 `RankingGrpcServer::Reload()` 委托 `GatedConfig::Reload()`。

```text
1. ranking_main: GatedConfig::Init + Require* 读 ranking_address
2. RankingGrpcServer 启动，RPC 内 RequestGuard + RequireSizeT
3. 改 configs/ranking.conf
4. kill -HUP <ranking_main pid>
5. control_thread → server.Reload()
     → GatedConfig::Reload(ValidateRankingConfig)
6. 新请求读到新 recommend_count；drain 期间新 RPC 被拒绝
```

`ValidateRankingConfig` 在 `ranking_grpc_server.cpp` 内，用 `RequireString` / `RequireSizeT` 校验 Reload 后的快照。

---

## 7. 依赖关系（简图）

```text
ranking_main / browse_demo
        │
        ▼
   GatedConfig ──────► ConfigManager ──► ConfigSnapshot
        │                      ▲
        │                      │ Reload swap
        └── ServiceGate ◄──────┘ drain 时序

RankingGrpcServer / RankingServiceImpl / RankingServer
        │
        └── GatedConfig::Instance()（同进程同一份）
```

---

## 8. 未来演进

```text
阶段 1（现在）   conf 文件 + ranking_main SIGHUP Reload
阶段 2           各服务独立 conf（ranking.conf / video.conf）
阶段 3           配置中心 watch → 调 GatedConfig::Reload
阶段 4           集群滚动：LB 摘节点 → drain → Reload → 挂回
```

| 层级 | 机制 |
|------|------|
| 进程内 | snapshot swap + `shared_mutex` |
| 节点级 | `GatedConfig::Reload` 内建 Gate drain |
| 集群级 | 配置中心 + 各节点依次 Reload |

---

## 9. 测试建议

对外 API 已是 `GatedConfig`，单测可选：

1. **集成式**：临时 conf 文件 + `GatedConfig::Init`，测 `Require*` / `Reload`
2. **后续**：为测试提供 `GatedConfig(ConfigManager&)`，注入 fake Manager（需在同包或 testonly 扩展 internal）

不再推荐业务代码直接 Mock `ConfigReader`；Reader 为 internal 实现细节。

---

## 10. 相关文件

| 文件 | 说明 |
|------|------|
| `configs/ranking.conf` | ranking 地址与 recommend_count |
| `src/common/gated_config_reloader.h` | 对外类 `GatedConfig` |
| `src/common/BUILD.bazel` | 可见性拆分 |
| `src/servers/ranking/ranking_main.cpp` | Init、SIGHUP 触发 Reload |
| `src/servers/ranking/ranking_grpc_server.cpp` | `Reload()` + 校验 |
| `src/servers/ranking/ranking_service_impl.cpp` | `RequestGuard` 示例 |
| `examples/browse_demo/main.cpp` | 客户端读 conf |

---

## 11. 相关文档

- [学习笔记-配置与并发.md](./学习笔记-配置与并发.md) — 快照 / Gate / GatedConfig 思路与优缺点
- [项目结构说明.md](./项目结构说明.md) — 目录与 Bazel 命令
