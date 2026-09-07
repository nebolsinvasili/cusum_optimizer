# Python ctypes Bindings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create a Python package `cusum` that wraps the C++ CUSUM optimizer via ctypes, exposing Config, single-pair ARL, distribution-aware ARL, and full grid search as a pip-installable library.

**Architecture:** Three layers: (1) `extern "C"` bridge in `src/bridge.cpp` exposing C++ classes as opaque pointers + flat C functions; (2) shared library `lib/libcusum.so` compiled from bridge + all C++ sources; (3) Python package `python/cusum/` using ctypes FFI returning dataclasses.

**Tech Stack:** C++17, g++ with OpenMP, ctypes (Python stdlib), nlohmann/json (already used), pyproject.toml for packaging.

**Spec:** Design approved in chat — see conversation above for full API design.

## File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `include/bridge.h` | CREATE | C API header — opaque handles, structs, extern "C" functions |
| `src/bridge.cpp` | CREATE | C API implementation — bridges C functions to C++ Calculator/Config/Simulator |
| `src/Calculator.cpp` | MODIFY:476-479 | Add `getResults()` public accessor returning `results_` vector |
| `include/Calculator.hpp` | MODIFY:33 | Add `getResults()` declaration |
| `Makefile` | MODIFY | Add `lib` target for shared library, `bridge.o` to SOURCES |
| `python/cusum/__init__.py` | CREATE | Public API — Config, Result, Distribution, Cusum classes |
| `python/cusum/_bindings.py` | CREATE | ctypes FFI — load .so, define argtypes/restypes for all C functions |
| `python/cusum/_utils.py` | CREATE | Library path discovery helper |
| `pyproject.toml` | CREATE | pip-installable package config |

---

## Task 1: Add `getResults()` accessor to Calculator

**Files:**
- Modify: `include/Calculator.hpp:33`
- Modify: `src/Calculator.cpp:476-479`

**Interfaces:**
- Consumes: `results_` private member (vector<ResultEntry>)
- Produces: `const std::vector<ResultEntry>& getResults() const` — used by bridge.cpp

- [ ] **Step 1: Add declaration to header**

In `include/Calculator.hpp`, add after line 35 (after `resume` declaration):

```cpp
    /// Возвращает все накопленные результаты (для внешних API: bridge, Python).
    const std::vector<ResultEntry>& getResults() const;
```

- [ ] **Step 2: Add implementation to Calculator.cpp**

In `src/Calculator.cpp`, add before the closing `} // namespace cusum` (line 479):

```cpp
const std::vector<ResultEntry>& Calculator::getResults() const {
    return results_;
}
```

- [ ] **Step 3: Verify it compiles**

Run: `make`
Expected: BUILD OK, no errors.

- [ ] **Step 4: Commit**

```bash
git add include/Calculator.hpp src/Calculator.cpp
git commit -m "feat: add getResults() accessor to Calculator for external API"
```

---

## Task 2: Create C API header (`include/bridge.h`)

**Files:**
- Create: `include/bridge.h`

**Interfaces:**
- Consumes: Config struct fields, Calculator::run(), Simulator functions
- Produces: Opaque handles (void*), CusumResult/CusumResultArray structs, extern "C" functions

- [ ] **Step 1: Create the header file**

Create `include/bridge.h` with this exact content:

