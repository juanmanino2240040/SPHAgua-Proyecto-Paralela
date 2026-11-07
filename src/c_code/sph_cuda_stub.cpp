#include "sph_cuda.h"

// Implementacion "vacia" para compilaciones sin CUDA. SConstruct compila este
// archivo cuando no se pasa cuda=1 (o cuando no encuentra nvcc), de forma que
// la extension sigue funcionando con el solver CPU de siempre.
#if !defined(SPH_HAS_CUDA)

#include <string>

namespace sph {

struct CudaSolver::Impl {
	Config conf;
	std::string error = "extension compilada sin CUDA";
};

CudaSolver::CudaSolver() :
		impl(new Impl()) {}

CudaSolver::~CudaSolver() {
	delete impl;
}

bool CudaSolver::runtime_available() {
	return false;
}

bool CudaSolver::initialize(const Config &p_conf, const std::vector<Particle> &) {
	impl->conf = p_conf;
	impl->error = "extension compilada sin CUDA (recompila con cuda=1)";
	return false;
}

void CudaSolver::update_conf(const Config &p_conf) {
	impl->conf = p_conf;
}

void CudaSolver::simulate_step() {}

void CudaSolver::simulate_step(CudaPhaseTimes *) {}

void CudaSolver::synchronize() const {}

int CudaSolver::copy_positions(std::vector<Vec3> &r_out) const {
	r_out.clear();
	return 0;
}

CudaStats CudaSolver::stats() const {
	return CudaStats();
}

double CudaSolver::average_density() const {
	return 0.0;
}

int CudaSolver::active_particles() const {
	return impl->conf.active_particles;
}

int CudaSolver::total_particles() const {
	return impl->conf.particle_count;
}

bool CudaSolver::is_initialized() const {
	return false;
}

const char *CudaSolver::error() const {
	return impl->error.c_str();
}

Config &CudaSolver::get_conf() {
	return impl->conf;
}

const Config &CudaSolver::get_conf() const {
	return impl->conf;
}

} // namespace sph

#endif // !SPH_HAS_CUDA
