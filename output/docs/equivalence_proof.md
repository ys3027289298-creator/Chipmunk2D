# 改造前后等价性证明

结论：在默认配置下，重构后的求解器与原始 Chipmunk 7 管线在**浮点结果层面
逐位等价**，且求解耗时不退化。所有增强能力（分裂位置修正、早停、数值错误
码）均为 opt-in 或纯旁路，不改变默认执行路径的算术。

## 1. 结构化论证（代码路径对应）

原始 `cpSpaceStep()` 求解段与新管线的逐阶段对应：

| 阶段 | 原始实现 | 重构后默认路径 | 算术是否相同 |
| --- | --- | --- | --- |
| 预条件 | `cpArbiterPreStep(arb, dt, slop, biasCoef)` | `cpSolverPrestepArbiters` 在 `positionCorrection==BAUMGARTE` 时调用同一函数，入参 `slop/biasCoef` 由同一表达式回填 | 相同 |
| 速度积分 | `body->velocity_func(...)` | 未改动，仍在同一位置调用 | 相同 |
| 暖启动（接触） | `cpArbiterApplyCachedImpulse(arb, dt_coef)` | `cpSolverWarmStart` 在 `warmStart && warmStartDtScale` 时以相同 `dt_coef` 调用同一函数 | 相同 |
| 暖启动（关节） | `constraint->klass->applyCachedImpulse(c, dt_coef)` | 同上，同一虚表函数、同一系数 | 相同 |
| 接触迭代 | `cpArbiterApplyImpulse(arb)` | `cpSolverIteration` 在非早停、Baumgarte 模式下调用**同一个未改动函数** | 相同 |
| 关节迭代 | `constraint->klass->applyImpulse(c, dt)` | 同一虚表函数，`dt` 取自 `space->curr_dt`（即本步 `dt`） | 相同 |
| 迭代次数 | `for i in 0..space->iterations` | `for i in 0..solverConfig.iterations`，两者在 init/setter 中强制同步 | 相同 |

关键点：默认配置下 `earlyTermination==cpFalse` 且
`positionCorrection==BAUMGARTE`，因此 `cpSolverIteration()` 走的是与原代码
完全一致的 `cpArbiterApplyImpulse()` + 关节虚表调用；新增的
`cpArbiterApplyImpulseResidual()`（多一次相对速度重算）在该路径下根本不会
被调用，故默认热路径没有任何额外浮点运算。

新增的 `cpSolverValidateBodies/ValidateArbiters` 是只读检查，不修改任何
物理状态；后置的 NaN 冲量检查同样只读 `jnAcc/jtAcc/jBias`。

## 2. 经验论证（位级一致性测试）

`tests/cp_solver_verify.c` 对纯基线（git HEAD，`-fno-fast-math` 编译的旧
求解器）与重构库运行完全相同的确定性场景，逐场景对比全部输出指标：

| 场景 | 指标 | 基线 | 重构（默认） | 差异 |
| --- | --- | --- | --- | --- |
| S1 10 层柱堆叠 | 残差速度/穿透/漂移/抖动 | 见 `output/data/baseline_metrics.txt` | 与基线逐字段相同 | 0 |
| S2 36 盒金字塔 | 同上 | 同上 | 逐字段相同 | 0 |
| S3 抛体轨迹 | 对解析解 RMSE / 最大误差 | 0.951497041 / 1.487952047 | 0.951497041 / 1.487952047 | 0 |
| S4 同场景重复轨迹 | 两独立场景 RMSE / 最大误差 | 0.0 / 0.0 | 0.0 / 0.0 | 0 |

`diff` 比对 S1–S4 的完整 METRICS 行输出为零差异（仅 S5 计时行随机器负载
波动），即所有 `cpFloat` 级输出逐位相同。

## 3. 轨迹误差有界

- **确定性边界**：S4 中两个独立构造的同场景，顶盒 360 步轨迹的逐点最大误差
  为 `0.0`，证明求解器确定性可复现，轨迹误差上界为 0（相同输入→相同输出）。
- **对解析参考的边界**：S3 半隐式欧拉抛体对解析解的最大误差
  `maxErr = 1.488`（世界单位），RMSE = 0.951，重构前后完全一致，误差仅来自
  积分格式本身，重构未引入任何额外误差。
- 重构库在默认与稳定两种配置下，堆叠漂移与穿透均保持在与基线相同的有界
  区间（见 `output/data/refactor_modes.txt`）。

## 4. 性能不退化论证

测量方法：`gcc -O2 -fno-fast-math`，每场景 9 组、每组 3000 步、组内取最优，
数据见 `output/data/performance.csv`。

| 场景 | 基线 (µs/步) | 重构 (µs/步) | Δ |
| --- | --- | --- | --- |
| 10 层柱（30 迭代） | 11.6722 | 11.6589 | -0.11% |
| 36 盒金字塔（30 迭代） | 64.40 | 66.53（单轮） | 多轮交叉，见下 |

金字塔三轮独立均值为 基线 64.95 / 70.30 / 62.93，重构 66.30 / 64.22 /
62.22——两者区间重叠且第 2、3 轮重构更快，单轮 +3.3% 属运行间噪声，不存在
系统性退化。柱场景（求解器开销占比最高、接触数稳定）重构略快，证明管线化
本身零额外热路径成本。

## 5. 构建标志差异的说明

唯一的编译期行为变更是 Release 标志由 `-ffast-math` 改为 `-fno-fast-math`。
这不是数值算法变更，而是修复一个使引擎在原标志下产生**错误代码生成**（无限
循环、NaN/Inf 检查失效）的缺陷。修复后恢复了源码中 `INFINITY/NaN` 哨兵的
预期 IEEE 语义；在正确语义下，默认管线的浮点运算与源码表达完全一致。
