#include "sph_simulator.h"

#include <algorithm>
#include <cmath>

namespace sph {

void Simulator::initialize(const Config &p_conf, std::vector<Particle> p_particles) {
	conf = p_conf;
	particles = std::move(p_particles);

	// The reference solver rearranges its particles so that every active
	// (fluid) particle comes before the stationary (boundary) particles. A
	// stable partition achieves the same invariant with much simpler code.
	auto first_stationary = std::partition(particles.begin(), particles.end(),
			[](const Particle &p) { return !p.is_stationary; });

	conf.active_particles = int(first_stationary - particles.begin());
	conf.particle_count = int(particles.size());

	// restDensity = mass / h^dim, matching the reference solver.
	const double cell_volume = std::pow(conf.h, double(conf.dim));
	for (Particle &p : particles) {
		if (p.rest_density <= 0.0 && cell_volume > 0.0) {
			p.rest_density = p.mass / cell_volume;
		}
	}
}

void Simulator::update_conf(const Config &p_conf) {
	conf = p_conf;
	if (kernel) {
		kernel->update_conf(conf);
	}
}

void Simulator::simulate_step() {
	if (!kernel || conf.active_particles <= 0) {
		return;
	}
	compute_acceleration();
	perform_update_step();
	check_bounding_boxes();
}

void Simulator::compute_acceleration() {
	for (int i = 0; i < conf.active_particles; i++) {
		particles[i].acc = Vec3(0.0, 0.0, 0.0);
	}

	kernel->complete_neighbor_search();
	kernel->calculate_kernel();
	kernel->calculate_kernel_derivative();

	compute_external_forces();
	compute_pressure_forces();
	compute_viscous_forces();
}

void Simulator::compute_external_forces() {
	for (int i = 0; i < conf.active_particles; i++) {
		particles[i].acc += conf.gravity;
	}
}

void Simulator::compute_pressure_forces() {
	// First pass: density and pressure for every fluid particle.
	for (int i = 0; i < conf.active_particles; i++) {
		particles[i].density = 0.0;
		for (size_t k = 0; k < particles[i].neighbors.size(); k++) {
			const int j = particles[i].neighbors[k];
			particles[i].density += particles[j].mass * particles[i].kernel[k];
		}
		particles[i].pressure = std::max(conf.k * (particles[i].density - particles[i].rest_density), 0.0);
	}

	// Second pass: pressure acceleration.
	for (int i = 0; i < conf.active_particles; i++) {
		for (size_t k = 0; k < particles[i].neighbors.size(); k++) {
			const int j = particles[i].neighbors[k];
			const double rho_i = particles[i].density;

			double coef = 0.0;
			if (particles[j].is_stationary) {
				// Mirror only the pressure of the fluid particle onto the
				// boundary particle (the comment in the reference solver).
				coef = 2.0 * particles[j].mass * (particles[i].pressure / (rho_i * rho_i));
			} else {
				const double rho_j = particles[j].density;
				coef = particles[j].mass * (particles[i].pressure / (rho_i * rho_i) +
												   particles[j].pressure / (rho_j * rho_j));
			}

			particles[i].acc.x -= coef * particles[i].kernel_deriv_x[k];
			particles[i].acc.y -= coef * particles[i].kernel_deriv_y[k];
			particles[i].acc.z -= coef * particles[i].kernel_deriv_z[k];
		}
	}
}

void Simulator::compute_viscous_forces() {
	for (int i = 0; i < conf.active_particles; i++) {
		for (size_t k = 0; k < particles[i].neighbors.size(); k++) {
			const int j = particles[i].neighbors[k];

			const double dWX = particles[i].kernel_deriv_x[k];
			const double dWY = particles[i].kernel_deriv_y[k];
			const double dWZ = particles[i].kernel_deriv_z[k];

			double coef = 2.0 * conf.nu * particles[j].mass / particles[j].density;
			if (particles[j].is_stationary) {
				coef = 2.0 * conf.nu * particles[j].mass / particles[i].density;
			}

			const Vec3 vij = particles[i].speed - particles[j].speed;
			const Vec3 xij = particles[i].pos - particles[j].pos;
			const double denom = xij.length_squared() + 0.01 * conf.h * conf.h;

			particles[i].acc.x += coef * (vij.x * xij.x) / denom * dWX;
			particles[i].acc.y += coef * (vij.y * xij.y) / denom * dWY;
			particles[i].acc.z += coef * (vij.z * xij.z) / denom * dWZ;
		}
	}
}

// Surface tension is not used by the final solver but is kept for parity with
// the reference implementation / future use.
void Simulator::compute_surface_tension() {
	for (int i = 0; i < conf.active_particles; i++) {
		Vec3 surface_tension(0.0, 0.0, 0.0);
		for (size_t k = 0; k < particles[i].neighbors.size(); k++) {
			const int j = particles[i].neighbors[k];
			surface_tension += (particles[i].pos - particles[j].pos) * particles[i].kernel[k];
		}
		particles[i].acc -= surface_tension * conf.gamma;
	}
}

void Simulator::perform_update_step() {
	// Euler-Cromer: update the speed with the current acceleration first, then
	// advance the position with the new speed.
	for (int i = 0; i < conf.active_particles; i++) {
		particles[i].speed += particles[i].acc * conf.timestep;
		particles[i].pos += particles[i].speed * conf.timestep;
	}
}

void Simulator::check_bounding_boxes() {
	if (!conf.remove_escaped) {
		// Robust default for a closed Godot container: keep the particles inside
		// the domain by clamping the position and killing the outgoing velocity.
		for (int i = 0; i < conf.active_particles; i++) {
			Vec3 &p = particles[i].pos;
			Vec3 &v = particles[i].speed;

			if (p.x < conf.area_min.x) {
				p.x = conf.area_min.x;
				if (v.x < 0.0) v.x = 0.0;
			} else if (p.x > conf.area_max.x) {
				p.x = conf.area_max.x;
				if (v.x > 0.0) v.x = 0.0;
			}

			if (p.y < conf.area_min.y) {
				p.y = conf.area_min.y;
				if (v.y < 0.0) v.y = 0.0;
			} else if (p.y > conf.area_max.y) {
				p.y = conf.area_max.y;
				if (v.y > 0.0) v.y = 0.0;
			}

			if (p.z < conf.area_min.z) {
				p.z = conf.area_min.z;
				if (v.z < 0.0) v.z = 0.0;
			} else if (p.z > conf.area_max.z) {
				p.z = conf.area_max.z;
				if (v.z > 0.0) v.z = 0.0;
			}
		}
		return;
	}

	// Faithful to the reference solver: remove particles that leave the domain.
	for (int i = 0; i < conf.active_particles; i++) {
		const Vec3 &p = particles[i].pos;
		if (p.x > conf.area_max.x || p.y > conf.area_max.y || p.z > conf.area_max.z ||
				p.x < conf.area_min.x || p.y < conf.area_min.y || p.z < conf.area_min.z) {
			std::swap(particles[i], particles[conf.active_particles - 1]);
			particles.erase(particles.begin() + (conf.active_particles - 1));
			conf.active_particles--;
			conf.particle_count = int(particles.size());
			if (kernel) {
				kernel->update_conf(conf);
			}
			i--;
		}
	}
}

double Simulator::get_total_energy() {
	double energy = 0.0;
	for (int i = 0; i < conf.active_particles; i++) {
		energy += particles[i].pressure * conf.h * conf.h / conf.k;
		energy += 0.5 * particles[i].mass * particles[i].speed.length_squared();
		energy += (conf.area_max.y - conf.area_min.y) - particles[i].mass * conf.gravity.dot(particles[i].pos);
	}
	return energy;
}

double Simulator::get_average_density() {
	double density_sum = 0.0;
	int valid_particles = 0;
	for (int i = 0; i < conf.active_particles; i++) {
		if (particles[i].density >= particles[i].rest_density) {
			density_sum += particles[i].density;
			valid_particles++;
		}
	}
	if (valid_particles == 0) {
		return 0.0;
	}
	return density_sum / valid_particles;
}

double Simulator::get_cfl() {
	double max_speed = 0.0;
	for (int i = 0; i < conf.active_particles; i++) {
		const double speed = particles[i].speed.length();
		if (speed > max_speed) {
			max_speed = speed;
		}
	}
	return max_speed / conf.h * conf.timestep;
}

int Simulator::count_undersampled() {
	int count = 0;
	for (int i = 0; i < conf.active_particles; i++) {
		if (particles[i].density < particles[i].rest_density) {
			count++;
		}
	}
	return count;
}

double Simulator::avg_speed() {
	if (conf.active_particles == 0) {
		return 0.0;
	}
	double speed_sum = 0.0;
	for (int i = 0; i < conf.active_particles; i++) {
		speed_sum += particles[i].speed.length();
	}
	return speed_sum / conf.active_particles;
}

} // namespace sph
