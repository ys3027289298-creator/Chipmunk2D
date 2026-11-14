# 求解器流程重构设计说明（Chipmunk2D）

## 1. 背景与问题定位

原始 `cpSpaceStep()`（`src/cpSpaceStep.c`）把求解流程的五个阶段硬编码在
一个函数里：

1. 位置积分与碰撞对收集；
2. 接触/约束预条件（`cpArbiterPreStep`，Baumgarte 偏置速度）；
3. 速度积分；
4. 暖启动（复用上一步累积冲量，`cpArbiterApplyCachedImpulse`）；
5. 固定次数的投影高斯–赛德尔（PGS）迭代（`cpArbiterApplyImpulse`）。

存在三类问题：

- **策略不可配置**：迭代次数、暖启动、位置修正是分散的隐式行为，无法在
  不改引擎源码的情况下切换位置修正策略或加入收敛判据。
- **数值异常不可诊断**：异常输入（NaN 质量、极端质量比）要么触发
  `cpAssertHard(...); abort()` 直接杀死宿主进程，要么静默传播，没有错误码。
- **构建级缺陷（关键）**：Release 构建带 `-ffast-math`（隐含
  `-ffinite-math-only`）。该选项让编译器假设不存在 NaN/Inf，从而把所有
  `INFINITY` 哨兵比较常量折叠。实测 `cpSpaceProcessComponents()` 中的
  `sleepTimeThreshold != INFINITY` 在该选项下恒为真，休眠组件分支被错误
  激活并进入死循环（本任务最初观察到的“卡住/超时”根因）。

## 2. 统一的可配置求解流程

新增公共头 `include/chipmunk/cpSolver.h` 与实现 `src/cpSolver.c`，用单一
结构 `cpSolverConfig` 描述整条求解管线：

| 配置项 | 含义 | 默认值（等价旧行为） |
| --- | --- | --- |
| `iterations` | PGS 迭代上限，与 `cpSpace.iterations` 双向同步 | 10 |
| `warmStart` | 复用上一步累积接触/关节冲量 | `cpTrue` |
| `warmStartDtScale` | 变步长时按 `dt/prev_dt` 缩放暖启动冲量 | `cpTrue` |
| `positionCorrection` | `0`=Baumgarte 偏置（旧），`1`=分裂位置修正 | `0` |
| `baumgarteBias` / `slop` | 偏置系数与渗透容差 | 由 `collisionBias/slop` 每步回填 |
| `positionCorrectionRate` / `maxCorrectionSpeed` | 分裂修正的收敛率与速度上限 | 0.2 / 0（不限） |
| `earlyTermination` / `velocityTolerance` | 残差足够小时提前退出 | 关 / 1e-4 |
| `maxMassRatio` | 接触两端质量比上限，超限报错 | 1e12 |
| `errorCallback` / `errorUserData` | 数值问题回调 | NULL |

`cpSpaceStep()` 的求解段被替换为四个语义明确的管线调用（`cpSolver.c`）：

- `cpSolverPrestepArbiters()`：按配置选择 Baumgarte 或分裂预条件；
- `cpSolverValidateBodies()` / `cpSolverValidateArbiters()`：数值校验；
- `cpSolverWarmStart()`：受 `warmStart*` 开关控制的暖启动；
- `cpSolverRunIterations()`：统一迭代循环，内置可选早停与残差统计。

### 2.1 分裂位置修正（可选，默认关闭）

`cpArbiterPreStepSplit()` 把穿透量转换为独立的伪速度目标
（`v_bias/w_bias` 通道），与物理速度通道 `v/w` 分离累积，避免 Baumgarte
把位置修正能量注入物理速度。冲量更新仍复用同一套 PGS 算术，仅偏置目标的
构造不同。该模式是 **opt-in**，默认配置完全不启用。

### 2.2 数值诊断与错误码

`cpSpace` 新增 `lastSolverError`，每步开头复位，问题首次出现时粘滞记录并
触发可选回调，不再 `abort()`：

| 错误码 | 触发条件 |
| --- | --- |
| `CP_SOLVER_NAN_BODY (1)` | 位置/速度/角度出现 NaN |
| `CP_SOLVER_INFINITE_BODY (2)` | 位置/速度/角度出现 Inf |
| `CP_SOLVER_INVALID_MASS (3)` | 质量/转动惯量为 NaN 或负 |
| `CP_SOLVER_DEGENERATE_CONTACT (4)` | 接触有效质量为零/非有限 |
| `CP_SOLVER_EXTREME_MASS_RATIO (5)` | 接触质量比超过 `maxMassRatio` |
| `CP_SOLVER_NAN_IMPULSE (6)` | 迭代产生非有限冲量 |
| `CP_SOLVER_DID_NOT_CONVERGE (7)` | 早停开启但残差仍显著超限（警告级） |

`cpBodySetMass/cpBodySetMoment` 对非法值改为“保留并置逆质量为 0”，使对象
仍可构造、在 `cpSpaceStep` 中被诊断，而不是在构造期崩溃。

## 3. 对外行为不变的保证

- 所有既有公共函数签名、结构体字段、语义保持不变；仅在 `cpSpace` 末尾
  追加字段，并新增独立头文件与函数（纯增量 API）。
- 默认 `cpSolverConfig` 复刻旧管线：Baumgarte 修正、暖启动开启、固定迭代、
  早停关闭。
- 默认热路径上的接触冲量更新仍调用原始 `cpArbiterApplyImpulse()`，其函数
  体算术逐行保留；带残差测量的 `cpArbiterApplyImpulseResidual()` 仅在开启
  早停时才使用，因此默认路径零额外浮点运算。
- 构建系统把 `-ffast-math` 改为 `-fno-fast-math`，并在
  `cpSpaceComponent.c`/`cpSolver.c` 加入 `#if defined(__FAST_MATH__) #error`
  编译期防护，防止哨兵比较再次被错误折叠。

## 4. 文件清单

新增：
- `include/chipmunk/cpSolver.h`
- `src/cpSolver.c`
- `tests/cp_solver_testbed.h`、`tests/cp_solver_verify.c`、
  `tests/cp_solver_modes.c`、`tests/cp_solver_diagnostics.c`、
  `tests/cp_solver_render.h`、`tests/cp_solver_record.c`、
  `tests/CMakeLists.txt`、`tests/run_acceptance.sh`

修改：
- `CMakeLists.txt`（去掉 `-ffast-math`，接入 tests 子目录与 CTest）
- `src/cpSpaceStep.c`（管线化 + 诊断挂载）
- `src/cpArbiter.c`（分裂预条件、残差变体，保留原函数）
- `src/cpSpace.c`、`src/cpBody.c`、`src/cpSpaceComponent.c`
- `include/chipmunk/chipmunk.h`、`chipmunk_private.h`、`chipmunk_structs.h`