```c
#ifndef CUSUM_BRIDGE_H
#define CUSUM_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { CUSUM_SN = 0, CUSUM_SR = 1 } CusumChartType;

typedef struct {
    double k;
    double H;
    double ARL;
    double deviation;
} CusumResult;

typedef struct {
    CusumResult* results;
    int count;
} CusumResultArray;

/* Opaque handle to Config */
typedef void* CusumConfig;

/* Opaque handle to Calculator */
typedef void* CusumCalculator;

/* Config lifecycle */
CusumConfig cusum_config_create(void);
void cusum_config_destroy(CusumConfig cfg);
int  cusum_config_from_json(CusumConfig cfg, const char* json_str);

/* Config field setters (return 0 on success, -1 on bad input) */
int cusum_config_set_simulations(CusumConfig cfg, int value);
int cusum_config_set_n(CusumConfig cfg, int value);
int cusum_config_set_max_iter(CusumConfig cfg, int value);
int cusum_config_set_target_arl(CusumConfig cfg, double value);
int cusum_config_set_n_cores(CusumConfig cfg, int value);
int cusum_config_set_tolerance(CusumConfig cfg, double value);
int cusum_config_set_chart_type(CusumConfig cfg, CusumChartType chart);
int cusum_config_set_k_range(CusumConfig cfg, double start, double end, double step);
int cusum_config_set_h_range(CusumConfig cfg, double start, double end, double step);
int cusum_config_set_top_n(CusumConfig cfg, int value);
int cusum_config_set_temp_file(CusumConfig cfg, const char* path);
int cusum_config_set_checkpoint_file(CusumConfig cfg, const char* path);
int cusum_config_set_final_file(CusumConfig cfg, const char* path);
int cusum_config_set_best_file(CusumConfig cfg, const char* path);
int cusum_config_set_log_file(CusumConfig cfg, const char* path);
int cusum_config_set_error_file(CusumConfig cfg, const char* path);
int cusum_config_set_checkpoint_interval(CusumConfig cfg, int value);
int cusum_config_set_update_interval(CusumConfig cfg, int value);

/* Single-pair ARL calculation */
double cusum_calculate_arl(CusumConfig cfg, double k, double H, CusumChartType chart);

/* Single-pair ARL with distribution stats.
   stats_out must point to array of at least 10 doubles.
   Returns ARL, fills stats_out with: [ARL, mean, stddev, p5, p25, p50, p75, p95, min, max]. */
double cusum_calculate_arl_dist(CusumConfig cfg, double k, double H,
                                CusumChartType chart, double* stats_out);

/* Full grid search — returns array of results. Caller must free with cusum_free_results. */
CusumResultArray cusum_run_grid(CusumConfig cfg);

/* Free result array returned by cusum_run_grid. */
void cusum_free_results(CusumResultArray arr);

#ifdef __cplusplus
}
#endif

#endif /* CUSUM_BRIDGE_H */
```

- [ ] **Step 2: Verify header compiles as C**

Run: `echo '#include "bridge.h"' | g++ -std=c++17 -fsyntax-only -x c++ -I include -`
Expected: No errors.

- [ ] **Step 3: Commit**

```bash
git add include/bridge.h
git commit -m "feat: add C API header for Python ctypes bindings"
```

---

## Task 3: Implement C API bridge (`src/bridge.cpp`)

**Files:**
- Create: `src/bridge.cpp`

**Interfaces:**
- Consumes: `bridge.h` (Task 2), `Calculator.hpp`, `Config.hpp`, `Simulator.hpp`
- Produces: Compiled bridge object linked into libcusum.so (Task 5)

- [ ] **Step 1: Create bridge.cpp**

Create `src/bridge.cpp` with this exact content:

