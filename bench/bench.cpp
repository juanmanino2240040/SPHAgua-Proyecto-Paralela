// sph_bench: mide el tiempo del solver SPH desde la terminal, sin Godot.
//
//   ./sph_bench --particles 8000 --steps 200 --csv results/run.csv
//
// Ver README.md para todas las opciones y el significado de cada columna.

#include "backend.h"
#include "scene_builder.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {

struct Options {
	bench::SceneParams scene;
	int steps = 200;
	int warmup = 20;
	int repeat = 1;
	int threads = 0; // 0 => valor por defecto de OpenMP (OMP_NUM_THREADS)
	std::string csv;
	std::string backend = "cpu";
	bool verify = false;
	bool info = false;
	bool quiet = false;
};

void usage(const char *argv0) {
	std::printf(
			"Uso: %s [opciones]\n"
			"\n"
			"Escena:\n"
			"  --particles N        particulas objetivo (def. 1500; el fluido real queda algo por debajo)\n"
			"  --dt X|auto          paso de tiempo (def. auto: proporcional a h, ver README)\n"
			"  --fill F             fraccion de altura llena de fluido (def. 0.5)\n"
			"  --stiffness K        rigidez k (def. 5000)\n"
			"  --viscosity NU       viscosidad nu (def. 0.2)\n"
			"  --bounds x0 y0 z0 x1 y1 z1   dominio (def. el de particles_agua.tscn)\n"
			"  --no-boundary        sin particulas de frontera\n"
			"\n"
			"Medicion:\n"
			"  --steps S            pasos medidos (def. 200)\n"
			"  --warmup W           pasos previos NO medidos (def. 20)\n"
			"  --repeat R           repetir la corrida R veces desde cero (def. 1)\n"
			"  --threads T          hilos OpenMP (solo si se compilo con OMP=1)\n"
			"  --backend NAME       cpu (def.) o cuda (si se compilo con CUDA=1)\n"
			"  --csv FILE           anade una fila por corrida al CSV (crea la cabecera)\n"
			"  --quiet              no imprime el resumen legible\n"
			"\n"
			"Utilidades:\n"
			"  --verify             comprueba que la ruta cronometrada == Simulator::simulate_step()\n"
			"  --info               imprime backend, OpenMP y numero de hilos, y sale\n"
			"  --help               esta ayuda\n",
			argv0);
}

bool parse(int argc, char **argv, Options &o) {
	auto need = [&](int i, int n) {
		if (i + n >= argc) {
			std::fprintf(stderr, "Falta(n) %d valor(es) tras %s\n", n, argv[i]);
			return false;
		}
		return true;
	};
	for (int i = 1; i < argc; ++i) {
		const char *a = argv[i];
		if (!std::strcmp(a, "--help") || !std::strcmp(a, "-h")) {
			usage(argv[0]);
			std::exit(0);
		} else if (!std::strcmp(a, "--particles")) {
			if (!need(i, 1)) return false;
			o.scene.particle_count = std::atoi(argv[++i]);
		} else if (!std::strcmp(a, "--dt")) {
			if (!need(i, 1)) return false;
			const char *v = argv[++i];
			o.scene.timestep = std::strcmp(v, "auto") ? std::atof(v) : 0.0;
		} else if (!std::strcmp(a, "--fill")) {
			if (!need(i, 1)) return false;
			o.scene.fill_ratio = std::atof(argv[++i]);
		} else if (!std::strcmp(a, "--stiffness")) {
			if (!need(i, 1)) return false;
			o.scene.stiffness = std::atof(argv[++i]);
		} else if (!std::strcmp(a, "--viscosity")) {
			if (!need(i, 1)) return false;
			o.scene.viscosity = std::atof(argv[++i]);
		} else if (!std::strcmp(a, "--bounds")) {
			if (!need(i, 6)) return false;
			o.scene.bounds_min = sph::Vec3(std::atof(argv[i + 1]), std::atof(argv[i + 2]), std::atof(argv[i + 3]));
			o.scene.bounds_max = sph::Vec3(std::atof(argv[i + 4]), std::atof(argv[i + 5]), std::atof(argv[i + 6]));
			i += 6;
		} else if (!std::strcmp(a, "--no-boundary")) {
			o.scene.generate_boundary = false;
		} else if (!std::strcmp(a, "--steps")) {
			if (!need(i, 1)) return false;
			o.steps = std::atoi(argv[++i]);
		} else if (!std::strcmp(a, "--warmup")) {
			if (!need(i, 1)) return false;
			o.warmup = std::atoi(argv[++i]);
		} else if (!std::strcmp(a, "--repeat")) {
			if (!need(i, 1)) return false;
			o.repeat = std::atoi(argv[++i]);
		} else if (!std::strcmp(a, "--threads")) {
			if (!need(i, 1)) return false;
			o.threads = std::atoi(argv[++i]);
		} else if (!std::strcmp(a, "--backend")) {
			if (!need(i, 1)) return false;
			o.backend = argv[++i];
		} else if (!std::strcmp(a, "--csv")) {
			if (!need(i, 1)) return false;
			o.csv = argv[++i];
		} else if (!std::strcmp(a, "--quiet")) {
			o.quiet = true;
		} else if (!std::strcmp(a, "--verify")) {
			o.verify = true;
		} else if (!std::strcmp(a, "--info")) {
			o.info = true;
		} else {
			std::fprintf(stderr, "Opcion desconocida: %s (usa --help)\n", a);
			return false;
		}
	}
	if (o.steps < 1 || o.warmup < 0 || o.repeat < 1 || o.scene.particle_count < 1) {
		std::fprintf(stderr, "Valores invalidos: steps>=1, warmup>=0, repeat>=1, particles>=1\n");
		return false;
	}
	return true;
}

