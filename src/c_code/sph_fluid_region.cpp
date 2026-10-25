#include "sph_fluid_region.h"

#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/shape3d.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_set>
#include <vector>

using namespace godot;

SPHFluidRegion3D::SPHFluidRegion3D() {}

SPHFluidRegion3D::~SPHFluidRegion3D() {}

void SPHFluidRegion3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_particle_count", "p_count"), &SPHFluidRegion3D::set_particle_count);
	ClassDB::bind_method(D_METHOD("get_particle_count"), &SPHFluidRegion3D::get_particle_count);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "particle_count", PROPERTY_HINT_RANGE, "1,200000,1"), "set_particle_count", "get_particle_count");

	ClassDB::bind_method(D_METHOD("set_container_path", "p_path"), &SPHFluidRegion3D::set_container_path);
	ClassDB::bind_method(D_METHOD("get_container_path"), &SPHFluidRegion3D::get_container_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "container_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node3D"), "set_container_path", "get_container_path");

	ClassDB::bind_method(D_METHOD("set_smoothing_radius", "p_value"), &SPHFluidRegion3D::set_smoothing_radius);
	ClassDB::bind_method(D_METHOD("get_smoothing_radius"), &SPHFluidRegion3D::get_smoothing_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "smoothing_radius", PROPERTY_HINT_RANGE, "0,1,0.001,or_greater"), "set_smoothing_radius", "get_smoothing_radius");

	ClassDB::bind_method(D_METHOD("set_stiffness", "p_value"), &SPHFluidRegion3D::set_stiffness);
	ClassDB::bind_method(D_METHOD("get_stiffness"), &SPHFluidRegion3D::get_stiffness);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "stiffness", PROPERTY_HINT_RANGE, "1,1000000,1,or_greater"), "set_stiffness", "get_stiffness");

	ClassDB::bind_method(D_METHOD("set_viscosity", "p_value"), &SPHFluidRegion3D::set_viscosity);
	ClassDB::bind_method(D_METHOD("get_viscosity"), &SPHFluidRegion3D::get_viscosity);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "viscosity", PROPERTY_HINT_RANGE, "0,10,0.01,or_greater"), "set_viscosity", "get_viscosity");

	ClassDB::bind_method(D_METHOD("set_kernel_support", "p_value"), &SPHFluidRegion3D::set_kernel_support);
	ClassDB::bind_method(D_METHOD("get_kernel_support"), &SPHFluidRegion3D::get_kernel_support);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "kernel_support", PROPERTY_HINT_RANGE, "1,4,0.1"), "set_kernel_support", "get_kernel_support");

	ClassDB::bind_method(D_METHOD("set_timestep", "p_value"), &SPHFluidRegion3D::set_timestep);
	ClassDB::bind_method(D_METHOD("get_timestep"), &SPHFluidRegion3D::get_timestep);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "timestep", PROPERTY_HINT_RANGE, "0.00001,0.05,0.00001,or_greater"), "set_timestep", "get_timestep");

	ClassDB::bind_method(D_METHOD("set_rest_density", "p_value"), &SPHFluidRegion3D::set_rest_density);
	ClassDB::bind_method(D_METHOD("get_rest_density"), &SPHFluidRegion3D::get_rest_density);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rest_density", PROPERTY_HINT_RANGE, "0.001,100000,1,or_greater"), "set_rest_density", "get_rest_density");

	ClassDB::bind_method(D_METHOD("set_gravity", "p_value"), &SPHFluidRegion3D::set_gravity);
	ClassDB::bind_method(D_METHOD("get_gravity"), &SPHFluidRegion3D::get_gravity);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "gravity"), "set_gravity", "get_gravity");

	ClassDB::bind_method(D_METHOD("set_fill_ratio", "p_value"), &SPHFluidRegion3D::set_fill_ratio);
	ClassDB::bind_method(D_METHOD("get_fill_ratio"), &SPHFluidRegion3D::get_fill_ratio);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fill_ratio", PROPERTY_HINT_RANGE, "0.05,1,0.01"), "set_fill_ratio", "get_fill_ratio");

	ClassDB::bind_method(D_METHOD("set_generate_boundary", "p_value"), &SPHFluidRegion3D::set_generate_boundary);
	ClassDB::bind_method(D_METHOD("get_generate_boundary"), &SPHFluidRegion3D::get_generate_boundary);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generate_boundary"), "set_generate_boundary", "get_generate_boundary");

	ClassDB::bind_method(D_METHOD("set_particle_radius", "p_value"), &SPHFluidRegion3D::set_particle_radius);
	ClassDB::bind_method(D_METHOD("get_particle_radius"), &SPHFluidRegion3D::get_particle_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "particle_radius", PROPERTY_HINT_RANGE, "0,0.5,0.001,or_greater"), "set_particle_radius", "get_particle_radius");

	ClassDB::bind_method(D_METHOD("set_simulation_speed", "p_value"), &SPHFluidRegion3D::set_simulation_speed);
	ClassDB::bind_method(D_METHOD("get_simulation_speed"), &SPHFluidRegion3D::get_simulation_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "simulation_speed", PROPERTY_HINT_RANGE, "0,10,0.01,or_greater"), "set_simulation_speed", "get_simulation_speed");

	ClassDB::bind_method(D_METHOD("set_max_substeps", "p_value"), &SPHFluidRegion3D::set_max_substeps);
	ClassDB::bind_method(D_METHOD("get_max_substeps"), &SPHFluidRegion3D::get_max_substeps);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_substeps", PROPERTY_HINT_RANGE, "1,512,1,or_greater"), "set_max_substeps", "get_max_substeps");

	ClassDB::bind_method(D_METHOD("set_remove_escaped_particles", "p_value"), &SPHFluidRegion3D::set_remove_escaped_particles);
	ClassDB::bind_method(D_METHOD("get_remove_escaped_particles"), &SPHFluidRegion3D::get_remove_escaped_particles);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "remove_escaped_particles"), "set_remove_escaped_particles", "get_remove_escaped_particles");

	ClassDB::bind_method(D_METHOD("set_paused", "p_value"), &SPHFluidRegion3D::set_paused);
	ClassDB::bind_method(D_METHOD("get_paused"), &SPHFluidRegion3D::get_paused);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "paused"), "set_paused", "get_paused");

	ClassDB::bind_method(D_METHOD("get_active_particle_count"), &SPHFluidRegion3D::get_active_particle_count);
	ClassDB::bind_method(D_METHOD("get_average_density"), &SPHFluidRegion3D::get_average_density);
	ClassDB::bind_method(D_METHOD("reset_simulation"), &SPHFluidRegion3D::reset_simulation);
}

