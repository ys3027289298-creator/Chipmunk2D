# 求解器重构产物导航

本目录包含堆叠求解器整体重构的全部验收产物。

## 目录

- `docs/design.md` — 统一可配置求解流程的设计说明（含问题根因、配置项、错误码）。
- `docs/equivalence_proof.md` — 改造前后等价性证明（代码路径对应 + 位级测试 + 性能论证）。
- `docs/acceptance_report.md` — 验收报告：稳定性数据、轨迹误差对比、异常诊断、录屏章节。
- `data/baseline_metrics.txt` — 纯净基线（git HEAD 旧求解器）原始指标。
- `data/refactor_default_metrics.txt` — 重构库默认配置指标（S1–S4 与基线逐位相同）。
- `data/refactor_modes.txt` — legacy 与稳定（分裂位置修正）配置对比。
- `data/diagnostics.txt` — NaN/极端质量比诊断用例结果（9 项全通过）。
- `data/performance.csv` — 求解耗时基准（µs/步）。
- `video/solver_refactor_recording.mp4` — 带分段标题的录屏（4 章 × 4 秒，H.264）。
- `video/frame_*.png` — 各章节抽帧。
- `build/` — 验证二进制、可复现帧流（gzip）与纯净基线源码快照。

## 一键复现

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_DEMOS=OFF -DBUILD_SHARED=OFF
cmake --build build -j
ctest --test-dir build --output-on-failure     # 仓库自带测试入口
./tests/run_acceptance.sh                       # 完整对比 + 录屏，产物写入本目录
```

## 录屏章节

| 时间 | 标题 |
| --- | --- |
| 0:00 | S1 STACK STABILITY - LEGACY |
| 0:04 | S2 STACK STABILITY - STABLE |
| 0:08 | S3 TRAJECTORY ERROR BOUND |
| 0:12 | S4 NUMERICAL DIAGNOSTICS |
