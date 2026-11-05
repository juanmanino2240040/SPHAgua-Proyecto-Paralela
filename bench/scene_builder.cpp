#include "scene_builder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_set>

namespace bench {

double resolve_spacing(const SceneParams &p) {
	if (p.smoothing_radius > 0.0) {
		return p.smoothing_radius;
	}
	const sph::Vec3 size = p.bounds_max - p.bounds_min;
	const double fluid_height = size.y * std::clamp(p.fill_ratio, 0.05, 1.0);
	const double volume = size.x * size.z * fluid_height;
	const int target = std::max(1, p.particle_count);
	double spacing = std::cbrt(volume / double(target));
	if (!(spacing > 0.0)) {
		spacing = 0.05;
	}
	return spacing;
}

double auto_timestep(double h) {
	SceneParams reference; // escena original: 1500 particulas
	reference.particle_count = 1500;
	const double h_ref = resolve_spacing(reference);
	return std::min(kReferenceTimestep, kReferenceTimestep * h / h_ref);
}

Scene build_scene(const SceneParams &p) {
	const sph::Vec3 bmin = p.bounds_min;
	const sph::Vec3 bmax = p.bounds_max;
	const sph::Vec3 size = bmax - bmin;

	const double spacing = resolve_spacing(p);
	const double cell_volume = spacing * spacing * spacing;
	double mass = p.rest_density * cell_volume;
	if (!(mass > 0.0)) {
		mass = 1.0;
	}

	std::vector<sph::Particle> parts;

	// ---- Fluido: malla regular en la parte inferior del dominio.
	const double fluid_height = size.y * std::clamp(p.fill_ratio, 0.05, 0.95);
	int nx = std::max(1, int(size.x / spacing) - 1);
	int nz = std::max(1, int(size.z / spacing) - 1);
	int ny = std::max(1, int(fluid_height / spacing) - 1);

	const double x0 = bmin.x + 0.5 * (size.x - (nx - 1) * spacing);
	const double z0 = bmin.z + 0.5 * (size.z - (nz - 1) * spacing);
	const double y0 = bmin.y + 2.0 * spacing;

	while (ny > 1 && y0 + (ny - 1) * spacing > bmax.y - 2.0 * spacing) {
		ny--;
	}

	parts.reserve(size_t(nx) * size_t(ny) * size_t(nz));
	for (int ix = 0; ix < nx; ++ix) {
		for (int iy = 0; iy < ny; ++iy) {
			for (int iz = 0; iz < nz; ++iz) {
				sph::Particle q;
				q.is_stationary = false;
				q.mass = mass;
				q.rest_density = p.rest_density;
				q.pos = sph::Vec3(x0 + ix * spacing, y0 + iy * spacing, z0 + iz * spacing);
				parts.push_back(q);
			}
		}
	}

	// ---- Frontera: 2 capas estacionarias en las 6 caras, sin duplicados en aristas.
	if (p.generate_boundary) {
		std::unordered_set<uint64_t> seen;
		auto encode = [](int a, int b, int c) -> uint64_t {
			return (uint64_t(uint32_t(a)) << 42) ^ (uint64_t(uint32_t(b)) << 21) ^ uint64_t(uint32_t(c));
		};
		auto add_boundary = [&](double px, double py, double pz) {
			const int a = int(std::lround((px - bmin.x) / spacing));
			const int b = int(std::lround((py - bmin.y) / spacing));
			const int c = int(std::lround((pz - bmin.z) / spacing));
			if (!seen.insert(encode(a, b, c)).second) {
				return;
			}
			sph::Particle q;
			q.is_stationary = true;
			q.mass = mass;
			q.rest_density = p.rest_density;
			q.pos = sph::Vec3(px, py, pz);
			parts.push_back(q);
		};

		const int layers = 2;
		const double eps = spacing * 1e-4;

		for (int l = 0; l < layers; ++l) { // suelo y techo
			const double yf = bmin.y + (l + 0.5) * spacing;
			const double yc = bmax.y - (l + 0.5) * spacing;
			for (double x = bmin.x + 0.5 * spacing; x <= bmax.x - 0.5 * spacing + eps; x += spacing) {
				for (double z = bmin.z + 0.5 * spacing; z <= bmax.z - 0.5 * spacing + eps; z += spacing) {
					add_boundary(x, yf, z);
					add_boundary(x, yc, z);
				}
			}
		}
		for (int l = 0; l < layers; ++l) { // paredes x
			const double xw0 = bmin.x + (l + 0.5) * spacing;
			const double xw1 = bmax.x - (l + 0.5) * spacing;
			for (double y = bmin.y + 0.5 * spacing; y <= bmax.y - 0.5 * spacing + eps; y += spacing) {
				for (double z = bmin.z + 0.5 * spacing; z <= bmax.z - 0.5 * spacing + eps; z += spacing) {
					add_boundary(xw0, y, z);
					add_boundary(xw1, y, z);
				}
			}
		}
		for (int l = 0; l < layers; ++l) { // paredes z
			const double zw0 = bmin.z + (l + 0.5) * spacing;
			const double zw1 = bmax.z - (l + 0.5) * spacing;
			for (double y = bmin.y + 0.5 * spacing; y <= bmax.y - 0.5 * spacing + eps; y += spacing) {
				for (double x = bmin.x + 0.5 * spacing; x <= bmax.x - 0.5 * spacing + eps; x += spacing) {
					add_boundary(x, y, zw0);
					add_boundary(x, y, zw1);
				}
			}
		}
	}

	Scene scene;
	scene.h = spacing;
	scene.conf.particle_count = int(parts.size());
	scene.conf.active_particles = 0; // lo calcula Simulator::initialize()
	scene.conf.dim = 3;
	scene.conf.h = spacing;
	scene.conf.k = p.stiffness;
	scene.conf.nu = p.viscosity;
	scene.conf.kernel_support = p.kernel_support;
	scene.conf.timestep = p.timestep > 0.0 ? p.timestep : auto_timestep(spacing);
	scene.conf.gravity = p.gravity;
	scene.conf.area_min = bmin;
	scene.conf.area_max = bmax;
	scene.conf.remove_escaped = p.remove_escaped;
	scene.particles = std::move(parts);
	return scene;
}

} // namespace bench