constexpr const char *kCsvHeader =
		"backend,omp,threads,particles_target,fluid,boundary,total,h,dt,steps,repeat,"
		"ms_step,ms_grid,ms_query,ms_kernel,ms_kernel_deriv,ms_pressure,ms_viscosity,ms_integrate,ms_bounds,ms_other,"
		"avg_neighbors,avg_density,cfl,finite,checksum,hash";

int compiled_with_openmp() {
#ifdef _OPENMP
	return 1;
#else
	return 0;
#endif
}

// Anade una fila al CSV. Crea la carpeta si no existe y se niega a mezclar filas
// con una cabecera distinta (p. ej. un CSV de una version anterior del bench).
bool append_csv(const std::string &path, const std::string &row) {
	namespace fs = std::filesystem;
	std::error_code ec;
	const fs::path parent = fs::path(path).parent_path();
	if (!parent.empty()) {
		fs::create_directories(parent, ec);
	}

	// Solo se revisa la cabecera de archivos normales (con una tuberia o /dev/stdout
	// leer bloquearia); en ese caso la fila se escribe tal cual.
	const bool regular = fs::is_regular_file(path, ec);
	const bool special = !regular && fs::exists(path, ec);

	bool has_content = special;
	if (regular) {
		std::ifstream in(path);
		std::string first;
		if (in && std::getline(in, first)) {
			has_content = true;
			if (!first.empty() && first.back() == '\r') {
				first.pop_back();
			}
			if (first != kCsvHeader) {
				std::fprintf(stderr,
						"ERROR: %s tiene otra cabecera (¿CSV de una version anterior?). No se anade la fila; borra o renombra ese archivo.\n",
						path.c_str());
				return false;
			}
		}
	}

	std::ofstream f(path, std::ios::app);
	if (!f) {
		std::fprintf(stderr, "No se pudo abrir %s para escribir\n", path.c_str());
		return false;
	}
	if (!has_content) {
		f << kCsvHeader << "\n";
	}
	f << row << "\n";
	return true;
}

int run_verify(const Options &o) {
	auto a = bench::make_backend(o.backend);
	auto b = bench::make_backend(o.backend);
	if (!a || !b) {
		std::fprintf(stderr, "Backend no disponible: %s (¿compilado sin CUDA=1 o GPU incompatible?)\n", o.backend.c_str());
		return 2;
	}
	a->init(bench::build_scene(o.scene));
	b->init(bench::build_scene(o.scene));
	const int n = std::max(1, std::min(o.steps, 60));
	for (int s = 0; s < n; ++s) {
		a->step_reference();
		b->step(nullptr);
	}
	const bench::Stats sa = a->stats();
	const bench::Stats sb = b->stats();
	const bool ok = sa.hash == sb.hash;
	std::printf("verify (%d pasos, %d particulas de fluido): ruta oficial hash=%016llx, ruta cronometrada hash=%016llx -> %s\n", n, sa.active,
			(unsigned long long)sa.hash, (unsigned long long)sb.hash, ok ? "OK" : "FALLO");
	return ok ? 0 : 1;
}

} // namespace

