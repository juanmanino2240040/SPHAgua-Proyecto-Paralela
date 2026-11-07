import os
import shutil
import subprocess

# Version de la API de Godot que usa el proyecto (project.godot -> config/features).
# Se puede sobreescribir con: scons api_version=4.7
api_version = ARGUMENTS.get("api_version", "4.7")

# Argumentos propios de este proyecto. Se leen y se quitan de ARGUMENTS antes de
# cargar godot-cpp/SConstruct: si no, godot-cpp avisa de "variables desconocidas"
# y parece que cuda=0/1 se ignora (aunque este SConstruct si lo usa).
cuda_arg = ARGUMENTS.pop("cuda", "auto").lower()
cuda_home_arg = ARGUMENTS.pop("cuda_home", None)
cuda_path_arg = ARGUMENTS.pop("cuda_path", None)
nvcc_arg = ARGUMENTS.pop("nvcc", None)
cuda_arch_arg = ARGUMENTS.pop("cuda_arch", None)

base_env = SConscript("godot-cpp/SConstruct", exports="api_version")
# Clonar antes de anadir flags propios (CUDA, rutas): asi cambiar cuda=0/1 no
# invalida la firma de compilacion de godot-cpp y no se reconstruye entero.
env = base_env.Clone()

env.Append(CPPPATH=["src/c_code/", "godot-cpp/include", "godot-cpp/gen/include"])

# ---------------------------------------------------------------------------
# CUDA (opcional)
#
#   scons                    -> detecta nvcc y compila el backend GPU si puede
#   scons cuda=0             -> solo CPU (enlaza sph_cuda_stub.cpp)
#   scons cuda=1             -> exige nvcc; falla si no esta
#   scons cuda=1 cuda_arch=sm_86
#
# El backend se elige en tiempo de ejecucion con la propiedad use_cuda del nodo
# SPHFluidRegion3D. Si no hay GPU compatible se usa el solver CPU.
# ---------------------------------------------------------------------------
cuda_enabled = cuda_arg not in ("0", "false", "no", "off")
if cuda_arg == "auto":
    cuda_enabled = bool(shutil.which("nvcc")) or os.path.exists(
        os.path.join(os.environ.get("CUDA_HOME", "/usr/local/cuda"), "bin", "nvcc")
    )

cuda_home = cuda_home_arg or cuda_path_arg or os.environ.get("CUDA_HOME", "/usr/local/cuda")
nvcc = nvcc_arg or os.path.join(cuda_home, "bin", "nvcc")
if not os.path.exists(nvcc):
    nvcc = shutil.which("nvcc") or nvcc

# Raiz de CUDA segun el nvcc que realmente se va a usar. Es la fuente fiable
# cuando cuda_home no existe o apunta a otra instalacion (p. ej. se pasa
# cuda_home=/opt/cuda-12.9 pero el toolkit esta en /opt/cuda).
nvcc_root = (
    os.path.dirname(os.path.dirname(os.path.realpath(nvcc)))
    if os.path.exists(nvcc)
    else None
)

# Si no se indico cuda_home/cuda_path y el valor por defecto no existe (p. ej.
# CUDA instalado en /opt/cuda), deducir la raiz a partir del nvcc encontrado.
if (
    not cuda_home_arg
    and not cuda_path_arg
    and "CUDA_HOME" not in os.environ
    and not os.path.isdir(cuda_home)
    and nvcc_root
):
    cuda_home = nvcc_root


def find_cuda_libdir(root):
    """Directorio con libcudart_static.a (o libcudart.so como respaldo)."""
    candidates = (
        os.path.join(root, "targets", "x86_64-linux", "lib"),
        os.path.join(root, "lib64"),
        os.path.join(root, "lib"),
    )
    for names in (("libcudart_static.a",), ("libcudart.so",)):
        for candidate in candidates:
            if any(os.path.exists(os.path.join(candidate, name)) for name in names):
                return candidate
    return None

if cuda_enabled and not os.path.exists(nvcc):
    if cuda_arg in ("1", "true", "yes", "on"):
        print("ERROR: cuda=1 pero no se encontro nvcc (usa nvcc=... o cuda_home=...).")
        Exit(1)
    print("CUDA: nvcc no encontrado; se compila solo el backend CPU.")
    cuda_enabled = False

sources = [
    "src/c_code/sph_kernel.cpp",
    "src/c_code/sph_simulator.cpp",
    "src/c_code/sph_fluid_region.cpp",
    "src/c_code/register_types.cpp",
]

cuda_archive = None

