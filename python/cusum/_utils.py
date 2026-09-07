"""Library path discovery for libcusum.so."""
import os
from pathlib import Path

def find_library() -> str:
    """Find libcusum.so relative to this package or in standard paths."""
    pkg_dir = Path(__file__).resolve().parent
    project_root = pkg_dir.parent.parent
    lib_path = project_root / "lib" / "libcusum.so"
    if lib_path.exists():
        return str(lib_path)

    env_path = os.environ.get("CUSUM_LIB_PATH")
    if env_path and Path(env_path).exists():
        return env_path

    for search in ["/usr/local/lib", "/usr/lib", Path.home() / "lib"]:
        p = Path(search) / "libcusum.so"
        if p.exists():
            return str(p)

    raise FileNotFoundError(
        "Cannot find libcusum.so. Build with 'make lib' or set CUSUM_LIB_PATH."
    )