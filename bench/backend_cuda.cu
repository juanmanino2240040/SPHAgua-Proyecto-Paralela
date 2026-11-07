// Backend "cuda": envuelve sph::CudaSolver (src/c_code/sph_cuda.cu).
//
// Solo se compila con `make CUDA=1`; en ese caso Makefile define SPH_HAS_CUDA y
// enlaza este objeto con sph_cuda.cu. Con CUDA=0 el backend no existe y
// make_backend("cuda") devuelve nullptr (igual que en la extension sin CUDA).
//
// Diferencias frente al backend CPU que conviene tener presentes al comparar:
//   - la fisica va en float32 (la CPU usa double),
//   - los vecinos se truncan a kMaxNeighbors (128),
//   - varias etapas estan fusionadas en un solo kernel (ver CudaPhaseTimes).
// Por eso el hash no tiene por que coincidir con la CPU; usa el checksum y las
// magnitudes fisicas (densidad, vecinos, cfl) para comparar.

#include "backend.h"

#include "sph_cuda.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace bench {

namespace {

class CudaBackend : public Backend {
	sph::CudaSolver solver;

public:
	const char *name() const override { return "cuda"; }

	int threads() const override { return 0; } // 0 = lo ejecuta la GPU

	void init(Scene scene) override {
		if (!solver.runtime_available()) {
			throw std::runtime_error("no hay GPU CUDA compatible con este binario");
		}
		if (!solver.initialize(scene.conf, scene.particles)) {
			const std::string message = solver.error();
			throw std::runtime_error(message.empty() ? "CudaSolver::initialize fallo" : message);
		}
	}

	void step(PhaseTimes *t) override {
		if (!t) {
			solver.simulate_step();
			return;
		}
		sph::CudaPhaseTimes ct;
		solver.simulate_step(&ct);
		t->grid += ct.grid;
		t->query += ct.query;
		t->kernel += ct.kernel;
		t->kernel_deriv += ct.kernel_deriv;
		t->pressure += ct.pressure;
		t->viscosity += ct.viscosity;
		t->integrate += ct.integrate;
		t->bounds += ct.bounds;
		t->other += ct.other;
	}

	// Misma ruta; solo se anade la sincronizacion para que --verify compare
	// despues de que la GPU haya terminado.
	void step_reference() override {
		solver.simulate_step();
		solver.synchronize();
	}

	void synchronize() override { solver.synchronize(); }

	Stats stats() override {
		solver.synchronize();
		const sph::CudaStats cs = solver.stats();
		Stats s;
		s.active = cs.active;
		s.boundary = cs.total - cs.active;
		s.avg_neighbors = cs.avg_neighbors;
		s.avg_density = cs.avg_density;
		s.cfl = cs.cfl;
		s.undersampled = cs.undersampled;
		s.finite = cs.finite;
		s.checksum = cs.checksum;
		s.hash = cs.hash;
		return s;
	}
};

} // namespace

std::unique_ptr<Backend> make_cuda_backend() {
	if (!sph::CudaSolver::runtime_available()) {
		return nullptr;
	}
	return std::make_unique<CudaBackend>();
}

} // namespace bench