void SPHFluidRegion3D::_notification(int p_what) {
	if (p_what == NOTIFICATION_PREDELETE) {
		// The MultiMeshInstance3D is a child node and is deleted by the engine,
		// just clear our pointer to avoid dangling references.
		multimesh_instance = nullptr;
	}
}

sph::Vec3 SPHFluidRegion3D::to_sph(const Vector3 &p_v) const {
	return sph::Vec3(p_v.x, p_v.y, p_v.z);
}

bool SPHFluidRegion3D::compute_container_bounds(AABB &r_bounds) const {
	if (container_path.is_empty()) {
		return false;
	}
	Node *node = get_node_or_null(container_path);
	if (!node) {
		return false;
	}

	bool has_bounds = false;
	Vector3 bounds_min;
	Vector3 bounds_max;

	auto consider = [&](Object *p_obj) {
		CollisionShape3D *col = Object::cast_to<CollisionShape3D>(p_obj);
		if (!col) {
			return;
		}
		Ref<Shape3D> shape = col->get_shape();
		BoxShape3D *box = Object::cast_to<BoxShape3D>(shape.ptr());
		if (!box) {
			return;
		}

		const Transform3D xform = col->get_global_transform();
		const Vector3 half = box->get_size() * 0.5f;
		for (int sx = -1; sx <= 1; sx += 2) {
			for (int sy = -1; sy <= 1; sy += 2) {
				for (int sz = -1; sz <= 1; sz += 2) {
					const Vector3 corner(half.x * sx, half.y * sy, half.z * sz);
					const Vector3 world = xform.xform(corner);
					if (!has_bounds) {
						bounds_min = world;
						bounds_max = world;
						has_bounds = true;
					} else {
						bounds_min = bounds_min.min(world);
						bounds_max = bounds_max.max(world);
					}
				}
			}
		}
	};

	consider(node);
	for (int i = 0; i < node->get_child_count(); ++i) {
		consider(node->get_child(i));
	}

	if (!has_bounds) {
		return false;
	}

	r_bounds = AABB(bounds_min, bounds_max - bounds_min);
	return true;
}

