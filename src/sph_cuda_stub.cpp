// Version "sin CUDA" de CudaSolver: permite compilar la extension en maquinas sin
// toolkit. initialize() falla y el nodo usa el solver CPU (OpenMP).
#include "sph_cuda.h"

namespace sph {

struct CudaSolver::Impl {
	std::string err = "Extension compilada sin soporte CUDA (scons cuda=yes).";
};

CudaSolver::CudaSolver() : impl(new Impl) {}
CudaSolver::~CudaSolver() { delete impl; }
bool CudaSolver::device_available(std::string *) { return false; }
bool CudaSolver::initialize(const CudaConfig &, const double *, int, const double *, int) { return false; }
void CudaSolver::set_dynamic_params(double, double, double, const double[3]) {}
bool CudaSolver::step(int) { return false; }
bool CudaSolver::fetch_multimesh(const double[12], float *) { return false; }
double CudaSolver::average_density() { return 0.0; }
int CudaSolver::fluid_count() const { return 0; }
double CudaSolver::last_gpu_ms() const { return 0.0; }
const std::string &CudaSolver::last_error() const { return impl->err; }

} // namespace sph