```cpp
#include "bridge.h"
#include "Config.hpp"
#include "Calculator.hpp"
#include "Simulator.hpp"
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

static cusum::ChartType to_chart(CusumChartType c) {
    return (c == CUSUM_SR) ? cusum::ChartType::SR : cusum::ChartType::SN;
}

extern "C" {

CusumConfig cusum_config_create(void) {
    auto* cfg = new(std::nothrow) cusum::Config();
    return static_cast<CusumConfig>(cfg);
}

void cusum_config_destroy(CusumConfig cfg) {
    if (cfg) {
        delete static_cast<cusum::Config*>(cfg);
    }
}

int cusum_config_from_json(CusumConfig cfg, const char* json_str) {
    if (!cfg || !json_str) return -1;
    // Write JSON to temp file, load via Config::loadFromFile
    // This is simplest approach given Config already supports JSON files.
    const char* tmp = "/tmp/cusum_config_tmp.json";
    FILE* f = fopen(tmp, "w");
    if (!f) return -1;
    fputs(json_str, f);
    fclose(f);
    try {
        *static_cast<cusum::Config*>(cfg) = cusum::Config::loadFromFile(tmp);
        remove(tmp);
        return 0;
    } catch (...) {
        remove(tmp);
        return -1;
    }
}

int cusum_config_set_simulations(CusumConfig cfg, int value) {
    if (!cfg || value <= 0) return -1;
    static_cast<cusum::Config*>(cfg)->simulations = value;
    return 0;
}

int cusum_config_set_n(CusumConfig cfg, int value) {
    if (!cfg || value <= 0) return -1;
    static_cast<cusum::Config*>(cfg)->n = value;
    return 0;
}

int cusum_config_set_max_iter(CusumConfig cfg, int value) {
    if (!cfg || value <= 0) return -1;
    static_cast<cusum::Config*>(cfg)->max_iter = value;
    return 0;
}

int cusum_config_set_target_arl(CusumConfig cfg, double value) {
    if (!cfg || value <= 0) return -1;
    static_cast<cusum::Config*>(cfg)->target_ARL = value;
    return 0;
}

int cusum_config_set_n_cores(CusumConfig cfg, int value) {
    if (!cfg) return -1;
    static_cast<cusum::Config*>(cfg)->n_cores = value;
    return 0;
}

int cusum_config_set_tolerance(CusumConfig cfg, double value) {
    if (!cfg) return -1;
    static_cast<cusum::Config*>(cfg)->tolerance = value;
    return 0;
}

int cusum_config_set_chart_type(CusumConfig cfg, CusumChartType chart) {
    if (!cfg) return -1;
    static_cast<cusum::Config*>(cfg)->chart_type = (chart == CUSUM_SR) ? "SR" : "SN";
    return 0;
}

int cusum_config_set_k_range(CusumConfig cfg, double start, double end, double step) {
    if (!cfg || step <= 0 || start > end) return -1;
    auto* c = static_cast<cusum::Config*>(cfg);
    c->k_values.clear();
    for (double v = start; v <= end + step * 0.5; v += step) {
        c->k_values.push_back(v);
    }
    return 0;
}

int cusum_config_set_h_range(CusumConfig cfg, double start, double end, double step) {
    if (!cfg || step <= 0 || start > end) return -1;
    auto* c = static_cast<cusum::Config*>(cfg);
    c->H_values.clear();
    for (double v = start; v <= end + step * 0.5; v += step) {
        c->H_values.push_back(v);
    }
    return 0;
}

int cusum_config_set_top_n(CusumConfig cfg, int value) {
    if (!cfg || value <= 0) return -1;
    static_cast<cusum::Config*>(cfg)->top_n = value;
    return 0;
}

int cusum_config_set_temp_file(CusumConfig cfg, const char* path) {
    if (!cfg || !path) return -1;
    static_cast<cusum::Config*>(cfg)->temp_file = path;
    return 0;
}

int cusum_config_set_checkpoint_file(CusumConfig cfg, const char* path) {
    if (!cfg || !path) return -1;
    static_cast<cusum::Config*>(cfg)->checkpoint_file = path;
    return 0;
}

int cusum_config_set_final_file(CusumConfig cfg, const char* path) {
    if (!cfg || !path) return -1;
    static_cast<cusum::Config*>(cfg)->final_file = path;
    return 0;
}

int cusum_config_set_best_file(CusumConfig cfg, const char* path) {
    if (!cfg || !path) return -1;
    static_cast<cusum::Config*>(cfg)->best_file = path;
    return 0;
}

int cusum_config_set_log_file(CusumConfig cfg, const char* path) {
    if (!cfg || !path) return -1;
    static_cast<cusum::Config*>(cfg)->log_file = path;
    return 0;
}

int cusum_config_set_error_file(CusumConfig cfg, const char* path) {
    if (!cfg || !path) return -1;
    static_cast<cusum::Config*>(cfg)->error_file = path;
    return 0;
}

int cusum_config_set_checkpoint_interval(CusumConfig cfg, int value) {
    if (!cfg || value <= 0) return -1;
    static_cast<cusum::Config*>(cfg)->checkpoint_interval = value;
    return 0;
}

int cusum_config_set_update_interval(CusumConfig cfg, int value) {
    if (!cfg || value <= 0) return -1;
    static_cast<cusum::Config*>(cfg)->update_interval = value;
    return 0;
}

double cusum_calculate_arl(CusumConfig cfg, double k, double H, CusumChartType chart) {
    if (!cfg) return -1.0;
    auto* c = static_cast<cusum::Config*>(cfg);
    return cusum::calculateARLParallel(k, H, c->simulations, c->n,
                                       c->max_iter, c->n_cores, to_chart(chart));
}

double cusum_calculate_arl_dist(CusumConfig cfg, double k, double H,
                                CusumChartType chart, double* stats_out) {
    if (!cfg || !stats_out) return -1.0;
    auto* c = static_cast<cusum::Config*>(cfg);
    auto [arl, lengths, stats] = cusum::calculateARLParallelWithDistribution(
        k, H, c->simulations, c->n, c->max_iter, c->n_cores, to_chart(chart));
    // stats vector: [ARL, mean, stddev, p5, p25, p50, p75, p95, min, max]
    int count = std::min(static_cast<int>(stats.size()), 10);
    for (int i = 0; i < count; ++i) stats_out[i] = stats[i];
    return arl;
}

CusumResultArray cusum_run_grid(CusumConfig cfg) {
    CusumResultArray empty = {nullptr, 0};
    if (!cfg) return empty;

    auto* c = static_cast<cusum::Config*>(cfg);
    c->resume = false;

    cusum::Calculator calculator(*c);
    calculator.run(*c);

    const auto& results = calculator.getResults();
    if (results.empty()) return empty;

    CusumResultArray arr;
    arr.count = static_cast<int>(results.size());
    arr.results = static_cast<CusumResult*>(std::malloc(arr.count * sizeof(CusumResult)));
    if (!arr.results) return empty;

    for (int i = 0; i < arr.count; ++i) {
        arr.results[i].k = results[i].k;
        arr.results[i].H = results[i].H;
        arr.results[i].ARL = results[i].ARL;
        arr.results[i].deviation = std::abs(results[i].ARL - c->target_ARL);
    }
    return arr;
}

void cusum_free_results(CusumResultArray arr) {
    if (arr.results) {
        std::free(arr.results);
    }
}

} // extern "C"
```

