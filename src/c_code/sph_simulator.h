#ifndef SPH_SIMULATOR_H
#define SPH_SIMULATOR_H

#include "sph_kernel.h"
#include "sph_particle.h"

#include <vector>

// Fluid simulation loop.
//
// Port of simulate::Simulator from https://github.com/frog711/sph-fluid-solver,
// promoted from 2D to 3D. The acceleration of every fluid particle is computed
// from the pressure term, the viscous term and the external (gravity) term of
// the Navier-Stokes equation, followed by an Euler-Cromer update step and a
// domain check.
namespace sph {

class Simulator {
public:
	Simulator() = default;

	// Takes ownership of the initial particle set, partitions it so all fluid
	// particles come first (required by the kernel) and derives rest density
	// when it was not set explicitly.
	void initialize(const Config &p_conf, std::vector<Particle> p_particles);
	void update_conf(const Config &p_conf);

	Config &get_conf() { return conf; }
	const Config &get_conf() const { return conf; }

	void set_kernel(Kernel *p_kernel) { kernel = p_kernel; }

	void simulate_step();

	double get_total_energy();
	double get_average_density();
	double get_cfl();
	int count_undersampled();
	double avg_speed();

	std::vector<Particle> &get_particles() { return particles; }
	const std::vector<Particle> &get_particles() const { return particles; }
	Particle *get_particle_data() { return particles.data(); }

private:
	Config conf;
	Kernel *kernel = nullptr;
	std::vector<Particle> particles;

	void compute_acceleration();
	void compute_pressure_forces();
	void compute_viscous_forces();
	void compute_external_forces();
	void compute_surface_tension();
	void perform_update_step();
	void check_bounding_boxes();
};

} // namespace sph

#endif // SPH_SIMULATOR_H
