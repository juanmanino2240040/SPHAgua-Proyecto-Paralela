#ifndef BENCH_BACKEND_H
#define BENCH_BACKEND_H

#include "scene_builder.h"

#include <cstdint>
#include <memory>
#include <string>

// Interfaz minima que el benchmark necesita de un "motor" SPH.
//
// Hoy existe un unico backend, "cpu" (el solver de src/c_code, secuencial o con
// OpenMP segun como se compile). La interfaz esta pensada para que un futuro
// backend "cuda" se enchufe sin tocar bench.cpp: recibe la misma Scene, avanza
// pasos y devuelve las mismas estadisticas para comparar resultados.
namespace bench {

// Tiempo acumulado (ms) por fase del paso de simulacion.
struct PhaseTimes {
	double grid = 0.0; // construccion de la rejilla (conteo/ordenamiento por celdas)
	double query = 0.0; // consulta de vecinos en las 27 celdas
	double kernel = 0.0; // W por par
	double kernel_deriv = 0.0; // gradiente de W por par
	double pressure = 0.0; // gravedad + densidad + aceleracion por presion
	double viscosity = 0.0;
	double integrate = 0.0; // Euler-Cromer
	double bounds = 0.0; // ajuste al dominio
	double other = 0.0; // puesta a cero de aceleraciones

	double total() const { return grid + query + kernel + kernel_deriv + pressure + viscosity + integrate + bounds + other; }
};

// Estado resumido de la simulacion. Se calcula FUERA de la zona medida.
struct Stats {
	int active = 0; // particulas de fluido
	int boundary = 0; // particulas de frontera
	double avg_neighbors = 0.0; // vecinos medios por particula de fluido
	double avg_density = 0.0;
	double cfl = 0.0;
	int undersampled = 0;
	bool finite = true; // false si aparece NaN/inf
	double checksum = 0.0; // suma gruesa de posiciones y velocidades (comparar con tolerancia)
	uint64_t hash = 0; // hash FNV-1a de los bits de pos/vel (comparar bit a bit)
};

class Backend {
public:
	virtual ~Backend() = default;

	virtual const char *name() const = 0;
	virtual int threads() const = 0;

	virtual void init(Scene scene) = 0;

	// Un paso de simulacion; si t != nullptr acumula el tiempo de cada fase.
	virtual void step(PhaseTimes *t) = 0;

	// Un paso por la ruta oficial del solver (Simulator::simulate_step), sin
	// cronometros. Sirve para --verify: ambas rutas deben dar el mismo hash.
	virtual void step_reference() = 0;

	// Espera a que el backend termine el trabajo pendiente (GPU). La CPU no
	// necesita hacer nada.
	virtual void synchronize() {}

	virtual Stats stats() = 0;
};

// name: "cpu" o "cuda". Devuelve nullptr si el backend no existe en esta
// compilacion o si no hay una GPU utilizable.
std::unique_ptr<Backend> make_backend(const std::string &name);

#if defined(SPH_HAS_CUDA)
std::unique_ptr<Backend> make_cuda_backend();
#endif

} // namespace bench

#endif // BENCH_BACKEND_H
