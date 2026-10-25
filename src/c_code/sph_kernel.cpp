#include "sph_kernel.h"

#include <algorithm>
#include <cmath>

namespace sph {

// Empirical correction constant used by the reference solver. It slightly
// reduces the kernel mass to compensate for the truncation error of the
// compact support. Kept identical for behavioural parity.
static const double correction_const = 0.999139;

Kernel::Kernel(const Config &p_conf) {
	conf = p_conf;
}

void Kernel::update_conf(const Config &p_conf) {
	conf = p_conf;
}

void Kernel::initialize(Particle *p_particles) {
	particles = p_particles;

	cell_size = conf.h * conf.kernel_support;
	if (cell_size <= 0.0) {
		cell_size = conf.h;
	}

	const double span_x = conf.area_max.x - conf.area_min.x;
	const double span_y = conf.area_max.y - conf.area_min.y;
	const double span_z = conf.area_max.z - conf.area_min.z;

	grid_nx = std::max(1, int(span_x / cell_size) + 1);
	grid_ny = std::max(1, int(span_y / cell_size) + 1);
	grid_nz = std::max(1, int(span_z / cell_size) + 1);
	grid_max = grid_nx * grid_ny * grid_nz;

	grid.resize(std::max(1, conf.particle_count));
	grid_count.resize(grid_max + 1, 0);
}

int Kernel::cell_index(const Vec3 &p_pos, int &r_gx, int &r_gy, int &r_gz) const {
	int gx = int((p_pos.x - conf.area_min.x) / cell_size);
	int gy = int((p_pos.y - conf.area_min.y) / cell_size);
	int gz = int((p_pos.z - conf.area_min.z) / cell_size);

	// Clamp particles that are (marginally) outside the domain into the border
	// cells so the neighbour search never indexes out of bounds.
	gx = std::min(std::max(gx, 0), grid_nx - 1);
	gy = std::min(std::max(gy, 0), grid_ny - 1);
	gz = std::min(std::max(gz, 0), grid_nz - 1);

	r_gx = gx;
	r_gy = gy;
	r_gz = gz;
	return (gz * grid_ny + gy) * grid_nx + gx;
}

void Kernel::complete_neighbor_search() {
	grid_neighborhood_search();
}

void Kernel::cell_neighborhood_search(int p_i, int p_gx, int p_gy, int p_gz) {
	if (p_gx < 0 || p_gy < 0 || p_gz < 0 || p_gx >= grid_nx || p_gy >= grid_ny || p_gz >= grid_nz) {
		return;
	}

	const int grid_id = (p_gz * grid_ny + p_gy) * grid_nx + p_gx;
	const double support = conf.kernel_support * conf.h;
	const double support_sq = support * support;

	// grid_count[grid_id] is the first slot of the cell and grid_count[grid_id + 1]
	// is the first slot of the following cell (see grid_neighborhood_search()).
	for (int k = grid_count[grid_id]; k < grid_count[grid_id + 1]; k++) {
		const int j = grid[k];
		const Vec3 d = particles[p_i].pos - particles[j].pos;
		if (d.length_squared() < support_sq) {
			particles[p_i].neighbors.push_back(j);
		}
	}
}

void Kernel::grid_neighborhood_search() {
	// Counting sort of all particles into the uniform grid. Even inactive
	// (stationary / boundary) particles are inserted so they can be found as
	// neighbours of the active fluid particles.
	for (int i = 0; i <= grid_max; i++) {
		grid_count[i] = 0;
	}

	for (int i = 0; i < conf.particle_count; i++) {
		particles[i].neighbors.clear();
		int gx = 0, gy = 0, gz = 0;
		const int grid_id = cell_index(particles[i].pos, gx, gy, gz);
		grid_count[grid_id]++;
	}

	for (int i = 1; i <= grid_max; i++) {
		grid_count[i] = grid_count[i] + grid_count[i - 1];
	}

	for (int i = 0; i < conf.particle_count; i++) {
		int gx = 0, gy = 0, gz = 0;
		const int grid_id = cell_index(particles[i].pos, gx, gy, gz);
		grid[--grid_count[grid_id]] = i;
	}

	// After the pass above grid_count[c] holds the first slot of cell c and
	// grid_count[c + 1] the end of cell c.
	for (int i = 0; i < conf.active_particles; i++) {
		int gx = 0, gy = 0, gz = 0;
		cell_index(particles[i].pos, gx, gy, gz);
		for (int dz = -1; dz <= 1; dz++) {
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					cell_neighborhood_search(i, gx + dx, gy + dy, gz + dz);
				}
			}
		}
	}
}

