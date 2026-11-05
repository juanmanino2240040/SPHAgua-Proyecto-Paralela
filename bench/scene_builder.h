#ifndef BENCH_SCENE_BUILDER_H
#define BENCH_SCENE_BUILDER_H

#include "sph_particle.h"

#include <vector>

// Construccion de la escena SIN Godot.
//
// Replica SPHFluidRegion3D::build_particles() (src/c_code/sph_fluid_region.cpp)
// para que el benchmark mida exactamente la misma configuracion que Godot
// simula. Si cambias la generacion de particulas en el nodo, cambiala tambien
// aqui (o, mejor, extrae esta logica a un archivo comun sin dependencias de Godot).
namespace bench {

// Referencia de estabilidad: dt de la escena original (1500 particulas).
constexpr double kReferenceTimestep = 0.0005;

struct SceneParams {
	int particle_count = 1500;

	// AABB del contenedor de src/scenes/particles_agua.tscn (union de sus 5 BoxShape3D).
	sph::Vec3 bounds_min = sph::Vec3(-0.263169698, 0.749503508, -0.274677882);
	sph::Vec3 bounds_max = sph::Vec3(0.267469698, 1.249503509, 0.278800677);

	// Mismos valores por defecto que las propiedades del nodo.
	double smoothing_radius = 0.0; // 0 => derivado de particle_count
	double stiffness = 5000.0;
	double viscosity = 0.2;
	double kernel_support = 2.0;
	double rest_density = 1000.0;
	double fill_ratio = 0.5;
	bool generate_boundary = true;
	bool remove_escaped = false;
	sph::Vec3 gravity = sph::Vec3(0.0, -9.81, 0.0);

	// 0 => dt automatico (ver auto_timestep()).
	double timestep = 0.0;
};

struct Scene {
	sph::Config conf;
	std::vector<sph::Particle> particles;
	double h = 0.0;
};

// Espaciado entre particulas (= h) que el nodo deriva de particle_count.
double resolve_spacing(const SceneParams &p);

// dt proporcional a h (condicion tipo CFL), anclado a la escena original:
// dt = 0.0005 con 1500 particulas y menor al aumentar la resolucion. Un dt fijo
// de 0.0005 diverge con h pequenas (p. ej. 30000 particulas).
double auto_timestep(double h);

Scene build_scene(const SceneParams &p);

} // namespace bench

#endif // BENCH_SCENE_BUILDER_H
