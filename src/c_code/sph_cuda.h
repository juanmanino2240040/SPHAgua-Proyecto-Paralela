#ifndef SPH_CUDA_H
#define SPH_CUDA_H

#include "sph_particle.h"

#include <vector>

// Backend CUDA del solver SPH.
//
// Reimplementa el mismo esquema que sph_simulator.cpp / sph_kernel.cpp pero
// manteniendo el estado en la GPU con arrays planos (SoA):
//
//   - rejilla uniforme de celdas construida con counting sort (atomicAdd + scan)
//   - busqueda de vecinos en las 27 celdas contiguas
//   - densidad/presion, gradiente de presion, viscosidad e integracion
//
// La interfaz publica NO expone tipos de CUDA: se puede compilar y enlazar
// contra `sph_cuda_stub.cpp` en maquinas sin GPU. En ese caso initialize()
// devuelve false y el nodo de Godot sigue usando el solver CPU.
namespace sph {

struct CudaStats {
	int active = 0;
	int total = 0;
	double avg_neighbors = 0.0;
	double avg_density = 0.0;
	double avg_speed = 0.0;
	double cfl = 0.0;
	int undersampled = 0;
	bool finite = true;
	double checksum = 0.0;
	unsigned long long hash = 0;
};

// Tiempo (ms) por etapa, medido con eventos CUDA. La GPU fusiona varias
// operaciones que la CPU mide por separado, asi que el reparto es:
//   other       -> puesta a cero de aceleraciones (incluye gravedad)
//   grid        -> celdas, conteo, prefijo y dispersion
//   query       -> busqueda de vecinos en las 27 celdas
//   kernel      -> W + densidad + ecuacion de estado (etapa fusionada)
//   pressure    -> gradiente dW + aceleracion por presion
//   viscosity   -> fuerzas viscosas
//   integrate   -> Euler-Cromer
//   bounds      -> ajuste al dominio
// kernel_deriv no existe como etapa aparte en la GPU (queda en 0).
struct CudaPhaseTimes {
	double grid = 0.0;
	double query = 0.0;
	double kernel = 0.0;
	double kernel_deriv = 0.0;
	double pressure = 0.0;
	double viscosity = 0.0;
	double integrate = 0.0;
	double bounds = 0.0;
	double other = 0.0;
};

class CudaSolver {
public:
	CudaSolver();
	~CudaSolver();

	CudaSolver(const CudaSolver &) = delete;
	CudaSolver &operator=(const CudaSolver &) = delete;

	// Comprueba que hay una GPU y que el binario tiene codigo para ella.
	// Lanza un kernel de prueba y sincroniza, por lo que tambien detecta el
	// tipico error "no kernel image is available for execution on the device".
	static bool runtime_available();

	// Copia las particulas a la GPU, las particiona (fluido primero), calcula
	// la densidad de reposo que falte y reserva la rejilla. Devuelve false si
	// no hay GPU, si el binario no soporta la arquitectura o si la
	// configuracion no es compatible (p. ej. remove_escaped_particles).
	bool initialize(const Config &p_conf, const std::vector<Particle> &p_particles);

	// Actualiza los parametros sin reenviar las particulas. Si cambia h,
	// kernel_support o el dominio se reconstruye solo la rejilla.
	void update_conf(const Config &p_conf);

	void simulate_step();

	// Igual que simulate_step(), pero ademas acumula el tiempo de cada etapa en
	// *p_times. Sincroniza al final del paso (los eventos lo necesitan), por lo
	// que solo conviene usarlo al perfilar, no en el bucle normal.
	void simulate_step(CudaPhaseTimes *p_times);

	// Sincroniza el stream por defecto (util para medir tiempos).
	void synchronize() const;

	// Copia las posiciones de las particulas activas al vector del host y
	// devuelve cuantas son. Se llama una vez por frame para pintar el MultiMesh.
	int copy_positions(std::vector<Vec3> &r_out) const;

	// Estadisticas para el benchmark y para la UI. Sincroniza implicitamente.
	CudaStats stats() const;
	double average_density() const;

	int active_particles() const;
	int total_particles() const;
	bool is_initialized() const;

	// Ultimo error (vacio si todo fue bien).
	const char *error() const;

	Config &get_conf();
	const Config &get_conf() const;

private:
	struct Impl;
	Impl *impl = nullptr;
};

} // namespace sph

#endif // SPH_CUDA_H