double SPHFluidRegion3D::resolve_spacing(const AABB &p_bounds) const {
	if (smoothing_radius > 0.0) {
		return smoothing_radius;
	}

	const double fluid_height = p_bounds.size.y * std::clamp(fill_ratio, 0.05, 1.0);
	const double volume = double(p_bounds.size.x) * double(p_bounds.size.z) * fluid_height;
	const int target = std::max(1, particle_count);
	double spacing = std::cbrt(volume / double(target));
	if (!(spacing > 0.0)) {
		spacing = 0.05;
	}
	return spacing;
}

void SPHFluidRegion3D::build_particles(const AABB &p_bounds) {
	const Vector3 bounds_min = p_bounds.position;
	const Vector3 bounds_max = p_bounds.position + p_bounds.size;
	const Vector3 size = p_bounds.size;

	const double spacing = resolve_spacing(p_bounds);
	resolved_h = spacing;

	const double cell_volume = spacing * spacing * spacing;
	double mass = rest_density * cell_volume;
	if (!(mass > 0.0)) {
		mass = 1.0;
	}

	std::vector<sph::Particle> parts;

	// ---- Fluid particles: regular lattice filling the lower part of the domain.
	const double fluid_height = double(size.y) * std::clamp(fill_ratio, 0.05, 0.95);
	int nx = std::max(1, int(double(size.x) / spacing) - 1);
	int nz = std::max(1, int(double(size.z) / spacing) - 1);
	int ny = std::max(1, int(fluid_height / spacing) - 1);

	const double x0 = double(bounds_min.x) + 0.5 * (double(size.x) - (nx - 1) * spacing);
	const double z0 = double(bounds_min.z) + 0.5 * (double(size.z) - (nz - 1) * spacing);
	const double y0 = double(bounds_min.y) + 2.0 * spacing;

	// Keep the column below the ceiling.
	while (ny > 1 && y0 + (ny - 1) * spacing > double(bounds_max.y) - 2.0 * spacing) {
		ny--;
	}

	for (int ix = 0; ix < nx; ++ix) {
		for (int iy = 0; iy < ny; ++iy) {
			for (int iz = 0; iz < nz; ++iz) {
				sph::Particle p;
				p.is_stationary = false;
				p.mass = mass;
				p.rest_density = rest_density;
				p.pos = sph::Vec3(x0 + ix * spacing, y0 + iy * spacing, z0 + iz * spacing);
				parts.push_back(p);
			}
		}
	}

	// ---- Stationary boundary particles along the six faces of the domain,
	// matching the two-layer boundary the reference solver uses. Duplicated
	// lattice points (edges / corners) are removed so no particle is counted
	// twice in the density summation.
	if (generate_boundary) {
		std::unordered_set<uint64_t> seen;
		auto encode = [](int a, int b, int c) -> uint64_t {
			return (uint64_t(uint32_t(a)) << 42) ^ (uint64_t(uint32_t(b)) << 21) ^ uint64_t(uint32_t(c));
		};
		auto add_boundary = [&](double px, double py, double pz) {
			const int a = int(std::lround((px - double(bounds_min.x)) / spacing));
			const int b = int(std::lround((py - double(bounds_min.y)) / spacing));
			const int c = int(std::lround((pz - double(bounds_min.z)) / spacing));
			const uint64_t key = encode(a, b, c);
			if (!seen.insert(key).second) {
				return;
			}
			sph::Particle p;
			p.is_stationary = true;
			p.mass = mass;
			p.rest_density = rest_density;
			p.pos = sph::Vec3(px, py, pz);
			parts.push_back(p);
		};

		const int layers = 2;
		const double eps = spacing * 1e-4;

		// Floor (y-min) and ceiling (y-max).
		for (int l = 0; l < layers; ++l) {
			const double yf = double(bounds_min.y) + (l + 0.5) * spacing;
			const double yc = double(bounds_max.y) - (l + 0.5) * spacing;
			for (double x = double(bounds_min.x) + 0.5 * spacing; x <= double(bounds_max.x) - 0.5 * spacing + eps; x += spacing) {
				for (double z = double(bounds_min.z) + 0.5 * spacing; z <= double(bounds_max.z) - 0.5 * spacing + eps; z += spacing) {
					add_boundary(x, yf, z);
					add_boundary(x, yc, z);
				}
			}
		}

		// Walls on the x faces (x-min and x-max).
		for (int l = 0; l < layers; ++l) {
			const double xw0 = double(bounds_min.x) + (l + 0.5) * spacing;
			const double xw1 = double(bounds_max.x) - (l + 0.5) * spacing;
			for (double y = double(bounds_min.y) + 0.5 * spacing; y <= double(bounds_max.y) - 0.5 * spacing + eps; y += spacing) {
				for (double z = double(bounds_min.z) + 0.5 * spacing; z <= double(bounds_max.z) - 0.5 * spacing + eps; z += spacing) {
					add_boundary(xw0, y, z);
					add_boundary(xw1, y, z);
				}
			}
		}

		// Walls on the z faces (z-min and z-max).
		for (int l = 0; l < layers; ++l) {
			const double zw0 = double(bounds_min.z) + (l + 0.5) * spacing;
			const double zw1 = double(bounds_max.z) - (l + 0.5) * spacing;
			for (double y = double(bounds_min.y) + 0.5 * spacing; y <= double(bounds_max.y) - 0.5 * spacing + eps; y += spacing) {
				for (double x = double(bounds_min.x) + 0.5 * spacing; x <= double(bounds_max.x) - 0.5 * spacing + eps; x += spacing) {
					add_boundary(x, y, zw0);
					add_boundary(x, y, zw1);
				}
			}
		}
	}

	// ---- Configure and hand the particles to the solver.
	sph::Config conf;
	conf.particle_count = int(parts.size());
	conf.active_particles = 0; // computed by Simulator::initialize()
	conf.dim = 3;
	conf.h = spacing;
	conf.k = stiffness;
	conf.nu = viscosity;
	conf.kernel_support = kernel_support;
	conf.timestep = timestep;
	conf.gravity = to_sph(gravity);
	conf.area_min = to_sph(bounds_min);
	conf.area_max = to_sph(bounds_max);
	conf.remove_escaped = remove_escaped_particles;

	simulator.initialize(conf, std::move(parts));

	kernel = sph::Kernel(simulator.get_conf());
	kernel.initialize(simulator.get_particle_data());
	simulator.set_kernel(&kernel);
}