void Kernel::quadratic_neighbor_search() {
	for (int i = 0; i < conf.active_particles; i++) {
		particles[i].neighbors.clear();
	}
	const double support = conf.kernel_support * conf.h;
	const double support_sq = support * support;
	for (int i = 0; i < conf.active_particles; i++) {
		particles[i].neighbors.push_back(i);
		for (int j = i + 1; j < conf.particle_count; j++) {
			const Vec3 d = particles[i].pos - particles[j].pos;
			if (d.length_squared() < support_sq) {
				particles[i].neighbors.push_back(j);
				particles[j].neighbors.push_back(i);
			}
		}
	}
}

std::vector<int> Kernel::specific_neighbor_search(const Vec3 &p_pos) {
	std::vector<int> neighbors;
	const double support_sq = std::pow(conf.kernel_support * conf.h, 2);
	for (int i = 0; i < conf.particle_count; i++) {
		if ((particles[i].pos - p_pos).length_squared() < support_sq) {
			neighbors.push_back(i);
		}
	}
	return neighbors;
}

void Kernel::calculate_kernel() {
	const double alpha = get_factor();
	for (int i = 0; i < conf.active_particles; i++) {
		particles[i].kernel.resize(particles[i].neighbors.size());
		for (size_t k = 0; k < particles[i].neighbors.size(); k++) {
			const int j = particles[i].neighbors[k];
			const double dx = (particles[i].pos.x - particles[j].pos.x) / conf.h;
			const double dy = (particles[i].pos.y - particles[j].pos.y) / conf.h;
			const double dz = (particles[i].pos.z - particles[j].pos.z) / conf.h;
			const double q = std::sqrt(dx * dx + dy * dy + dz * dz);
			const double q1 = std::max(0.0, 1 - q);
			const double q2 = std::max(0.0, 2 - q);
			const double kernel_val = correction_const * alpha * (q2 * q2 * q2 - 4 * q1 * q1 * q1);
			particles[i].kernel[k] = kernel_val;
		}
	}
}

void Kernel::calculate_kernel_derivative() {
	const double alpha = get_factor();
	for (int i = 0; i < conf.active_particles; i++) {
		const size_t n = particles[i].neighbors.size();
		particles[i].kernel_deriv_x.resize(n);
		particles[i].kernel_deriv_y.resize(n);
		particles[i].kernel_deriv_z.resize(n);
		for (size_t k = 0; k < n; k++) {
			const int j = particles[i].neighbors[k];
			const double dx = (particles[i].pos.x - particles[j].pos.x) / conf.h;
			const double dy = (particles[i].pos.y - particles[j].pos.y) / conf.h;
			const double dz = (particles[i].pos.z - particles[j].pos.z) / conf.h;
			const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
			const double q1 = std::max(0.0, 1 - dist);
			const double q2 = std::max(0.0, 2 - dist);
			const double inner = -3 * q2 * q2 + 12 * q1 * q1;
			double common = 0.0;
			// Ignore elements with a very low distance to avoid underflow.
			if (dist > 0.001) {
				common = alpha * inner / (dist * conf.h * conf.h);
			}
			particles[i].kernel_deriv_x[k] = correction_const * (particles[i].pos.x - particles[j].pos.x) * common;
			particles[i].kernel_deriv_y[k] = correction_const * (particles[i].pos.y - particles[j].pos.y) * common;
			particles[i].kernel_deriv_z[k] = correction_const * (particles[i].pos.z - particles[j].pos.z) * common;
		}
	}
}

double Kernel::get_factor() const {
	// Cubic spline (M4) normalisation for the compact form
	//   W(q) = alpha * (max(0, 2 - q)^3 - 4 * max(0, 1 - q)^3),  q = r / h
	// The reference solver uses the 2D factor 5 / (14 * pi * h^2); the 3D
	// factor is 1 / (4 * pi * h^3). 1D is included for completeness.
	if (conf.dim == 1) {
		return 1.0 / (6.0 * conf.h);
	} else if (conf.dim == 2) {
		return 5.0 / (14.0 * M_PI * std::pow(conf.h, 2));
	} else if (conf.dim == 3) {
		return 1.0 / (4.0 * M_PI * std::pow(conf.h, 3));
	}
	return 1.0 / (4.0 * M_PI * std::pow(conf.h, 3));
}

double Kernel::get_kernel_entry(int p_i, int p_j) {
	for (size_t k = 0; k < particles[p_i].neighbors.size(); k++) {
		if (particles[p_i].neighbors[k] == p_j) {
			return particles[p_i].kernel[k];
		}
	}
	return 0.0;
}

} // namespace sph
