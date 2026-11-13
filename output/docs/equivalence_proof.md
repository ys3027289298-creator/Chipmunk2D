# 广相重构：碰撞对集合等价性证明

## 1. 系统边界与不变量

碰撞对的产生链路在重构前后保持不变：

```
cpSpaceStep()
  └─ cpSpatialIndexEach(dynamicShapes, cpShapeUpdateFunc)   // 刷新所有动态形状 BB
  └─ cpSpatialIndexReindexQuery(dynamicShapes, cpSpaceCollideShapes, space)
        ├─ 动态-动态候选对（后端内部生成）
        └─ cpSpatialIndexCollideStatic(...)                 // 动态-静态候选对
```

`cpSpaceStep`、`cpHastySpaceStep`、`cpSpaceCollideShapes`、`QueryReject`、
窄相 `cpCollide`、arbiter 缓存与四个回调（begin/preSolve/postSolve/separate）
所在文件**均未修改**（见 `git diff --name-only`，不含 `cpSpaceStep.c`、
`cpHastySpace.c`、`cpCollision.c`、`cpArbiter.c`）。

两个广相后端（`cpBBTree` 与 `cpSpaceHash`）都实现同一个虚表
`cpSpatialIndexClass`，对上层只暴露同一组入口：`insert/remove/reindex/
reindexObject/reindexQuery/query/segmentQuery/each/count/contains/destroy`。
因此后端可替换，而调度方式与回调挂载点不变。

## 2. 候选对集合的语义等价

定义形状集合 `S`，形状 `x` 的包围盒 `BB(x)` 由同一个 `bbfunc`
（`cpShapeGetBB`）计算。

### 2.1 空间哈希（默认后端）

`cpSpaceHashReindexQuery` 先 `clearTable`，再对每个对象按其 BB 覆盖的所有
网格单元重新插入；`query_helper` 通过单元链表与每对象时间戳 `stamp`
保证：同一对象在每个单元只出现一次，且任意两个共享至少一个单元的对象
`(a,b)` 的查询回调恰好触发一次。两个对象共享单元当且仅当存在一个单元同时
与两者 BB 相交。

### 2.2 AABB 树（旧默认后端，保留为可选项）

`cpBBTreeReindexQuery` 经 `MarkSubtree/MarkLeafQuery` 遍历树，仅当
`cpBBIntersects(leaf->bb, subtree->bb)` 时下钻；叶节点相遇即对
`(leaf, subtree)` 触发一次查询回调，`PairInsert`/配对表保证每对一次。

### 2.3 共同的精确集合

对网格单元 `C`，若 `BB(a)∩C≠∅` 且 `BB(b)∩C≠∅`，因单元为轴对齐凸区域，
可推出 `BB(a)` 与 `BB(b)` 在该单元覆盖范围内相交，即
`cpBBIntersects(BB(a), BB(b))` 为真。反之，若两个 BB 相交，相交矩形
`BB(a)∩BB(b)` 非空且至少覆盖一个网格单元，二者必然共享该单元。因此：

> 后端输出的**候选对集合**都精确等于
> `C = { {a,b} ⊆ S : a≠b, cpBBIntersects(BB(a), BB(b)) }`，
> 动态-静态部分由公共的 `cpSpatialIndexCollideStatic` 以同一谓词生成。

两后端对每对都只回调一次（去重机制不同：哈希用 per-query `stamp`，
树用配对表 `Pair`），故候选对是**集合**语义，与遍历顺序无关。

### 2.4 最终碰撞对（含过滤与窄相）

候选对进入同一个 `cpSpaceCollideShapes`：

1. `QueryReject` 用相同的 BB 相交、同体、filter、约束规则过滤；
2. 通过者进入同一个窄相 `cpCollide`，接触数为 0 则丢弃；
3. 其余以 `CP_HASH_PAIR` 写入同一 arbiter 缓存并触发同一组处理器回调。

由于输入候选集合 `C` 相同、过滤与窄相函数相同、确定性浮点运算，
**最终碰撞对集合与回调触发集合相同**。回调的相对先后由后端遍历顺序决定，
可能不同；但题目约束的是“碰撞对集合”与“回调时机”（在 step 中的调用阶段），
二者均保持。测试因此同时比对：排序后的逐帧碰撞对集合，以及每帧
begin/preSolve/postSolve/separate 的计数序列。

## 3. 自动化验证

`output/tests/equiv_test.c` 用确定性 RNG 构造同一 400 动态形状 + 静态边界的
场景，跑 60 步：

- **进程内对比**：同一二进制分别用空间哈希与 AABB 树，逐帧比对排序后的
  碰撞对集合与四类回调计数。
- **跨二进制对比**：改造前基线库（默认 AABB 树）与改造后新库（默认空间
  哈希）分别输出逐帧 trace，`diff` 必须完全一致。

结果（`output/acceptance_log.txt`）：两种对比均 `PASS`，60 步共
1736 个逐帧 pair 快照完全一致，跨二进制 trace 零差异。

## 4. 对外 API 与回调时机

- `include/chipmunk/` 下公开头文件零改动（`git diff --name-only -- include/`
  为空）。
- 静态库导出符号只增不删：新增 `cpBroadPhase*` 内部符号（声明于内部头
  `src/cpBroadPhase.h`），无任何既有符号被移除。
- `cpSpaceUseSpatialHash(dim,count)` 签名与语义保留，仍是公开的哈希切换入口。
- step 内的调用阶段（积分 → 广相/窄相/回调 → 组件处理 → 求解 → 积分）
  未改动，故回调时机不变。

## 5. 内存上界（线性）

默认空间哈希的每对象成本：

- 每个对象一个 `cpHandle`（`handleSet`，`cpHashSet` 负载到 1 即扩容，表 O(n)）；
- 每个“对象×占用单元”一个 `cpSpaceHashBin`。普通尺寸形状相对单元只覆盖
  常数个单元，故 bin 总数 O(n)；
- 单元表 `table` 经新增的 `cpSpaceHashGrowIfNeeded` 在对象数超过单元数时
  扩容到 `next_prime(n)`，维持负载因子 ≤ 1，表 O(n)。

因此总空间 O(n)。实测（`output/bench/`）sparse 场景 live/shape 在
n→16000 时收敛到约 615 B/形状，总量随 n 近似线性；哈希表扩容实测见
`output/bench/hash_growth.txt`（cells/n 恒为 1.000）。

## 6. 退化输入拒绝

新增构造期硬断言（`cpAssertHard`，Release/Debug 均生效）：

- 圆：半径必须有限且 `>0`，offset 坐标必须有限；
- 线段：半径有限非负，端点有限且不重合；
- 多边形：顶点数 ≥3、坐标有限、无重复点，凸包结果不足 3 个顶点
  （共线/重合退化）即拒绝。

因 Release 使用 `-ffast-math`（含 `-ffinite-math-only`），常规
`isfinite/isnan` 会被常量折叠，`cpValidate.h` 改用 IEEE-754 位模式判定，
对 fast-math 免疫。验证见 `output/tests/degenerate_test.c`。