void SPHFluidRegion3D::create_multimesh() {
	const double radius = particle_radius > 0.0 ? particle_radius : resolved_h * 0.5;

	multimesh_instance = memnew(MultiMeshInstance3D);

	multimesh.instantiate();
	multimesh->set_transform_format(MultiMesh::TRANSFORM_3D);
	multimesh->set_instance_count(std::max(1, simulator.get_conf().active_particles));

	Ref<SphereMesh> sphere_mesh;
	sphere_mesh.instantiate();
	sphere_mesh->set_radius(float(radius));
	sphere_mesh->set_height(float(radius) * 2.0f);
	multimesh->set_mesh(sphere_mesh);

	multimesh_instance->set_multimesh(multimesh);
	add_child(multimesh_instance, false, Node::INTERNAL_MODE_FRONT);
}

void SPHFluidRegion3D::update_multimesh() {
	if (!multimesh.is_valid()) {
		return;
	}

	const std::vector<sph::Particle> &parts = simulator.get_particles();
	const int active = std::min<int>(simulator.get_conf().active_particles, int(parts.size()));

	if (multimesh->get_instance_count() != active) {
		multimesh->set_instance_count(std::max(1, active));
	}

	// Particle positions are stored in world space; instance transforms are
	// relative to this node.
	const Transform3D inv = get_global_transform().affine_inverse();

	for (int i = 0; i < active; ++i) {
		const sph::Vec3 &p = parts[i].pos;
		Transform3D t;
		t.origin = inv.xform(Vector3(float(p.x), float(p.y), float(p.z)));
		multimesh->set_instance_transform(i, t);
	}
}

void SPHFluidRegion3D::_ready() {
	UtilityFunctions::print("Inicializando region SPH (port frog711/sph-fluid-solver) con particle_count objetivo = ", particle_count, ".");

	AABB bounds;
	if (!compute_container_bounds(bounds)) {
		UtilityFunctions::print("SPHFluidRegion3D: no se pudo leer el contenedor, usando limites por defecto.");
		bounds = AABB(Vector3(-0.5f, 0.0f, -0.5f), Vector3(1.0f, 2.0f, 1.0f));
	}

	build_particles(bounds);
	create_multimesh();
	update_multimesh();

	time_accumulator = 0.0;
	initialized = true;

	UtilityFunctions::print("SPH: ", simulator.get_conf().particle_count, " particulas (",
			simulator.get_conf().active_particles, " fluido), h = ", resolved_h, ".");
}