- [ ] **Step 2: Verify it compiles**

Run: `make`
Expected: BUILD OK. `build/bridge.o` created, linked into `bin/cusum`.

- [ ] **Step 3: Commit**

```bash
git add src/bridge.cpp
git commit -m "feat: implement C API bridge for Python ctypes bindings"
```

---

## Task 4: Add shared library target to Makefile

**Files:**
- Modify: `Makefile`

**Interfaces:**
- Consumes: All .o files including bridge.o
- Produces: `lib/libcusum.so` shared library

- [ ] **Step 1: Add LIB_DIR and shared library target**

In `Makefile`, after line 9 (`CONFIG_DIR = config`), add:

```makefile
LIB_DIR = lib
SHARED_LIB = $(LIB_DIR)/libcusum.so
```

- [ ] **Step 2: Add bridge.cpp to SOURCES**

In the SOURCES list (line 13-19), add `$(SRC_DIR)/bridge.cpp \` after the `Utils.cpp` line:

```makefile
SOURCES = \
    $(SRC_DIR)/Calculator.cpp \
    $(SRC_DIR)/Config.cpp \
    $(SRC_DIR)/main.cpp \
    $(SRC_DIR)/ProgressBar.cpp \
    $(SRC_DIR)/Simulator.cpp \
    $(SRC_DIR)/Utils.cpp \
    $(SRC_DIR)/bridge.cpp
```

- [ ] **Step 3: Add shared library build rules**

After the `$(BUILD_DIR):` rule (line 44), add:

```makefile
$(LIB_DIR):
	@mkdir -p $(LIB_DIR)

$(SHARED_LIB): $(OBJECTS) | $(LIB_DIR)
	@echo "[LINK] Building shared library: $@"
	$(CXX) -shared -fPIC -o $@ $^ $(LDFLAGS)
	@echo "[OK] Shared library: $(SHARED_LIB)"

