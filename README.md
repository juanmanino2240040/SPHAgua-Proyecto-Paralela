# SPH Agua 3D — Simulación de fluidos paralela en Godot

Simulador de agua en 3D basado en **SPH** (*Smoothed Particle Hydrodynamics*),
integrado en **Godot 4** mediante una extensión en C++ (GDExtension). El solver
corre en CPU con **OpenMP** y cuenta con un backend **CUDA** opcional para GPU.

El proyecto es un port a 3D del solver de referencia
[frog711/sph-fluid-solver](https://github.com/frog711/sph-fluid-solver)
(originalmente 2D), con generación de volumen de fluido, fronteras, benchmark y
paralelización propios.

---

## Características

- Solver SPH 3D lagrangiano y sin malla.
- Kernel cúbico M4 con soporte compacto de radio `2h`.
- Búsqueda de vecinos con rejilla uniforme y 27 celdas (evita el coste O(N²)).
- Generación automática de la malla de agua y de dos capas de frontera en las
  seis caras del dominio.
- Visualización eficiente con `MultiMesh` (miles de partículas por frame).
- Paralelismo de datos con OpenMP: **resultado idéntico bit a bit** al secuencial.
- Backend CUDA opcional (`float32`, hasta 128 vecinos por partícula).
- Benchmark de terminal independiente de Godot, con CSV, speedup, eficiencia y
  verificación por hash.

---

## Cómo funciona (resumen)

Cada partícula guarda masa, posición, velocidad, densidad y presión. En cada
paso de tiempo:

1. Se construye una rejilla uniforme de celdas de lado `2h` y se buscan las
   vecinas en las 27 celdas contiguas.
2. Se calcula la densidad con el kernel: `ρᵢ = Σⱼ mⱼ Wᵢⱼ`.
3. Se calcula la presión con una ecuación de estado simple:
   `pᵢ = max(k(ρᵢ − ρ₀), 0)`.
4. Se acumulan las fuerzas de presión, viscosidad y gravedad.
5. Se integra con **Euler–Cromer**: primero la velocidad y luego la posición.

El espaciado inicial se deriva del volumen de fluido y del número de partículas
(`s = ∛(V/N)`), de modo que `h = s` y `m = ρ₀s³`. Así la densidad de reposo se
cumple por construcción.

---

## Requisitos

- [Godot 4.7](https://godotengine.org/) (el proyecto usa la API 4.7).
- Compilador C++17 (`g++` o `clang++`).
- [SCons](https://scons.org/) y Python 3.
- `godot-cpp` (incluido en el repositorio).
- Opcional: CUDA 12.x o 13.x con `nvcc` para el backend GPU.

---

## Compilar la extensión

Desde la raíz del repositorio:

```bash
# Compila solo CPU (recomendado si no hay GPU/CUDA)
scons cuda=0

# Detección automática: compila CUDA si encuentra nvcc y una arquitectura válida
scons

# Forzar CUDA indicando arquitectura
scons cuda=1 cuda_arch=sm_75
```

El resultado es `bin/libsph_fluid_extension.so`, que Godot carga según
`sph_fluid_extension.gdextension`.

> **OpenMP en Godot:** por defecto `SConstruct` no añade `-fopenmp`, así que la
> extensión se compila en modo secuencial. Para activar OpenMP dentro de Godot,
> añade `-fopenmp` a `CXXFLAGS` y `LINKFLAGS` en `SConstruct`. El benchmark sí
> se compila con OpenMP mediante `make OMP=1`.

---

## Ejecutar la simulación

1. Compila la extensión (ver arriba).
2. Abre `project.godot` con Godot 4.7.
3. Ejecuta la escena principal `src/scenes/particles_agua.tscn`.

La escena principal usa `particle_count = 4000` y `use_cuda = false`.

### Propiedades del nodo `SPHFluidRegion3D`

| Propiedad | Descripción |
|---|---|
| `particle_count` | Número objetivo de partículas de fluido. |
| `fill_ratio` | Fracción de la altura de la caja que ocupa el agua. |
| `stiffness` | Constante `k` de la ecuación de presión. |
| `viscosity` | Coeficiente de viscosidad `ν`. |
| `rest_density` | Densidad de reposo `ρ₀` (por defecto 1000). |
| `gravity` | Vector de gravedad. |
| `generate_boundary` | Genera las partículas estáticas de frontera. |
| `use_cuda` | Intenta usar GPU; si no está disponible, cae a CPU. |
| `remove_escaped_particles` | Elimina partículas fuera del dominio (solo CPU). |

---

## Benchmark

El benchmark mide el solver **sin Godot**, usando los mismos fuentes de
`src/c_code`. Sirve para comparar versión secuencial, OpenMP y CUDA de forma
reproducible.

```bash
cd bench

make                 # secuencial  -> build/serial/sph_bench
make OMP=1           # OpenMP      -> build/omp/sph_bench
make CUDA=1          # CUDA        -> build/cuda/sph_bench

make run             # corrida de ejemplo
make verify          # comprueba la ruta cronometrada contra Simulator::simulate_step()
make sweep           # barrido de tamaños/hilos a results/*.csv
```

Ejemplos:

```bash
./build/serial/sph_bench --particles 8000 --steps 200
./build/omp/sph_bench --particles 8000 --steps 200 --threads 8 --repeat 5
./build/cuda/sph_bench --backend cuda --particles 8000 --steps 200 --repeat 5
```

Opciones principales: `--particles`, `--steps`, `--warmup`, `--repeat`,
`--threads`, `--backend cpu|cuda`, `--csv`, `--verify` y `--dt auto`.

El benchmark registra tiempo por paso, tiempos por fase, vecinos medios,
densidad media, CFL, comprobación de NaN y un `hash` FNV-1a de posiciones y
velocidades.

### Resultados de referencia

Mediana de `ms_step` para 16 000 partículas (los valores dependen del equipo):

| Configuración | Tiempo/paso | Speedup | Eficiencia |
|---|---:|---:|---:|
| Secuencial | 73.1 ms | 1.00× | — |
| OpenMP, 8 hilos | 14.3 ms | 5.11× | 64 % |
| OpenMP, 16 hilos | 10.8 ms | 6.76× | 42 % |

El speedup no es lineal por el trabajo secuencial de la rejilla, las barreras
de sincronización y el ancho de banda de memoria. La versión OpenMP produce el
mismo `hash` que la secuencial.

Documentación detallada: [`bench/README.md`](bench/README.md).

---

## Backend CUDA

El backend GPU es **opcional**. Ejecuta en la GPU la búsqueda de vecinos,
kernels, densidad/presión, viscosidad e integración; si no hay GPU compatible,
el nodo usa CPU automáticamente.

```bash
scons cuda=0                              # solo CPU
scons cuda=1 cuda_arch=sm_75              # forzar CUDA
scons cuda=1 cuda_arch=sm_50 cuda_home=/ruta/a/cuda-12.9   # GPU Maxwell
```

Diferencias frente a CPU:

- Física en `float32` (la CPU usa `double`).
- Lista de vecinos con tope fijo de 128.
- Uso de `atomicAdd` para acumular fuerzas.
- Por todo ello, el `hash` **no** coincide con la CPU; compara `checksum`,
  densidad media, vecinos medios y `cfl`.

En una GPU Maxwell (por ejemplo, GeForce 940MX, `cc 5.0`) hace falta CUDA 12.x:
CUDA 13 ya no genera código para `sm_50`. Si el backend no está disponible, la
extensión carga igualmente y la simulación continúa en CPU.

Documentación detallada: [`CUDA.md`](CUDA.md).

---

## Estructura del proyecto

```
.
├── SConstruct                     # Compilación de la extensión (CPU/CUDA)
├── project.godot                  # Proyecto Godot 4.7
├── sph_fluid_extension.gdextension
├── src/
│   ├── c_code/
│   │   ├── sph_particle.h         # Estructuras y macro SPH_PARALLEL_FOR
│   │   ├── sph_kernel.*           # Rejilla, vecinos, kernel y gradiente
│   │   ├── sph_simulator.*        # Densidad, presión, viscosidad, integración
│   │   ├── sph_fluid_region.*     # Nodo Godot, MultiMesh, selección CPU/CUDA
│   │   ├── sph_cuda.*             # Backend CUDA y stub sin CUDA
│   │   └── register_types.*       # Registro del nodo en Godot
│   ├── scenes/                    # Escenas de ejemplo
│   └── scripts/                   # Cámara, FPS y otros scripts GDScript
├── bench/                         # Benchmark de terminal (sin Godot)
├── results/                       # CSV y gráficas de resultados
├── bin/                           # Extensión compilada
└── CUDA.md                        # Documentación del backend GPU
```

---

## Limitaciones

- El modelo de presión es simple; no se usa tensión superficial en el solver final.
- Las fronteras son partículas estáticas, no paredes con fricción compleja.
- El benchmark mide el solver, no el render de Godot.
- CUDA está implementado, pero su validación depende del hardware disponible.
- `remove_escaped_particles` solo existe en el backend CPU.

---

## Créditos

- Solver SPH de referencia: [frog711/sph-fluid-solver](https://github.com/frog711/sph-fluid-solver).
- Motor y extensión: [Godot Engine](https://godotengine.org/) y
  [godot-cpp](https://github.com/godotengine/godot-cpp).
