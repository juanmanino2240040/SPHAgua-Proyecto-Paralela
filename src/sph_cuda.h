#ifndef SPH_CUDA_H
#define SPH_CUDA_H

// Interfaz del solver SPH en GPU (CUDA). No expone ningun tipo de CUDA, asi que
// se puede incluir desde codigo normal de Godot. La implementacion real esta en
// sph_cuda.cu; si compilas sin CUDA se usa sph_cuda_stub.cpp (todo devuelve false
// y el nodo cae automaticamente al solver CPU).

#include <string>

namespace sph {

struct CudaConfig {
	double h = 0.1;
	double k = 5000.0;
	double nu = 0.0;
	double kernel_support = 2.0;
	double timestep = 0.0005;
	double mass = 1.0;
	double rest_density = 1000.0;
	double gravity[3] = { 0.0, -9.81, 0.0 };
	double area_min[3] = { -0.5, 0.0, -0.5 };
	double area_max[3] = { 0.5, 2.0, 0.5 };
};

class CudaSolver {
public:
	CudaSolver();
	~CudaSolver();
	CudaSolver(const CudaSolver &) = delete;
	CudaSolver &operator=(const CudaSolver &) = delete;

	// true si hay al menos una GPU NVIDIA utilizable. Devuelve su nombre.
	static bool device_available(std::string *r_device_name = nullptr);

	// fluid_xyz / boundary_xyz: posiciones en MUNDO, intercaladas (x,y,z,x,y,z...).
	bool initialize(const CudaConfig &p_conf,
			const double *p_fluid_xyz, int p_fluid_count,
			const double *p_boundary_xyz, int p_boundary_count);

	// Parametros que se pueden cambiar en caliente (no requieren reconstruir la rejilla).
	void set_dynamic_params(double p_k, double p_nu, double p_dt, const double p_gravity[3]);

	// Avanza n pasos de simulacion sin sincronizar con la CPU.
	bool step(int p_steps);

	// Rellena r_out (12 * fluid_count floats) con el buffer de un MultiMesh
	// TRANSFORM_3D. p_world_to_node = matriz 3x4 por filas (m00 m01 m02 tx ...).
	bool fetch_multimesh(const double p_world_to_node[12], float *r_out);

	double average_density(); // misma definicion que Simulator::get_average_density()
	int fluid_count() const;
	double last_gpu_ms() const; // tiempo GPU del ultimo bloque de pasos
	const std::string &last_error() const;

private:
	struct Impl;
	Impl *impl = nullptr;
};

} // namespace sph

#endif // SPH_CUDA_H