lib: $(SHARED_LIB)
```

- [ ] **Step 4: Add `lib` to .PHONY**

Update line 27 to include `lib`:

```makefile
.PHONY: all clean clean-results clean-logs clean-csv clean-all distclean help run run-config run-quick run-detailed debug validate test lib
```

- [ ] **Step 5: Add clean rule for lib**

In the `clean` target (line 100-103), add after `@rm -f $(BIN_DIR)/cusum_sn $(VALIDATE_BIN)`:

```makefile
	@rm -rf $(LIB_DIR)
```

- [ ] **Step 6: Build and verify**

Run: `make lib`
Expected: `lib/libcusum.so` created successfully.

- [ ] **Step 7: Verify shared library exports symbols**

Run: `nm -D lib/libcusum.so | grep cusum_`
Expected: All `cusum_*` functions listed as T (text/code) symbols.

- [ ] **Step 8: Commit**

```bash
git add Makefile
git commit -m "feat: add shared library build target (make lib)"
```

---

## Task 5: Create Python package structure and ctypes bindings

**Files:**
- Create: `python/cusum/_utils.py`
- Create: `python/cusum/_bindings.py`
- Create: `python/cusum/__init__.py`

**Interfaces:**
- Consumes: `lib/libcusum.so` (Task 4), `include/bridge.h` (Task 2)
- Produces: Python package importable as `import cusum`

- [ ] **Step 1: Create directory structure**

Run: `mkdir -p python/cusum`

- [ ] **Step 2: Create `python/cusum/_utils.py`**

```python
"""Library path discovery for libcusum.so."""
import os
from pathlib import Path

def find_library() -> str:
    """Find libcusum.so relative to this package or in standard paths."""
    # Try relative to this file: ../../lib/libcusum.so
    pkg_dir = Path(__file__).resolve().parent
    project_root = pkg_dir.parent.parent
    lib_path = project_root / "lib" / "libcusum.so"
    if lib_path.exists():
        return str(lib_path)

    # Try CUSUM_LIB_PATH environment variable
    env_path = os.environ.get("CUSUM_LIB_PATH")
    if env_path and Path(env_path).exists():
        return env_path

    # Try system paths
    for search in ["/usr/local/lib", "/usr/lib", Path.home() / "lib"]:
        p = Path(search) / "libcusum.so"
        if p.exists():
            return str(p)

    raise FileNotFoundError(
        "Cannot find libcusum.so. Build with 'make lib' or set CUSUM_LIB_PATH."
    )
```

- [ ] **Step 3: Create `python/cusum/_bindings.py`**

```python
"""Raw ctypes bindings to libcusum.so."""
import ctypes
from pathlib import Path
from ._utils import find_library

_lib = ctypes.CDLL(find_library())

# --- Enums ---
CUSUM_SN = 0
CUSUM_SR = 1

# --- Structs ---
class CusumResult(ctypes.Structure):
    _fields_ = [
        ("k", ctypes.c_double),
        ("H", ctypes.c_double),
        ("ARL", ctypes.c_double),
        ("deviation", ctypes.c_double),
    ]

class CusumResultArray(ctypes.Structure):
    _fields_ = [
        ("results", ctypes.POINTER(CusumResult)),
        ("count", ctypes.c_int),
    ]

# --- Config functions ---
_lib.cusum_config_create.restype = ctypes.c_void_p
_lib.cusum_config_create.argtypes = []

_lib.cusum_config_destroy.restype = None
_lib.cusum_config_destroy.argtypes = [ctypes.c_void_p]

_lib.cusum_config_from_json.restype = ctypes.c_int
_lib.cusum_config_from_json.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

# Config setters
for _name in [
    "cusum_config_set_simulations", "cusum_config_set_n",
    "cusum_config_set_max_iter", "cusum_config_set_n_cores",
    "cusum_config_set_top_n", "cusum_config_set_checkpoint_interval",
    "cusum_config_set_update_interval",
]:
    fn = getattr(_lib, _name)
    fn.restype = ctypes.c_int
    fn.argtypes = [ctypes.c_void_p, ctypes.c_int]

