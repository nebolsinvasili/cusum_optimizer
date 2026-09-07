"""cusum — CLI entry point (python -m cusum).

Python-аналог C++ CLI: перебор сетки (k, H) и вывод лучшей пары.
"""
import argparse
import sys
import os
import contextlib
from typing import List, Optional, Tuple

from . import Cusum, Config, Result


@contextlib.contextmanager
def _silent_stdout():
    """Глушит stdout на уровне fd (ловит и C++ printf/ProgressBar)."""
    saved = os.dup(1)
    null_fd = os.open(os.devnull, os.O_WRONLY)
    os.dup2(null_fd, 1)
    try:
        yield
    finally:
        os.dup2(saved, 1)
        os.close(saved)
        os.close(null_fd)


def _parse_range(args: Optional[List[str]]) -> Optional[Tuple[float, float, float]]:
    """Разбирает 'start end [step]' в (start, end, step)."""
    if not args:
        return None
    if len(args) < 2 or len(args) > 3:
        raise argparse.ArgumentTypeError("ожидается 'start end [step]'")
    start = float(args[0])
    end = float(args[1])
    step = float(args[2]) if len(args) == 3 else 0.1
    return (start, end, step)


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="cusum",
        description="Расчёт ARL двусторонних CUSUM-карт (SN/SR) и подбор (k, H).",
    )
    p.add_argument("--chart", choices=["SN", "SR"], default="SN",
                   help="Тип карты (по умолчанию SN)")
    p.add_argument("--simulations", type=int, default=5000,
                   help="Число прогонов Монте-Карло на пару (k, H)")
    p.add_argument("--n", type=int, default=14, help="Размер подгруппы")
    p.add_argument("--max_iter", type=int, default=5000,
                   help="Максимальная длина серии")
    p.add_argument("--target_ARL", type=float, default=370.0,
                   help="Целевой ARL")
    p.add_argument("--cores", type=int, default=0,
                   help="Число ядер CPU (0 = авто)")
    p.add_argument("--tolerance", type=float, default=-1.0,
                   help="Ранняя остановка при |ARL-target| <= tolerance (-1 = выкл)")
    p.add_argument("--top_n", type=int, default=10,
                   help="Сколько лучших пар выводить")
    p.add_argument("--k_start", nargs="+", metavar=("S", "E"),
                   help="Диапазон k: start end [step]")
    p.add_argument("--H_start", nargs="+", metavar=("S", "E"),
                   help="Диапазон H: start end [step]")
    p.add_argument("--config", type=str, default=None,
                   help="Путь к JSON-конфигу (опции CLI переопределяют его)")
    p.add_argument("--json", action="store_true",
                   help="Выводить результат в JSON на stdout")
    p.add_argument("--keep-files", action="store_true",
                   help="Не удалять CSV/log-файлы после запуска")
    return p


def main(argv: Optional[List[str]] = None) -> int:
    args = build_parser().parse_args(argv)

    config = Config(
        simulations=args.simulations,
        n=args.n,
        max_iter=args.max_iter,
        target_ARL=args.target_ARL,
        n_cores=args.cores,
        tolerance=args.tolerance,
        chart_type=args.chart,
        top_n=args.top_n,
    )

    k_range = _parse_range(args.k_start)
    h_range = _parse_range(args.H_start)
    if k_range is None:
        k_range = (3.0, 7.0, 0.1)
    if h_range is None:
        h_range = (3.0, 7.0, 0.1)
    config.k_range = k_range
    config.H_range = h_range

    if args.chart == "SR" and args.k_start is None:
        sys.stderr.write(
            "Предупреждение: для SR сетка k/H по умолчанию [3.0,7.0] непригодна; "
            "задайте --k_start/--H_start явно.\n"
        )

    if args.config:
        with open(args.config, "r") as f:
            json_text = f.read()
        cusum = Cusum()
        cusum.load_json(json_text)
        cusum._apply_config(config)  # CLI опции переопределяют JSON
    else:
        cusum = Cusum(config)

    if args.json:
        with _silent_stdout():
            results = cusum.run()
    else:
        results = cusum.run()

    if not results:
        sys.stderr.write("Нет результатов.\n")
        return 1

    best = min(results, key=lambda r: r.deviation)

    if args.json:
        import json as jsonlib
        print(jsonlib.dumps({
            "best": {
                "k": best.k,
                "H": best.H,
                "ARL": best.ARL,
                "deviation": best.deviation,
            },
            "count": len(results),
        }, indent=2))
    else:
        print("=" * 60)
        print("BEST PAIR (k, H):")
        print("=" * 60)
        print(f"  k  = {best.k:.4f}")
        print(f"  H  = {best.H:.4f}")
        print(f"  ARL = {best.ARL:.2f} (deviation: {best.deviation:.2f})")
        print("=" * 60)

        top = sorted(results, key=lambda r: r.deviation)[: args.top_n]
        print(f"\nTOP {len(top)} BEST PAIRS:")
        print(f"  {'#':>3} | {'k':>7} | {'H':>7} | {'ARL':>9} | {'Deviation':>9}")
        print("-" * 46)
        for i, r in enumerate(top, 1):
            print(f"  {i:>3} | {r.k:7.3f} | {r.H:7.3f} | {r.ARL:9.2f} | {r.deviation:9.2f}")

    if not args.keep_files:
        cwd = os.getcwd()
        for fname in ["arl_calculation_temp.csv", "arl_calculation.log",
                      "arl_checkpoint.txt", "arl_results_final.csv",
                      "best_arl_pairs.csv", "errors.log"]:
            path = os.path.join(cwd, fname)
            if os.path.exists(path):
                try:
                    os.remove(path)
                except OSError:
                    pass

    return 0


if __name__ == "__main__":
    sys.exit(main())