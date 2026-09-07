"""cusum — Python bindings for the CUSUM ARL optimizer."""
import ctypes
from dataclasses import dataclass
from typing import List, Optional, Tuple

from ._bindings import (
    _lib,
    CUSUM_SN,
    CUSUM_SR,
)


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

    def calculate_arl(self, k: float, H: float,
                      chart_type: Optional[str] = None) -> float:
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