for _name in [
    "cusum_config_set_target_arl", "cusum_config_set_tolerance",
]:
    fn = getattr(_lib, _name)
    fn.restype = ctypes.c_int
    fn.argtypes = [ctypes.c_void_p, ctypes.c_double]

_lib.cusum_config_set_chart_type.restype = ctypes.c_int
_lib.cusum_config_set_chart_type.argtypes = [ctypes.c_void_p, ctypes.c_int]

_lib.cusum_config_set_k_range.restype = ctypes.c_int
_lib.cusum_config_set_k_range.argtypes = [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double]

_lib.cusum_config_set_h_range.restype = ctypes.c_int
_lib.cusum_config_set_h_range.argtypes = [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double]

for _name in [
    "cusum_config_set_temp_file", "cusum_config_set_checkpoint_file",
    "cusum_config_set_final_file", "cusum_config_set_best_file",
    "cusum_config_set_log_file", "cusum_config_set_error_file",
]:
    fn = getattr(_lib, _name)
    fn.restype = ctypes.c_int
    fn.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

# --- Core functions ---
_lib.cusum_calculate_arl.restype = ctypes.c_double
_lib.cusum_calculate_arl.argtypes = [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_int]

_lib.cusum_calculate_arl_dist.restype = ctypes.c_double
_lib.cusum_calculate_arl_dist.argtypes = [
    ctypes.c_void_p, ctypes.c_double, ctypes.c_double,
    ctypes.c_int, ctypes.POINTER(ctypes.c_double),
]

_lib.cusum_run_grid.restype = CusumResultArray
_lib.cusum_run_grid.argtypes = [ctypes.c_void_p]

_lib.cusum_free_results.restype = None
_lib.cusum_free_results.argtypes = [CusumResultArray]
```

- [ ] **Step 4: Create `python/cusum/__init__.py`**

```python
"""cusum — Python bindings for the CUSUM ARL optimizer."""
from dataclasses import dataclass
from typing import List, Optional, Tuple
import json
import tempfile
import os

from ._bindings import (
    _lib,
    CUSUM_SN,
    CUSUM_SR,
    CusumResultArray,
)
from ._bindings import CusumResult as _CusumResult


@dataclass
class Result:
    """One (k, H) pair result."""
    k: float
    H: float
    ARL: float
    deviation: float


@dataclass
class Distribution:
    """ARL distribution statistics."""
    ARL: float
    mean: float
    stddev: float
    p5: float
    p25: float
    p50: float
    p75: float
    p95: float
    min: float
    max: float


@dataclass
class Config:
    """CUSUM optimizer configuration."""
    simulations: int = 5000
    n: int = 14
    max_iter: int = 5000
    target_ARL: float = 370.0
    n_cores: int = 0
    tolerance: float = -1.0
    chart_type: str = "SN"
    k_range: Optional[Tuple[float, float, float]] = None
    H_range: Optional[Tuple[float, float, float]] = None
    top_n: int = 10
    temp_file: Optional[str] = None
    checkpoint_file: Optional[str] = None
    final_file: Optional[str] = None
    best_file: Optional[str] = None
    log_file: Optional[str] = None
    error_file: Optional[str] = None
    checkpoint_interval: int = 5
    update_interval: int = 5