void SPHFluidRegion3D::_process(double delta) {
	if (!initialized) {
		return;
	}

	const double dt = timestep > 0.0 ? timestep : 0.0005;

	if (!paused) {
		time_accumulator += delta * simulation_speed;

		int steps = 0;
		while (time_accumulator >= dt && steps < max_substeps) {
			simulator.simulate_step();
			time_accumulator -= dt;
			steps++;
			if (simulator.get_conf().active_particles <= 0) {
				break;
			}
		}
		// Avoid the "spiral of death" when the solver can not keep up.
		if (steps >= max_substeps) {
			time_accumulator = 0.0;
		}
	}

	update_multimesh();
}

// ---------------------------------------------------------------------------
// Getters / setters
// ---------------------------------------------------------------------------

void SPHFluidRegion3D::set_particle_count(int p_count) {
	particle_count = p_count;
}

int SPHFluidRegion3D::get_particle_count() const {
	return particle_count;
}

void SPHFluidRegion3D::set_container_path(const NodePath &p_path) {
	container_path = p_path;
}

NodePath SPHFluidRegion3D::get_container_path() const {
	return container_path;
}

void SPHFluidRegion3D::set_smoothing_radius(double p_value) {
	smoothing_radius = p_value;
}

double SPHFluidRegion3D::get_smoothing_radius() const {
	return smoothing_radius;
}

void SPHFluidRegion3D::set_stiffness(double p_value) {
	stiffness = p_value;
	if (initialized) {
		simulator.get_conf().k = p_value;
	}
}

double SPHFluidRegion3D::get_stiffness() const {
	return stiffness;
}

void SPHFluidRegion3D::set_viscosity(double p_value) {
	viscosity = p_value;
	if (initialized) {
		simulator.get_conf().nu = p_value;
	}
}

double SPHFluidRegion3D::get_viscosity() const {
	return viscosity;
}

void SPHFluidRegion3D::set_kernel_support(double p_value) {
	kernel_support = p_value;
}

double SPHFluidRegion3D::get_kernel_support() const {
	return kernel_support;
}

void SPHFluidRegion3D::set_timestep(double p_value) {
	timestep = p_value;
	if (initialized) {
		simulator.get_conf().timestep = p_value;
	}
}

double SPHFluidRegion3D::get_timestep() const {
	return timestep;
}

void SPHFluidRegion3D::set_rest_density(double p_value) {
	rest_density = p_value;
}

double SPHFluidRegion3D::get_rest_density() const {
	return rest_density;
}

void SPHFluidRegion3D::set_gravity(const Vector3 &p_value) {
	gravity = p_value;
	if (initialized) {
		simulator.get_conf().gravity = to_sph(p_value);
	}
}

Vector3 SPHFluidRegion3D::get_gravity() const {
	return gravity;
}

void SPHFluidRegion3D::set_fill_ratio(double p_value) {
	fill_ratio = p_value;
}

double SPHFluidRegion3D::get_fill_ratio() const {
	return fill_ratio;
}

void SPHFluidRegion3D::set_generate_boundary(bool p_value) {
	generate_boundary = p_value;
}

bool SPHFluidRegion3D::get_generate_boundary() const {
	return generate_boundary;
}

void SPHFluidRegion3D::set_particle_radius(double p_value) {
	particle_radius = p_value;
}

double SPHFluidRegion3D::get_particle_radius() const {
	return particle_radius;
}

void SPHFluidRegion3D::set_simulation_speed(double p_value) {
	simulation_speed = p_value;
}

double SPHFluidRegion3D::get_simulation_speed() const {
	return simulation_speed;
}

void SPHFluidRegion3D::set_max_substeps(int p_value) {
	max_substeps = std::max(1, p_value);
}

int SPHFluidRegion3D::get_max_substeps() const {
	return max_substeps;
}

void SPHFluidRegion3D::set_remove_escaped_particles(bool p_value) {
	remove_escaped_particles = p_value;
	if (initialized) {
		simulator.get_conf().remove_escaped = p_value;
	}
}

bool SPHFluidRegion3D::get_remove_escaped_particles() const {
	return remove_escaped_particles;
}

void SPHFluidRegion3D::set_paused(bool p_value) {
	paused = p_value;
}

bool SPHFluidRegion3D::get_paused() const {
	return paused;
}

int SPHFluidRegion3D::get_active_particle_count() const {
	return simulator.get_conf().active_particles;
}

double SPHFluidRegion3D::get_average_density() const {
	return const_cast<SPHFluidRegion3D *>(this)->simulator.get_average_density();
}

void SPHFluidRegion3D::reset_simulation() {
	if (multimesh_instance) {
		remove_child(multimesh_instance);
		memdelete(multimesh_instance);
		multimesh_instance = nullptr;
	}
	multimesh.unref();
	initialized = false;
	_ready();
}
