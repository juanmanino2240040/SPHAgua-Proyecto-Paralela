# Backend CUDA del solver SPH

El proyecto puede ejecutar la parte pesada de la simulación (búsqueda de
vecinos, kernels, densidad/presión, viscosidad e integración) en la GPU con
CUDA. El solver CPU de siempre se mantiene como *fallback* y sigue siendo el
predeterminado cuando no hay GPU compatible.

## Arquitectura

| Archivo | Contenido |
|---|---|
| `src/c_code/sph_cuda.h` | API pública de `sph::CudaSolver` (sin tipos de CUDA). |
| `src/c_code/sph_cuda.cu` | Implementación CUDA (SoA, rejilla uniforme, kernels). |
| `src/c_code/sph_cuda_stub.cpp` | Implementación vacía para compilar sin CUDA. |
| `src/c_code/sph_fluid_region.*` | Nodo Godot: propiedad `use_cuda`, copia de posiciones y *fallback*. |
| `bench/backend_cuda.cu` | Adaptador del benchmark (`--backend cuda`). |

Detalles de la implementación GPU:

- Estado en arrays planos `float` (SoA): posición, velocidad, aceleración,
  densidad, presión, masa y densidad de reposo.
- Rejilla uniforme de celdas de tamaño `h * kernel_support`, construida con
  *counting sort* (`atomicAdd` + prefijo exclusivo de Thrust).
- Búsqueda de vecinos en las 27 celdas contiguas; lista por partícula con tope
  fijo `kMaxNeighbors = 128`.
- Física en `float32` (la CPU usa `double`), por lo que los resultados son
  aproximados, no idénticos bit a bit.
- El nodo escribe el buffer `TRANSFORM_3D` del `MultiMesh` de golpe (12 floats
  por instancia) en vez de llamar a `set_instance_transform()` por partícula,
  lo que importa con miles de partículas.

## Compilar la extensión

```bash
# Detección automática: compila CUDA si encuentra nvcc y una arquitectura válida
scons

# Forzar solo CPU (enlaza sph_cuda_stub.cpp)
scons cuda=0

# Forzar CUDA eligiendo arquitectura
scons cuda=1 cuda_arch=sm_75
```

Variables útiles: `cuda_home=/ruta/a/cuda` (alias `cuda_path`) y
`nvcc=/ruta/a/nvcc`. Si no se indican, SCons deduce la raíz de CUDA a partir de
`nvcc` del `PATH`.

El runtime de CUDA se enlaza **estáticamente** (`cudart_static`), así que
`libsph_fluid_extension.so` no depende de `libcudart.so` en tiempo de ejecución.
En una máquina sin GPU/driver, la extensión carga igualmente y el nodo usa la
CPU.

### GPU Maxwell (GeForce 940MX de este equipo)

CUDA 13 **ya no genera código para `sm_50`** (Maxwell). Para ejecutar en esta
GPU hace falta CUDA 12.x (12.9 es la última que compila para Maxwell) y pedir la
arquitectura explícitamente:

```bash
scons cuda=1 cuda_arch=sm_50 cuda_home=/ruta/a/cuda-12.9
```

Sustituye `/ruta/a/cuda-12.9` por la raíz real del toolkit (en este equipo,
`/opt/cuda`). Si `nvcc` está en el `PATH`, basta con
`scons cuda=1 cuda_arch=sm_50`: SCons deduce `cuda_home` a partir de `nvcc`.
Si `cuda_home` no contiene `libcudart_static.a` (por ejemplo, una ruta que no
existe), ahora se usa automáticamente la raíz del `nvcc` encontrado.

Con CUDA 13 el binario se compila para `sm_75` y la GPU no puede ejecutarlo:
`runtime_available()` devuelve `false` y el nodo usa la CPU automáticamente.
No es un fallo silencioso; `get_backend_name()` devuelve `"cpu"` y el log
indica el motivo.

## Selección en tiempo de ejecución

La propiedad exportada `use_cuda` (por defecto `true`) intenta inicializar la
GPU en `_ready()`:

- Si `CudaSolver::initialize()` tiene éxito, `is_using_cuda()` es `true` y
  `get_backend_name()` devuelve `"cuda"`.
- Si no hay GPU, el binario no soporta su arquitectura o la configuración no es
  compatible, se imprime el error y se sigue con el solver CPU.

La propiedad `remove_escaped_particles` no está soportada por la GPU; si se
activa, el nodo cambia a CPU. Un fallo en caliente durante `_process()` también
provoca el cambio a CPU sin detener la simulación.

## Benchmark

```bash
cd bench
make CUDA=1                 # -> build/cuda/sph_bench
./build/cuda/sph_bench --backend cuda --particles 8000 --steps 200 --repeat 5
```

Si el backend no está disponible, el programa sale con código 2 y un mensaje
claro. Para comparar contra la CPU:

```bash
make OMP=1
./build/omp/sph_bench --particles 8000 --steps 200 --repeat 5 --threads 4
./build/cuda/sph_bench --backend cuda --particles 8000 --steps 200 --repeat 5
```

El `ms_step` del benchmark es tiempo de reloj de pared por paso y se mide
sincronizando la GPU al final del tramo (y tras el calentamiento), de modo que
no se cuenta trabajo pendiente. Los tiempos por fase salen de eventos CUDA y,
como los eventos deben leerse al final de cada paso, en modo `--backend cuda`
cada paso se sincroniza; sirven para ver el reparto, no para medir el *throughput*
máximo.

Etapas en GPU (la CPU las mide por separado):

| Columna | Etapa GPU |
|---|---|
| `ms_other` | reset de aceleraciones + gravedad |
| `ms_grid` | celdas, conteo, prefijo y dispersión |
| `ms_query` | búsqueda en 27 celdas |
| `ms_kernel` | W + densidad + ecuación de estado (fusionado) |
| `ms_pressure` | gradiente dW + aceleración por presión |
| `ms_viscosity` | fuerzas viscosas |
| `ms_integrate` | Euler-Cromer |
| `ms_bounds` | ajuste al dominio |

`ms_kernel_deriv` queda en 0 en GPU porque el gradiente va fusionado con la
etapa de presión.

## Limitaciones conocidas

- `float32` y tope de 128 vecinos: el `hash` no coincide con la CPU. Compara
  `checksum`, densidad media, vecinos medios y `cfl`.
- `remove_escaped_particles` solo existe en CPU.
- El paquete antiguo de `src/sph_cuda.cu`, `src/sph_cuda_kernels.cuh` y
  `src/sph_fluid_region.*` (raíz de `src/`, no `src/c_code/`) se conserva como
  referencia y **no** se compila. Tiene un error de *most vexing parse*
  (`std::vector<float> d(size_t(n));`) y usa `cuda_arch=native`, incompatible
  con CUDA moderno. No lo mezcles con `src/c_code/sph_cuda.*`.
- En esta máquina (940MX, `cc 5.0`) no se puede validar el rendimiento GPU con
  CUDA 13; hace falta instalar CUDA 12.x y compilar con `sm_50`. Si `nvcc` no
  está instalado, `scons` compila solo la CPU (la extensión sigue funcionando).