if cuda_enabled:
    # Arquitectura: la GPU local si nvcc la soporta; si no, la minima soportada.
    # CUDA 13 ya no compila Maxwell/Pascal/Volta (sm_50..sm_70). Si tu GPU es una
    # Maxwell (p. ej. GeForce 940MX) necesitas CUDA 12.x (12.9 es la ultima que
    # soporta sm_50) y cuda_arch=sm_50.
    cuda_arch = cuda_arch_arg
    if not cuda_arch:
        try:
            out = subprocess.check_output(
                ["nvidia-smi", "--query-gpu=compute_cap", "--format=csv,noheader"],
                text=True,
                stderr=subprocess.DEVNULL,
            )
            first = next(line for line in out.splitlines() if line.strip())
            cuda_arch = "sm_" + first.strip().replace(".", "")
        except Exception:
            cuda_arch = None

    try:
        supported = subprocess.check_output(
            [nvcc, "--list-gpu-arch"], text=True, stderr=subprocess.DEVNULL
        ).split()
    except Exception:
        supported = []

    # `nvcc --list-gpu-arch` devuelve compute_XX, mientras que -arch= usa sm_XX.
    # Normalizamos para no descartar una arquitectura valida (p. ej. sm_50).
    def arch_key(arch):
        return arch.strip().lower().replace("sm_", "compute_")

    supported_keys = {arch_key(a) for a in supported}

    if not cuda_arch or (supported_keys and arch_key(cuda_arch) not in supported_keys):
        fallback = next(
            (a for a in ("sm_75", "sm_80", "sm_86", "sm_89", "sm_90", "sm_120")
             if not supported_keys or arch_key(a) in supported_keys),
            "sm_75",
        )
        if cuda_arch:
            print("CUDA: %s no esta soportada por este nvcc; usando %s." % (cuda_arch, fallback))
        cuda_arch = fallback

    # Buscamos libcudart_static.a, que es la que se enlaza. Si cuda_home no la
    # tiene (ruta inexistente o de otra version), probamos la raiz deducida de
    # nvcc antes de rendirnos.
    cuda_libdir = find_cuda_libdir(cuda_home)
    if cuda_libdir is None and nvcc_root and nvcc_root != cuda_home:
        cuda_libdir = find_cuda_libdir(nvcc_root)
        if cuda_libdir is not None:
            print(
                "CUDA: %s no contiene libcudart_static.a; usando %s (segun nvcc)."
                % (cuda_home, nvcc_root)
            )
            cuda_home = nvcc_root
    if cuda_libdir is None:
        msg = (
            "no se encontro libcudart_static.a en %s (usa cuda_home=... o nvcc=...)"
            % cuda_home
        )
        if cuda_arg in ("1", "true", "yes", "on"):
            print("ERROR: " + msg)
            Exit(1)
        print("CUDA: " + msg + "; se compila solo el backend CPU.")
        cuda_enabled = False

if cuda_enabled:
    print("CUDA: nvcc=%s arch=%s lib=%s" % (nvcc, cuda_arch, cuda_libdir))

    env.Append(CPPDEFINES=["SPH_HAS_CUDA"])
    env.Append(LIBPATH=["bin", cuda_libdir])
    # cudart estatica: la extension no depende de libcudart.so en tiempo de
    # ejecucion. Asi el .so carga igual en maquinas sin toolkit CUDA y el
    # fallback a CPU funciona aunque no exista la libreria de runtime.
    env.Append(LIBS=["sph_cuda_core", "cudart_static", "rt", "pthread", "dl"])

    # SCons solo acepta objetos de SharedObject dentro de un SharedLibrary y no
    # tiene builder para .cu. Por eso el nucleo CUDA se empaqueta en un archivo
    # estatico y se enlaza con -lsph_cuda_core.
    cuda_obj_tmp = "src/c_code/sph_cuda_cuda.o"
    cuda_archive = env.Command(
        target="bin/libsph_cuda_core.a",
        source="src/c_code/sph_cuda.cu",
        action=[
            "%s -O3 -std=c++17 -arch=%s -Xcompiler -fPIC -DSPH_HAS_CUDA -Isrc/c_code -c $SOURCE -o %s"
            % (nvcc, cuda_arch, cuda_obj_tmp),
            "ar rcs $TARGET %s" % cuda_obj_tmp,
        ],
    )
else:
    sources.append("src/c_code/sph_cuda_stub.cpp")

library = env.SharedLibrary("bin/sph_fluid_extension", sources)

if cuda_archive is not None:
    env.Depends(library, cuda_archive)

Default(library)
