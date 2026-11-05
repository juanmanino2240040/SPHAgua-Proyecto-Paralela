#ifndef SPH_KERNEL_H
#define SPH_KERNEL_H

#include "sph_particle.h"

#include <vector>

// Neighbour search and kernel evaluation.
//
// Port of kernel::Kernel from https://github.com/frog711/sph-fluid-solver,
// promoted from 2D to 3D. Instead of the 9 cells scanned in 2D, a 3D uniform
// grid scans the 27 surrounding cells. The smoothing kernel is the same
// cubic spline (M4) used by the reference solver, only the normalisation
// factor differs for 3D.
namespace sph {

class Kernel {
public:
	Kernel() = default;
	explicit Kernel(const Config &p_conf);

	void update_conf(const Config &p_conf);
	void initialize(Particle *p_particles);

	void complete_neighbor_search();
	std::vector<int> specific_neighbor_search(const Vec3 &p_pos);

	void calculate_kernel();
	void calculate_kernel_derivative();

	// Discontinued helpers kept for parity with the reference solver.
	double get_kernel_entry(int p_i, int p_j);

private:
	Config conf;
	Particle *particles = nullptr;

	std::vector<int> grid;
	std::vector<int> grid_count;
	std::vector<int> cell_of; // celda de cada particula en el paso actual
	int grid_nx = 0;
	int grid_ny = 0;
	int grid_nz = 0;
	int grid_max = 0;
	double cell_size = 0.0;

	void quadratic_neighbor_search();
	void grid_neighborhood_search();
	// grid_neighborhood_search() = build_grid() + query_neighbors().
	// build_grid(): ordena todas las particulas por celda (conteo en 3 pasadas).
	// query_neighbors(): recorre las 27 celdas de cada particula de fluido.
	void build_grid();
	void query_neighbors();
	void cell_neighborhood_search(int p_i, int p_gx, int p_gy, int p_gz);

	// Returns the linear cell index and writes the per-axis cell coordinates.
	int cell_index(const Vec3 &p_pos, int &r_gx, int &r_gy, int &r_gz) const;

	double get_factor() const;
};

} // namespace sph

#endif // SPH_KERNEL_H
