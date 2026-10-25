#ifndef SPH_FLUID_REGION_H
#define SPH_FLUID_REGION_H

#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "sph_kernel.h"
#include "sph_simulator.h"

namespace godot {

// Godot node that wraps the ported SPH solver (3D port of
// https://github.com/frog711/sph-fluid-solver) and renders the fluid with a
// MultiMesh. The solver itself lives in the sph_* files and is independent from
// Godot; this class only builds the initial particle set from the scene
// geometry, drives the solver and pushes the positions to the MultiMesh.
class SPHFluidRegion3D : public Node3D {
	GDCLASS(SPHFluidRegion3D, Node3D);

private:
	// --- Solver core ---
	sph::Simulator simulator;
	sph::Kernel kernel;
	bool initialized = false;
	double time_accumulator = 0.0;

	// --- Rendering ---
	MultiMeshInstance3D *multimesh_instance = nullptr;
	Ref<MultiMesh> multimesh;

	// --- Exported parameters ---
	int particle_count = 1000;
	NodePath container_path;
	double smoothing_radius = 0.0; // 0 => derived from particle_count
	double stiffness = 5000.0; // k
	double viscosity = 0.2; // nu
	double kernel_support = 2.0;
	double timestep = 0.0005;
	double rest_density = 1000.0;
	Vector3 gravity = Vector3(0.0, -9.81, 0.0);
	double fill_ratio = 0.5;
	bool generate_boundary = true;
	double particle_radius = 0.0; // 0 => derived (half the spacing)
	double simulation_speed = 1.0;
	int max_substeps = 32;
	bool remove_escaped_particles = false;
	bool paused = false;

	// --- Resolved at _ready() ---
	double resolved_h = 0.1;

	// Helpers
	bool compute_container_bounds(AABB &r_bounds) const;
	void build_particles(const AABB &p_bounds);
	void create_multimesh();
	void update_multimesh();
	double resolve_spacing(const AABB &p_bounds) const;
	sph::Vec3 to_sph(const Vector3 &p_v) const;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	SPHFluidRegion3D();
	~SPHFluidRegion3D();

	// particle_count
	void set_particle_count(int p_count);
	int get_particle_count() const;

	// container_path
	void set_container_path(const NodePath &p_path);
	NodePath get_container_path() const;

	void set_smoothing_radius(double p_value);
	double get_smoothing_radius() const;

	void set_stiffness(double p_value);
	double get_stiffness() const;

	void set_viscosity(double p_value);
	double get_viscosity() const;

	void set_kernel_support(double p_value);
	double get_kernel_support() const;

	void set_timestep(double p_value);
	double get_timestep() const;

	void set_rest_density(double p_value);
	double get_rest_density() const;

	void set_gravity(const Vector3 &p_value);
	Vector3 get_gravity() const;

	void set_fill_ratio(double p_value);
	double get_fill_ratio() const;

	void set_generate_boundary(bool p_value);
	bool get_generate_boundary() const;

	void set_particle_radius(double p_value);
	double get_particle_radius() const;

	void set_simulation_speed(double p_value);
	double get_simulation_speed() const;

	void set_max_substeps(int p_value);
	int get_max_substeps() const;

	void set_remove_escaped_particles(bool p_value);
	bool get_remove_escaped_particles() const;

	void set_paused(bool p_value);
	bool get_paused() const;

	// Runtime helpers exposed to GDScript / the editor.
	int get_active_particle_count() const;
	double get_average_density() const;
	void reset_simulation();

	void _ready() override;
	void _process(double delta) override;
};

} // namespace godot

#endif // SPH_FLUID_REGION_H
