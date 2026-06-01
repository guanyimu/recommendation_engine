# Config 设计

本文档描述推荐引擎的配置模块设计，对应当前 `src/common/` 下的实现。

---

## 1. 设计目标

| 目标 | 做法 |
|------|------|
| 加配置项不改解析器 | 通用 `key=value` → `map`，新增 key 只改 conf 文件 |
| 可测试 | `ConfigReader` 接口，测试可注入 Mock |
| 可热更新 | `ConfigManager::Reload()` swap 新快照 |
| 并发安全 | `shared_mutex` + 整表替换 |
| 集群演进 | 进程内单例；未来由配置中心推送，替代 demo 里 main 的角色 |

---

## 2. 模块结构

```
src/common/
├── config_reader.h      # 抽象接口 Get / GetSizeT
├── config_snapshot.h    # 不可变 KV 快照（从文件加载）
├── config_manager.h     # 进程内单例，Reload swap 快照
├── service_gate.h       # 节点级 drain（不接新单 + 等 in-flight 结束）
└── config.h             # 统一 #include 入口
```

### 2.1 ConfigReader（接口）

```cpp
class ConfigReader {
 public:
  virtual std::string Get(key, default_value) const = 0;
  std::size_t GetSizeT(key, default_value) const;  // 默认实现
};
```

- 业务类（如 `RankingServer`）**构造时注入 `const ConfigReader&`**
- 不在业务代码里写 `ConfigManager::Instance()`，便于单测

### 2.2 ConfigSnapshot（不可变快照）

- 从 conf 文件读取全部 `key=value` 存入 `unordered_map`
- **只读**，Reload 时新建一份，不原地修改

### 2.3 ConfigManager（进程内单例）

```
Init(path)  → 首次 LoadFromFile
Reload()    → LoadFromFile → 加写锁 → swap snapshot_
Get(key)    → 加读锁 → 从 snapshot_ 取值
```

- **一个 OS 进程一份单例**；集群里每台 ranking 进程各自有一份
- swap 保证读线程看到完整旧表或完整新表，不会出现半更新 map

### 2.4 ServiceGate（节点级 drain）

```
StopAccepting()     → accepting = false
WaitDrained()       → 等 active_requests == 0（默认最多 2s）
Reload / 其它更新
StartAccepting()    → accepting = true
```

请求入口用 `ServiceGate::Guard`：未 `TryEnter` 成功则拒绝（服务更新中）。

---

## 3. 配置文件

```
configs/ranking.conf    # ranking 服务专用
```

示例：

```ini
# ranking 服务配置
recommend_count=8
```

格式：`key=value`，`#` 开头为注释。  
**新增 key 无需改 C++ 解析代码**，业务里 `config_.Get("new_key")` 即可。

---

## 4. 业务类如何使用

```cpp
// 构造：注入 ConfigReader 引用（通常是 ConfigManager::Instance()）
RankingServer ranking_server(config);

// 每次请求：从 config 读，不缓存到成员变量（支持热更新）
std::size_t count = config_.GetSizeT("recommend_count", 8);
```

注意：

- **不要** `RankingServer(size_t count)` 把值拷进成员后不再读 config  
- 若以后有**索引/模型等重资源**，Reload 后需额外 `RebuildFromConfig()`

---

## 5. 更新流程（当前 demo）

`main` **暂代配置中心**，演示完整更新链路：

```
1. ConfigManager::Init("configs/ranking.conf")
2. 正常处理请求（ServiceGate::Guard）
3. main 调用 ReloadFromConfigCenter():
     StopAccepting → WaitDrained → ConfigManager::Reload() → StartAccepting
4. 再次处理请求（读到新 recommend_count）
```

修改 `configs/ranking.conf` 后重新运行 demo，或在同进程内 Reload 即可验证。

---

## 6. 未来演进

```
阶段 1（现在）   conf 文件 + main 模拟配置中心下发 Reload
阶段 2           各服务独立 conf（ranking.conf / video.conf）
阶段 3           Redis / Nacos watch，后台线程 OnConfigChange → Reload
阶段 4           集群滚动更新：LB 摘节点 → drain → Reload → 挂回
```

| 层级 | 机制 | 保证 |
|------|------|------|
| 进程内 | `shared_mutex` + snapshot swap | Reload 与 Get 不数据竞争 |
| 节点级 | `ServiceGate` drain | 更新窗口减少在途请求 |
| 集群级 | 配置中心 + 滚动发布 | 各节点依次 Reload，无需全集群大锁 |

---

## 7. 测试建议

```cpp
class MockConfig : public ConfigReader {
  std::string Get(key, default_value) const override {
    return data_.at(key);
  }
};

MockConfig mock;
mock.Set("recommend_count", "3");
RankingServer server(mock);
// Rank() 应返回 3 个 item
```

不依赖 conf 文件、不依赖单例，单元测试隔离。

---

## 8. 相关文件

| 文件 | 说明 |
|------|------|
| `configs/ranking.conf` | ranking 服务配置 |
| `examples/browse_demo/main.cpp` | 演示 Init / 请求 / Reload |
| `src/servers/ranking/ranking_server.cpp` | 注入 ConfigReader 的示例 |
