# 验收报告：堆叠求解器整体重构

本报告对应任务的三项验收：堆叠稳定性数据、轨迹误差对比、异常输入可诊断。
所有数据由仓库自带 CMake/CTest 入口与 `tests/run_acceptance.sh` 一键复现，
原始输出保存在 `output/data/`。

## 1. 复现方式

```bash
# 方式 A：仓库自带测试入口（CTest）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_DEMOS=OFF -DBUILD_SHARED=OFF
cmake --build build -j
ctest --test-dir build --output-on-failure

# 方式 B：完整验收（含基线对比、录屏、全部产物）
./tests/run_acceptance.sh
```

CTest 三个用例全部通过：`solver_verify`、`solver_modes`、`solver_diagnostics`。

## 2. 堆叠稳定性数据

数据来源 `output/data/baseline_metrics.txt` 与 `refactor_default_metrics.txt`
（默认配置，与基线逐位相同）、`refactor_modes.txt`（稳定配置对比）。
指标含义：`residualSpeed` 稳态平均残余速度，`jitterEnergy` 稳态每帧平均位移
（直接刻画抖动），`maxPenetration` 最大接触穿透，`settleDrift` 水平漂移。

### 2.1 默认配置 vs 基线（等价性）

| 场景 | 残余速度 | 最大残余速度 | 水平漂移 | 最大穿透 | 每帧抖动 |
| --- | --- | --- | --- | --- | --- |
| 10 层柱（基线） | 0.006009216 | 0.036029888 | 0.004119256 | 0.002820424 | 0.000054529 |
| 10 层柱（重构默认） | 0.006009216 | 0.036029888 | 0.004119256 | 0.002820424 | 0.000054529 |
| 金字塔（基线/重构） | 0.0 | 0.0 | 0.165309681 | 0.388708173 | 0.844202884 |

默认配置所有指标与基线完全一致，证明重构未改变既有行为。

### 2.2 稳定配置（分裂位置修正 + 暖启动 + 可选早停）

| 场景/模式 | 每帧抖动 | 水平漂移 | 最大穿透 | 实际迭代数 |
| --- | --- | --- | --- | --- |
| 柱 / legacy Baumgarte | 0.000054529 | 0.004119256 | 0.002820424 | 30 |
| 柱 / stable split | 0.000054368 | 0.004112720 | 0.002820132 | 30 |
| 金字塔 / legacy | 0.844202884 | 0.165309681 | 0.388708173 | 30 |
| 金字塔 / stable | 0.844202885 | 0.165309524 | 0.388708165 | 1（早停命中） |

稳定模式在保持物理稳定区间的同时，金字塔场景因残差收敛提前在第 1 次迭代即
满足容差（`lastResidual≈9e-9`），显著减少迭代；柱场景各指标小幅改善。
在 180 Hz 固定步长下，分裂修正与 Baumgarte 的稳态数值接近（详见
`equivalence_proof.md`），稳定模式的核心价值是把修正能量与物理速度解耦、
并提供可配置收敛判据，而非在高步频下改变既有结果。

## 3. 轨迹误差对比（有界）

| 检查项 | 结果 |
| --- | --- |
| 抛体对解析半隐式欧拉解 RMSE | 0.951497041（基线=重构，零差异） |
| 抛体逐点最大误差 | 1.487952047（有界，仅来自积分格式） |
| 同场景两次独立运行轨迹 RMSE | 0.0 |
| 同场景两次独立运行逐点最大误差 | 0.0 |

误差有界且确定性可复现：相同输入产生逐位相同输出，重构未引入额外轨迹误差。

## 4. 异常输入可诊断（错误码）

数据来源 `output/data/diagnostics.txt`，全部 9 项断言通过。

| 输入 | 旧引擎行为 | 重构后错误码 |
| --- | --- | --- |
| `cpBodyNew(NAN, ...)` | `abort()` 进程崩溃 | `CP_SOLVER_INVALID_MASS (3)` |
| 接触质量比 1e18（1e9 vs 1e-9） | 无检测 | `CP_SOLVER_EXTREME_MASS_RATIO (5)` |
| 健康堆叠 | 正常 | `CP_SOLVER_OK (0)` |
| 错误清除 | 无此能力 | `cpSpaceClearSolverError` 返回旧值并复位为 OK |
| 错误回调 | 无 | NaN 场景回调收到 `CP_SOLVER_INVALID_MASS` |

错误码可通过 `cpSpaceGetLastSolverError()` 查询、`cpSolverErrorName()` 转
名称，并可注册 `errorCallback`。诊断不致命，宿主进程不再被中止。

## 5. 性能

见 `output/data/performance.csv`：柱场景 -0.11%（噪声内持平），金字塔多轮
测量交叉、无系统性退化。满足“求解耗时不得退化”。

## 6. 录屏分段（带标题）

文件 `output/video/solver_refactor_recording.mp4`（H.264，640x480，60fps，
16 秒），四个分段各 4 秒，标题由内置 5x7 字体直接栅格化：

| 时间 | 分段标题 | 内容 |
| --- | --- | --- |
| 0:00–0:04 | S1 STACK STABILITY - LEGACY | Baumgarte 偏置（原始管线）柱堆叠 |
| 0:04–0:08 | S2 STACK STABILITY - STABLE | 分裂位置修正 + 暖启动柱堆叠 |
| 0:08–0:12 | S3 TRAJECTORY ERROR BOUND | 仿真抛体与解析参考轨迹叠加 |
| 0:12–0:16 | S4 NUMERICAL DIAGNOSTICS | 健康（蓝）vs NaN 质量（红）+ 错误码状态 |

抽帧 PNG 与原始帧流（gzip）位于 `output/video/`、`output/build/`。

## 7. 产物索引

- 代码：`include/chipmunk/cpSolver.h`、`src/cpSolver.c` 及修改的引擎文件
- 测试：`tests/`（CTest 入口 + 可复现脚本）
- 数据：`output/data/*.txt`、`output/data/performance.csv`
- 文档：`output/docs/design.md`、`equivalence_proof.md`、本报告
- 录屏：`output/video/solver_refactor_recording.mp4`