class Cusum:
    """Main interface to the CUSUM ARL optimizer."""

    def __init__(self, config: Optional[Config] = None):
        self._handle = _lib.cusum_config_create()
        if not self._handle:
            raise RuntimeError("Failed to create CUSUM config")
        if config:
            self._apply_config(config)

    def _apply_config(self, cfg: Config) -> None:
        h = self._handle
        _lib.cusum_config_set_simulations(h, cfg.simulations)
        _lib.cusum_config_set_n(h, cfg.n)
        _lib.cusum_config_set_max_iter(h, cfg.max_iter)
        _lib.cusum_config_set_target_arl(h, cfg.target_ARL)
        _lib.cusum_config_set_n_cores(h, cfg.n_cores)
        _lib.cusum_config_set_tolerance(h, cfg.tolerance)
        _lib.cusum_config_set_top_n(h, cfg.top_n)
        _lib.cusum_config_set_checkpoint_interval(h, cfg.checkpoint_interval)
        _lib.cusum_config_set_update_interval(h, cfg.update_interval)

        chart = CUSUM_SR if cfg.chart_type.upper() == "SR" else CUSUM_SN
        _lib.cusum_config_set_chart_type(h, chart)

        if cfg.k_range:
            _lib.cusum_config_set_k_range(h, *cfg.k_range)
        if cfg.H_range:
            _lib.cusum_config_set_h_range(h, *cfg.H_range)

        if cfg.temp_file:
            _lib.cusum_config_set_temp_file(h, cfg.temp_file.encode())
        if cfg.checkpoint_file:
            _lib.cusum_config_set_checkpoint_file(h, cfg.checkpoint_file.encode())
        if cfg.final_file:
            _lib.cusum_config_set_final_file(h, cfg.final_file.encode())
        if cfg.best_file:
            _lib.cusum_config_set_best_file(h, cfg.best_file.encode())
        if cfg.log_file:
            _lib.cusum_config_set_log_file(h, cfg.log_file.encode())
        if cfg.error_file:
            _lib.cusum_config_set_error_file(h, cfg.error_file.encode())

    def calculate_arl(self, k: float, H: float, chart_type: Optional[str] = None) -> float:
        """Calculate ARL for a single (k, H) pair."""
        chart = CUSUM_SN
        if chart_type and chart_type.upper() == "SR":
            chart = CUSUM_SR
        result = _lib.cusum_calculate_arl(self._handle, k, H, chart)
        if result < 0:
            raise RuntimeError(f"ARL calculation failed for k={k}, H={H}")
        return result

    def calculate_arl_distribution(
        self, k: float, H: float, chart_type: Optional[str] = None
    ) -> Distribution:
        """Calculate ARL with full distribution statistics for a single (k, H) pair."""
        chart = CUSUM_SN
        if chart_type and chart_type.upper() == "SR":
            chart = CUSUM_SR
        stats = (ctypes.c_double * 10)()
        arl = _lib.cusum_calculate_arl_dist(self._handle, k, H, chart, stats)
        if arl < 0:
            raise RuntimeError(f"ARL distribution calculation failed for k={k}, H={H}")
        return Distribution(
            ARL=arl,
            mean=stats[1],
            stddev=stats[2],
            p5=stats[3],
            p25=stats[4],
            p50=stats[5],
            p75=stats[6],
            p95=stats[7],
            min=stats[8],
            max=stats[9],
        )

    def run(self) -> List[Result]:
        """Run full grid search. Returns list of Result objects."""
        arr = _lib.cusum_run_grid(self._handle)
        results = []
        try:
            for i in range(arr.count):
                r = arr.results[i]
                results.append(Result(k=r.k, H=r.H, ARL=r.ARL, deviation=r.deviation))
        finally:
            _lib.cusum_free_results(arr)
        return results

    def best_pair(self) -> Optional[Result]:
        """Run grid search and return the pair closest to target ARL."""
        results = self.run()
        if not results:
            return None
        return min(results, key=lambda r: r.deviation)

    def load_json(self, json_str: str) -> None:
        """Load configuration from a JSON string."""
        ret = _lib.cusum_config_from_json(self._handle, json_str.encode())
        if ret != 0:
            raise ValueError("Failed to parse JSON config")

    def __del__(self) -> None:
        if hasattr(self, "_handle") and self._handle:
            _lib.cusum_config_destroy(self._handle)
```

- [ ] **Step 5: Verify Python import works**

Run: `cd python && python -c "from cusum import Cusum, Config, Result, Distribution; print('Import OK')"`
Expected: `Import OK`

- [ ] **Step 6: Test single ARL calculation**

Run: `cd python && python -c "
from cusum import Cusum, Config
cfg = Config(simulations=1000, n=14, k_range=(5.0,6.0,0.5), H_range=(4.0,5.0,0.5))
c = Cusum(cfg)
arl = c.calculate_arl(5.5, 4.5)
print(f'ARL = {arl:.2f}')
assert arl > 0, 'ARL must be positive'
print('Single-pair test OK')
"`
Expected: ARL printed and positive, no errors.

