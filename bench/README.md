# bench/ – benchmark SPH desde la terminal (sin Godot)

Mide el tiempo del solver de `src/c_code` (`sph_kernel`, `sph_simulator`) sin
Godot ni `godot-cpp`. Godot queda solo para **ver** la simulación; todos los
tiempos se toman aquí.

## Compilar y ejecutar

```bash
cd bench
make                 # secuencial  -> build/serial/sph_bench   (línea base)
make OMP=1           # con OpenMP  -> build/omp/sph_bench
make run             # corrida de ejemplo (escena original, 1500 partículas)
make verify          # comprobación de coherencia (ver abajo)
```

Variables útiles: `CXX=clang++`, `OPT="-O3 -march=native"`, `SRC_DIR=../src/c_code`.
Los dos binarios usan **los mismos fuentes** del solver; solo cambia `-fopenmp`.

## Uso

```bash
./build/serial/sph_bench --particles 8000 --steps 200
./build/omp/sph_bench    --particles 8000 --steps 200 --threads 4 --repeat 5 --csv results/run.csv
./build/serial/sph_bench --info        # backend, OpenMP sí/no, hilos
```

| Opción | Significado |
|---|---|
| `--particles N` | partículas objetivo (def. 1500; el fluido real queda algo por debajo) |
| `--dt X\|auto` | paso de tiempo (def. `auto`, ver más abajo) |
| `--steps S` / `--warmup W` | pasos medidos / pasos previos no medidos (def. 200 / 20) |
| `--repeat R` | repite la corrida R veces desde cero |
| `--threads T` | hilos OpenMP (solo con `make OMP=1`) |
| `--csv FILE` | añade una fila por corrida (crea la cabecera si hace falta) |
| `--fill`, `--stiffness`, `--viscosity`, `--bounds`, `--no-boundary` | parámetros de la escena |
| `--verify` | la ruta cronometrada debe dar el mismo hash que `Simulator::simulate_step()` |
| `--quiet` | sin resumen legible (útil en scripts) |

Código de salida `3` si detecta inestabilidad numérica (vecinos medios > 45 o NaN):
esos tiempos no son representativos.

## Barrido y resumen

```bash
OUT=results/todo.csv BIN=build/serial/sph_bench ./run_sweep.sh
OUT=results/todo.csv BIN=build/omp/sph_bench    ./run_sweep.sh   # hilos 1,2,4..nproc
python3 summarize.py results/todo.csv --phases
```

`run_sweep.sh` acepta `SIZES`, `THREADS`, `REPEAT`, `WARMUP`, `STEPS`, `EXTRA`.
`summarize.py` toma la mediana de las repeticiones, usa la corrida secuencial como
base y calcula **speedup**, **eficiencia** (= speedup / hilos), µs por partícula y
una verificación contra la base (hash idéntico o diferencia relativa del checksum).

## Columnas del CSV

`backend, omp, threads, particles_target, fluid, boundary, total, h, dt, steps, repeat,`
`ms_step` (reloj de pared por paso), `ms_grid, ms_query, ms_kernel, ms_kernel_deriv, ms_pressure,
ms_viscosity, ms_integrate, ms_bounds, ms_other` (por fase), `avg_neighbors, avg_density, cfl,
finite, checksum, hash`.

- `hash`: FNV-1a de los bits de posición y velocidad. Dos ejecuciones con el mismo
  hash dan **exactamente** el mismo resultado.
- `checksum`: suma gruesa; sirve para comparar con tolerancia si una versión
  paralela cambia el orden de las sumas y el hash difiere por redondeo.

## Paso de tiempo automático

Con `dt = 0.0005` fijo la simulación **diverge** cuando h es pequeña (con 30 000
partículas los vecinos medios saltan a ~115 en vez de ~30). Por eso `--dt auto`
usa `dt = 0.0005 · h / h_ref` (tope 0.0005), anclado a la escena original
(1500 partículas, h ≈ 0.0366). Con ello:

| N objetivo | h | dt | vecinos medios | densidad media |
|---|---|---|---|---|
| 1 500 | 0.0366 | 5.0e-4 | 19 | 1000 |
| 8 000 | 0.0209 | 2.9e-4 | 24 | 1000 |
| 16 000 | 0.0166 | 2.3e-4 | 30 | 1000 |
| 30 000 | 0.0135 | 1.8e-4 | 26 | 1007 |

Para comparar versiones **usa siempre el mismo `dt`** (el automático es determinista).

## OpenMP en el solver

Los bucles por partícula de `sph_kernel.cpp` y `sph_simulator.cpp` llevan la macro
`SPH_PARALLEL_FOR` (definida en `sph_particle.h`). Con `-fopenmp` es
`#pragma omp parallel for schedule(static)`; sin él queda vacía y el código es
exactamente el secuencial, sin avisos.

Fases paralelizadas: consulta de vecinos (27 celdas), kernel W, gradiente dW,
presión (densidad y aceleración), viscosidad, gravedad, integración y puesta a
cero de aceleraciones. Siguen secuenciales: el conteo/ordenamiento de la rejilla
(~1-2 % del tiempo) y `check_bounding_boxes` (<0.3 %).

**El resultado es idéntico bit a bit al secuencial**, para cualquier número de
hilos: cada iteración escribe solo en su propia partícula y recorre los vecinos en
el mismo orden. Compruébalo siempre con la columna `verificacion` de `summarize.py`
(debe decir `= base (bit a bit)`).

Para usarlo en Godot hay que compilar la extensión con `-fopenmp` (en `SConstruct`:
`env.Append(CXXFLAGS=["-fopenmp"])` y `env.Append(LINKFLAGS=["-fopenmp"])`). Sin eso,
Godot sigue ejecutando la versión secuencial. El benchmark no depende de ello.

## CUDA (siguiente)

`backend.h` define la interfaz (`init`, `step`, `stats`). Un backend `cuda` se
añadiría como otro fichero (`backend_cuda.cu`) y se elegiría con `--backend cuda`,
recibiendo la misma `Scene` y devolviendo las mismas estadísticas. Para que sea
viable conviene antes pasar los `std::vector` por partícula a arreglos planos (SoA)
y listas de vecinos compactas (CSR).

## Limitaciones conocidas

- `scene_builder.cpp` **duplica** la generación de partículas de
  `SPHFluidRegion3D::build_particles()`. Si cambias una, cambia la otra (o extrae
  la lógica a un archivo común sin dependencias de Godot).
- `backend_cpu.cpp` usa `#define private public` para cronometrar fases privadas
  de `Simulator`. Es un truco acotado a ese fichero; `--verify` detecta si la
  secuencia de fases se desvía de `simulate_step()`.
- Los tiempos dependen de los flags: Godot compila con SCons y puede usar otros
  flags que `make`. Compara siempre dentro del mismo método de compilación.
- Para medir bien: máquina sin otra carga, `OMP_PROC_BIND=close OMP_PLACES=cores`
  con OpenMP, y varias repeticiones (`--repeat 5`).
