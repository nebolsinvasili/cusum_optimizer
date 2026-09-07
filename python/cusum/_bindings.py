"""Raw ctypes bindings to libcusum.so."""
import ctypes
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