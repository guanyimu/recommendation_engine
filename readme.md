# 简易推荐系统引擎

C++ 练手项目：多服务协作的推荐链路（智能体 → 主服务 → 排序 / 拉视频），Bazel 构建。

## 快速开始

```bash
# 编译并运行 demo（会演示配置 8 → 10 的热更新）
bazel run //examples/browse_demo:browse_demo
```

需在项目根目录执行；首次运行会拉取 Bazel 依赖。

## 文档

| 文档 | 内容 |
|------|------|
| [docs/学习笔记-配置与并发.md](docs/学习笔记-配置与并发.md) | **设计思路与优缺点**（快照/Config 分离、Gate/Guard、并发与集群） |
| [docs/Config设计.md](docs/Config设计.md) | 本仓库 Config 模块的实现说明 |
| [docs/项目结构说明.md](docs/项目结构说明.md) | 目录结构与 Bazel 命令 |

## 当前进度

- Agent + 三服务器（主 / 排序 / 拉视频）进程内联调
- 通用 KV 配置、`ConfigSnapshot` + `ConfigManager`、Reload
- `ServiceGate` / `Guard` 排空后更新
- demo 验证 `recommend_count` 8 → 10 生效

## 后续方向

- 排序 / 拉视频接数据库，去掉随机占位
- 配置中心 watch 替代 demo 里手动改 conf
- 单元测试（Mock `ConfigReader`）