- [ ] **Step 7: Commit**

```bash
git add python/cusum/
git commit -m "feat: add Python cusum package with ctypes bindings"
```

---

## Task 6: Create pyproject.toml for pip installation

**Files:**
- Create: `pyproject.toml`

**Interfaces:**
- Consumes: `python/cusum/` package (Task 5), `lib/libcusum.so` (Task 4)
- Produces: pip-installable package

- [ ] **Step 1: Create pyproject.toml**

Create `pyproject.toml` at project root:

```toml
[build-system]
requires = ["setuptools>=64"]
build-backend = "setuptools.backends._legacy:_Backend"

[project]
name = "cusum-optimizer"
version = "1.0.0"
description = "Python bindings for the CUSUM ARL optimizer (Monte-Carlo simulation)"
requires-python = ">=3.8"
license = {text = "MIT"}
classifiers = [
    "Development Status :: 4 - Beta",
    "Intended Audience :: Science/Research",
    "Topic :: Scientific/Engineering :: Mathematics",
    "Programming Language :: Python :: 3",
    "Programming Language :: C++",
]

[tool.setuptools.packages.find]
where = ["python"]

[tool.setuptools.package-data]
cusum = ["*.so", "*.dylib", "*.dll"]
```

- [ ] **Step 2: Add install instructions to README-style comment**

Add at top of `pyproject.toml`:

```toml
# Installation:
#   1. Build the shared library: make lib
#   2. Install Python package: pip install -e .
#   3. Or copy lib/libcusum.so to /usr/local/lib and pip install .
```

- [ ] **Step 3: Verify package builds**

Run: `pip install -e .`
Expected: Package installs successfully.

- [ ] **Step 4: Commit**

```bash
git add pyproject.toml
git commit -m "feat: add pyproject.toml for pip-installable Python package"
```

---

## Task 7: End-to-end verification

**Files:**
- Test only (no new files)

**Interfaces:**
- Consumes: All previous tasks
- Produces: Verified working Python bindings

- [ ] **Step 1: Run the validation test (C++ still works)**

Run: `make validate && make`
Expected: Validation passes, binary builds.

- [ ] **Step 2: Full Python integration test**

Run: `cd python && python -c "
from cusum import Cusum, Config, Result, Distribution

# Test 1: Config with defaults
cfg = Config(simulations=1000, n=14, k_range=(5.0,6.0,0.2), H_range=(4.0,5.0,0.2))
c = Cusum(cfg)

# Test 2: Single ARL
arl = c.calculate_arl(5.5, 4.5)
print(f'Test 2: ARL = {arl:.2f}')
assert arl > 0

# Test 3: Distribution
dist = c.calculate_arl_distribution(5.5, 4.5)
print(f'Test 3: ARL={dist.ARL:.2f}, mean={dist.mean:.2f}, p50={dist.p50:.2f}')
assert dist.ARL > 0
assert dist.p5 <= dist.p50 <= dist.p75

# Test 4: Grid search
c2 = Cusum(Config(simulations=500, n=14, k_range=(5.0,5.5,0.25), H_range=(4.0,4.5,0.25)))
results = c2.run()
print(f'Test 4: Grid returned {len(results)} results')
assert len(results) > 0
for r in results:
    assert r.ARL > 0

# Test 5: Best pair
best = c2.best_pair()
print(f'Test 5: Best k={best.k}, H={best.H}, ARL={best.ARL:.2f}, dev={best.deviation:.2f}')
assert best.deviation >= 0

print('All tests passed!')
"`
Expected: All 5 tests pass, output printed.

- [ ] **Step 3: Clean up test artifacts**

Run: `rm -f arl_*.csv arl_*.log arl_*.txt best_*.csv errors.log`

- [ ] **Step 4: Final commit**

```bash
git add -A
git commit -m "feat: complete Python ctypes bindings for CUSUM optimizer"
```
