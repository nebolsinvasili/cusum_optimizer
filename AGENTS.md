# AGENTS.md

Standalone C++17 CLI (Monte-Carlo ARL calculator for two-sided CUSUM control charts with sign SN and signed-rank SR statistics). Single executable, no git repo, no CI, no test framework wired in.

## Build & run
- Build: `make` (outputs `bin/cusum`). Debug: `make debug`.
- Common runs: `make run-config` (uses `config/default_config.json`), `make run-quick` (1000 sims, coarse grid).
- CLI: `./bin/cusum --chart SN|SR --simulations N --k_start s e step --H_start s e step --target_ARL X --resume`. Full option table + semantics in `docs/API.md`.
- Validation: `make validate` (alias `make test`) builds `tests/validate` from `tests/validate.cpp` + `build/Simulator.o` + `build/Utils.o` and checks the Monte-Carlo ARL against the **exact two-sided Markov-chain ARL** (book method Qiu ch. 4.2.2, coupled chain over (C+,C-) states); non-zero exit on failure. Verify changes with `make validate && make`.

## Gotchas
- Requires g++ with C++17 + OpenMP and the **nlohmann/json** header (`-I/usr/include`). Build fails if the header isn't installed.
- `make clean-all` deletes **all** `*.log *.csv *.txt *.png *.pdf` in the repo root — results are written into the CWD, not a separate dir.
- `ResultManager` (`src/ResultManager.cpp`, `include/ResultManager.hpp`) is **not in the Makefile's `SOURCES`** — never compiled/linked. If you add it, add it to `SOURCES` or remove it.
- Default (no-arg) run loads `config/default_config.json` unless CLI args are given; `resume` is force-set to `false` there.
- Fresh-start deletes stale `temp`/`checkpoint` files listed in the JSON config; `--resume` only works if the temp CSV still exists. Return code is always `0` regardless of errors.
- The default k/H grid `[3.0, 7.0]` is sized for SN. For `--chart SR` the statistic is `Σ sgn(x)rank(|x|)` with scale `±n(n+1)/2` — set k/H ranges explicitly or the run warns and produces degenerate ARLs.
- Monte-Carlo per-pair at defaults (5000 sims × 41×41 grid) is CPU-heavy; parallelized via OpenMP (`n_cores <= 0` = auto).
- `tests/validate` asserts MC ARL vs the exact two-sided Markov-chain reference on moderate ARL cases (tol ±5%) plus both book design points: SN (n=10, k=6, h=4 → ~465, tol 15% of nominal 500) and SR (n=10, k=41, h=24 → the book's Appendix-B program effectively uses h_eff = min(h, M−k) = 14 → ARL ~486.9 ≈ nominal 500; asserted. Literal h=24 gives ~8.06e4 — printed as INFO (degenerate: h beyond reachable SR states).
- `RandomGenerator` is a **thread-local** singleton (src/Utils.cpp `getInstance()`): OpenMP cores must never share one RNG (race corrupts draws and biases ARL low). Each core gets its own deterministic seed in `calculateARLParallel`. Keep it thread-local in any RNG changes.

## Docs
- `README.md` (Russian) — the full manual. `docs/API.md` — CLI, JSON schema, CSV formats, error handling. Config schema lives in `config/default_config.json` and is duplicated in `Config` (include/Config.hpp).
- Source comments, README, and API.md are in Russian — match that for user-facing output.
