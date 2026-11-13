# 广相重构交付物索引

本次改造把 Chipmunk2D 的广相（broad phase）重构为**可替换实现**，默认后端从
AABB 树切换为**空间哈希**，对外 API 与碰撞回调时机保持不变。

## 代码改动（仓库 src/ 下）

| 文件 | 性质 | 作用 |
|---|---|---|
| `src/cpBroadPhase.h/.c` | 新增（内部） | 广相工厂与运行期替换抽象，默认创建空间哈希，可切 AABB 树 |
| `src/cpValidate.h` | 新增（内部） | 对 `-ffast-math` 免疫的有限性判定（IEEE-754 位模式） |
| `src/cpSpace.c` | 修改 | `cpSpaceInit` 改走工厂；公开 API/头文件不变 |
| `src/cpSpaceHash.c` | 修改 | 新增按形状数自动扩容，保证单元表 O(n) |
| `src/cpShape.c` | 修改 | 圆/线段构造期拒绝退化与非法输入 |
| `src/cpPolyShape.c` | 修改 | 多边形顶点数、有限性、重复点、共线退化校验 |

公开头 `include/chipmunk/` 零改动；导出符号只增不删；`cpSpaceUseSpatialHash`
保留原签名与语义。

## 文档

- `output/docs/equivalence_proof.md` — 改造前后碰撞对集合等价性证明
- `output/docs/performance_report.md` — 性能与内存数据、线性上界分析
- `output/acceptance_log.txt` — 完整验收运行日志

## 测试与基准（`output/tests/`）

- `equiv_test.c` — 碰撞对集合 + 回调序列等价（进程内 hash/tree 对比、跨二进制 trace）
- `degenerate_test.c` — 退化形状拒绝（fork 捕获硬断言）
- `hotswap_test.c` — 运行期后端热切换后形状集合与碰撞行为保持
- `bench_broadphase.c` + `cp_alloc_override.*` — 计时与精确内存计量
- `run_all.sh` — 一键验收入口；`run_bench.sh` / `build_bench.sh` — 基准构建与多轮取中位
- `baseline_shim.c` — 让同一测试在改造前基线上运行

## 数据（`output/bench/`）

- `all_median.txt`、`*_median.txt` — 四后端组合的 5 轮中位计时与内存
- `hash_growth.txt` — 空间哈希单元表随形状数扩容实测（cells/n=1.000）
- `environment.txt` — 机器与编译器信息

## 录屏（`output/recording/`）

- `broadphase_refactor.mp4` — 5 段带标题卡录屏（构建 / 等价 / 跨二进制 / 退化拒绝 / 性能内存）
- `segments/*.cast`、`segments/*.gif` — 各段 asciinema 原始记录与 GIF
- `record_sessions.sh`、`compose_video.sh`、`make_card.py` — 录屏与合成脚本

## 复现命令

```sh
# 1. 构建改造后静态库
cmake -S . -B build-new -DBUILD_DEMOS=OFF -DBUILD_SHARED=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-new -j4

# 2. 一键验收（等价 + 退化拒绝；基线 worktree 路径见脚本内 BASELINE_SRC）
./output/tests/run_all.sh

# 3. 性能/内存（需要先按 build_bench.sh 构建插桩二进制）
./output/tests/build_bench.sh "$PWD" output/tests/bench_new_instr
./output/tests/bench_new_instr hash

# 4. 重新生成录屏
./output/recording/record_sessions.sh && ./output/recording/compose_video.sh
```

## 已知的上游既有问题（非本次引入）

- 接触缓冲环 `contactBuffersHead` 在 `cpSpaceFree` 后仍有固定 1,008,328 B 未释放，
  改造前后数值完全一致，见性能报告第 4 节。
- 长时高堆叠场景下上游求解器会数值发散（基线在同一物理步挂死），测试场景已用
  阻尼/铺开布局规避；该问题与广相无关。
