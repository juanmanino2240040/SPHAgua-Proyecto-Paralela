#ifndef SPH_PARTICLE_H
#define SPH_PARTICLE_H

#include <cmath>
#include <vector>

// Core data structures of the SPH solver.
//
// This is a 3D port of the structures found in the reference project
// (https://github.com/frog711/sph-fluid-solver). The reference solver is 2D
// and stores positions/speeds/accelerations as std::vector<double>; here they
// are stored in a small double-precision Vec3 so the same math can be used in
// 3D while keeping the double precision the original solver relies on.
namespace sph {

struct Vec3 {
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;

	Vec3() = default;
	Vec3(double p_x, double p_y, double p_z) :
			x(p_x), y(p_y), z(p_z) {}

	Vec3 operator+(const Vec3 &p_o) const { return Vec3(x + p_o.x, y + p_o.y, z + p_o.z); }
	Vec3 operator-(const Vec3 &p_o) const { return Vec3(x - p_o.x, y - p_o.y, z - p_o.z); }
	Vec3 operator*(double p_s) const { return Vec3(x * p_s, y * p_s, z * p_s); }
	Vec3 operator/(double p_s) const { return Vec3(x / p_s, y / p_s, z / p_s); }
	Vec3 &operator+=(const Vec3 &p_o) {
		x += p_o.x;
		y += p_o.y;
		z += p_o.z;
		return *this;
	}
	Vec3 &operator-=(const Vec3 &p_o) {
		x -= p_o.x;
		y -= p_o.y;
		z -= p_o.z;
		return *this;
	}

	double dot(const Vec3 &p_o) const { return x * p_o.x + y * p_o.y + z * p_o.z; }
	double length_squared() const { return x * x + y * y + z * z; }
	double length() const { return std::sqrt(length_squared()); }
};

// Mirrors structures::particle from the reference solver.
struct Particle {
	bool is_stationary = false;
	double rest_density = 0.0;
	double density = 0.0;
	double pressure = 0.0;
	double mass = 1.0;
	Vec3 pos;
	Vec3 speed;
	Vec3 acc;
	std::vector<int> neighbors;
	std::vector<double> kernel;
	std::vector<double> kernel_deriv_x;
	std::vector<double> kernel_deriv_y;
	std::vector<double> kernel_deriv_z;
};

// Mirrors structures::config from the reference solver, extended to 3D.
struct Config {
	int particle_count = 0;
	int active_particles = 0;
	int dim = 3;
	double h = 0.1;
	double k = 5000.0;
	double nu = 0.0;
	double gamma = 0.0;
	double kernel_support = 2.0;
	double timestep = 0.0005;
	Vec3 gravity = Vec3(0.0, -9.81, 0.0);

	// Axis aligned simulation domain. The reference solver used area = {width, height}
	// with the domain starting at the origin; here we store the min/max corner so the
	// solver can work directly with the bounds read from the Godot scene.
	Vec3 area_min = Vec3(-0.5, 0.0, -0.5);
	Vec3 area_max = Vec3(0.5, 2.0, 0.5);

	// If true, fluid particles that leave the domain are removed (faithful to the
	// reference implementation). Otherwise they are clamped back inside the domain.
	bool remove_escaped = false;
};

} // namespace sph

#endif // SPH_PARTICLE_H
