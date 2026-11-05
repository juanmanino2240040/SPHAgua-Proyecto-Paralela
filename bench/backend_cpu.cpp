// Backend "cpu": envuelve el solver de src/c_code (sph::Kernel + sph::Simulator).
//
// Es el unico fichero del benchmark que toca los internos del solver. Para
// cronometrar cada fase por separado necesita llamar a metodos privados de
// Simulator (compute_pressure_forces, ...). Con `#define private public` se
// consigue sin modificar el solver. Es un truco SOLO de este fichero; cuando se
// refactorice el solver, lo limpio es exponer un Simulator::step(PhaseTimes*) y
// borrar el truco.

#include "backend.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

#define private public
#include "sph_kernel.h"
#include "sph_simulator.h"
#undef private

namespace bench {

namespace {

using clk = std::chrono::steady_clock;

double elapsed_ms(clk::time_point a, clk::time_point b) {
	return std::chrono::duration<double, std::milli>(b - a).count();
}

void fnv_mix(uint64_t &h, double v) {
	uint64_t bits;
	std::memcpy(&bits, &v, sizeof(bits));
	for (int i = 0; i < 8; ++i) {
		h ^= (bits >> (8 * i)) & 0xffu;
		h *= 1099511628211ull;
	}
}

class CpuBackend : public Backend {
	sph::Simulator sim;
	sph::Kernel kernel;

public:
	const char *name() const override { return "cpu"; }

	int threads() const override {
#ifdef _OPENMP
		return omp_get_max_threads();
#else
		return 1;
#endif
	}

	void init(Scene scene) override {
		sim.initialize(scene.conf, std::move(scene.particles));
		kernel = sph::Kernel(sim.get_conf());
		kernel.initialize(sim.get_particle_data());
		sim.set_kernel(&kernel);
	}

	// Misma secuencia que Simulator::simulate_step() + compute_acceleration(),
	// con un cronometro entre fases. --verify comprueba que no se desvie.
	void step(PhaseTimes *t) override {
		if (sim.conf.active_particles <= 0) {
			return;
		}
		auto t0 = clk::now();
		auto lap = [&](double PhaseTimes::*field) {
			const auto t1 = clk::now();
			if (t) {
				t->*field += elapsed_ms(t0, t1);
			}
			t0 = t1;
		};

		for (int i = 0; i < sim.conf.active_particles; i++) {
			sim.particles[i].acc = sph::Vec3(0.0, 0.0, 0.0);
		}
		lap(&PhaseTimes::other);

		kernel.build_grid();
		lap(&PhaseTimes::grid);

		kernel.query_neighbors();
		lap(&PhaseTimes::query);

		kernel.calculate_kernel();
		lap(&PhaseTimes::kernel);

		kernel.calculate_kernel_derivative();
		lap(&PhaseTimes::kernel_deriv);

		sim.compute_external_forces();
		sim.compute_pressure_forces();
		lap(&PhaseTimes::pressure);

		sim.compute_viscous_forces();
		lap(&PhaseTimes::viscosity);

		sim.perform_update_step();
		lap(&PhaseTimes::integrate);

		sim.check_bounding_boxes();
		lap(&PhaseTimes::bounds);
	}

	void step_reference() override { sim.simulate_step(); }

	Stats stats() override {
		Stats s;
		const int active = sim.conf.active_particles;
		s.active = active;
		s.boundary = sim.conf.particle_count - active;

		double neighbors = 0.0;
		uint64_t hash = 1469598103934665603ull;
		double checksum = 0.0;
		for (int i = 0; i < active; i++) {
			const sph::Particle &p = sim.particles[i];
			neighbors += double(p.neighbors.size());
			const double vals[6] = { p.pos.x, p.pos.y, p.pos.z, p.speed.x, p.speed.y, p.speed.z };
			for (double v : vals) {
				if (!std::isfinite(v)) {
					s.finite = false;
				}
				fnv_mix(hash, v);
			}
			checksum += p.pos.x + p.pos.y + p.pos.z + p.speed.length();
		}
		s.avg_neighbors = active > 0 ? neighbors / active : 0.0;
		s.hash = hash;
		s.checksum = checksum;
		s.avg_density = sim.get_average_density();
		s.cfl = sim.get_cfl();
		s.undersampled = sim.count_undersampled();
		return s;
	}
};

} // namespace

std::unique_ptr<Backend> make_backend(const std::string &name) {
	if (name == "cpu") {
		return std::make_unique<CpuBackend>();
	}
	return nullptr;
}

} // namespace bench