int main(int argc, char **argv) {
	Options o;
	if (!parse(argc, argv, o)) {
		return 2;
	}

#ifdef _OPENMP
	if (o.threads > 0) {
		omp_set_num_threads(o.threads);
	}
#else
	if (o.threads > 1) {
		std::fprintf(stderr, "AVISO: compilado sin OpenMP (make OMP=1); --threads %d se ignora.\n", o.threads);
	}
#endif

	if (o.info) {
		auto be = bench::make_backend(o.backend);
		if (!be) {
			std::fprintf(stderr, "Backend no disponible: %s%s\n", o.backend.c_str(),
					o.backend == "cuda" ? " (compilado sin CUDA=1 o GPU incompatible)" : "");
			return 2;
		}
		std::printf("backend %s\nomp %d\nmax_threads %d\n", o.backend.c_str(), compiled_with_openmp(), be->threads());
		return 0;
	}
	if (o.verify) {
		return run_verify(o);
	}

	using clk = std::chrono::steady_clock;
	int exit_code = 0;

	for (int rep = 1; rep <= o.repeat; ++rep) {
		auto be = bench::make_backend(o.backend);
		if (!be) {
			std::fprintf(stderr, "Backend no disponible: %s%s\n", o.backend.c_str(),
					o.backend == "cuda" ? " (compilado sin CUDA=1 o GPU incompatible)" : "");
			return 2;
		}
		try {
			bench::Scene scene = bench::build_scene(o.scene);
			const double h = scene.h;
			const double dt = scene.conf.timestep;
			be->init(std::move(scene));

			for (int s = 0; s < o.warmup; ++s) {
				be->step(nullptr);
			}
			// El calentamiento puede haber dejado trabajo asincrono en la GPU:
			// no debe entrar en la medida.
			be->synchronize();

			bench::PhaseTimes pt;
			const auto t0 = clk::now();
			for (int s = 0; s < o.steps; ++s) {
				be->step(&pt);
			}
			be->synchronize(); // espera a que la GPU termine antes de parar el reloj
			const auto t1 = clk::now();
			const double wall_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

			const bench::Stats st = be->stats();
			const double n = double(o.steps);
			const int total = st.active + st.boundary;
			const double phase_total = pt.total();

			if (st.active <= 0) {
				std::fprintf(stderr, "ERROR: el backend no tiene particulas activas (revisa la escena).\n");
				return 2;
			}

			if (!st.finite || st.avg_neighbors > 45.0) {
				std::fprintf(stderr,
						"AVISO: posible inestabilidad numerica (vecinos medios=%.1f, finito=%d). Los tiempos no son representativos; reduce --dt.\n",
						st.avg_neighbors, int(st.finite));
				exit_code = 3;
			}

			if (!o.quiet) {
				auto pr = [&](const char *label, double ms) {
					const double pct = phase_total > 0.0 ? 100.0 * ms / phase_total : 0.0;
					std::printf("  %-26s %10.4f ms/paso  %5.1f %%\n", label, ms / n, pct);
				};
				std::printf("[%s] repeticion %d/%d  hilos=%d  omp=%d\n", be->name(), rep, o.repeat, be->threads(), compiled_with_openmp());
				std::printf("  h=%.5f  dt=%.6g  fluido=%d  frontera=%d  total=%d  vecinos medios=%.1f\n", h, dt, st.active, st.boundary, total, st.avg_neighbors);
				pr("vecinos: rejilla (orden)", pt.grid);
				pr("vecinos: consulta 27 celdas", pt.query);
				pr("kernel W (+densidad/pres.)", pt.kernel);
				pr("gradiente dW", pt.kernel_deriv);
				pr("presion", pt.pressure);
				pr("viscosidad", pt.viscosity);
				pr("integracion", pt.integrate);
				pr("limites", pt.bounds);
				pr("otros", pt.other);
				std::printf("  TOTAL (reloj de pared)     %10.4f ms/paso   (%d pasos medidos, %.1f ms)\n", wall_ms / n, o.steps, wall_ms);
				std::printf("  densidad media=%.2f  cfl=%.4g  hash=%016llx\n", st.avg_density, st.cfl, (unsigned long long)st.hash);
			}

			if (!o.csv.empty()) {
				char row[1024];
				std::snprintf(row, sizeof(row),
						"%s,%d,%d,%d,%d,%d,%d,%.9g,%.9g,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.3f,%.6f,%.6g,%d,%.12g,%016llx",
						be->name(), compiled_with_openmp(), be->threads(), o.scene.particle_count, st.active, st.boundary, total, h, dt, o.steps, rep,
						wall_ms / n, pt.grid / n, pt.query / n, pt.kernel / n, pt.kernel_deriv / n, pt.pressure / n, pt.viscosity / n, pt.integrate / n,
						pt.bounds / n, pt.other / n, st.avg_neighbors, st.avg_density, st.cfl, int(st.finite), st.checksum, (unsigned long long)st.hash);
				if (!append_csv(o.csv, row)) {
					exit_code = 4;
				}
			}
		} catch (const std::exception &e) {
			std::fprintf(stderr, "ERROR en el backend %s: %s\n", o.backend.c_str(), e.what());
			return 2;
		}
	}
	return exit_code;
}
