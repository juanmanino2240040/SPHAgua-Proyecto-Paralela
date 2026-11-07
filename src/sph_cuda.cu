// Solver SPH en GPU (CUDA). Ver sph_cuda.h para la interfaz y
// sph_cuda_kernels.cuh para la fisica.
//
// Pipeline de CADA paso (todo en GPU, sin sincronizar con la CPU):
//   k_cell_keys -> radix sort (CUB) -> k_cell_start -> k_gather
//   -> k_density -> k_forces_integrate
// Los buffers de estado (pos_a / vel_a) nunca salen de la GPU salvo para pintar.

#define _USE_MATH_DEFINES // M_PI en MSVC
#include <cuda_runtime.h>
#include <cub/cub.cuh>

#include "sph_cuda.h"
#include "sph_cuda_kernels.cuh"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <vector>

namespace sph {

namespace {
const double kCorrectionConst = 0.999139; // identica a sph_kernel.cpp
const int kBlock = 128;

inline int blocks_for(int n) { return (n + kBlock - 1) / kBlock; }
} // namespace

#define CUDA_TRY(call)                                                              \
	do {                                                                            \
		cudaError_t _e = (call);                                                    \
		if (_e != cudaSuccess) {                                                    \
			impl->err = std::string(#call) + " -> " + cudaGetErrorString(_e);      \
			return false;                                                           \
		}                                                                           \
	} while (0)

struct CudaSolver::Impl {
	sph_gpu::Params P{};
	double area_min[3] = { 0, 0, 0 };
	int ncell = 0;
	int end_bit = 1;
	bool ready = false;
	std::string err;

	// Estado (orden = orden por celda del ultimo paso) y buffers ordenados
	float4 *pos_a = nullptr, *vel_a = nullptr;
	float4 *pos_b = nullptr, *vel_b = nullptr;
	float *density = nullptr, *pressure = nullptr;
	unsigned int *keys_in = nullptr, *keys_out = nullptr, *idx_in = nullptr, *idx_out = nullptr;
	int *fstart = nullptr;
	// Borde (estatico)
	float4 *bpos = nullptr;
	int *bstart = nullptr;
	// Salida MultiMesh
	float4 *xf_out = nullptr;
	// CUB
	void *cub_tmp = nullptr;
	size_t cub_bytes = 0;

	cudaEvent_t ev0 = nullptr, ev1 = nullptr;
	double gpu_ms = 0.0;
	bool pending_timing = false;

	template <class T>
	static void free_ptr(T *&p) {
		if (p) {
			cudaFree(p);
			p = nullptr;
		}
	}

	void release() {
		free_ptr(pos_a); free_ptr(vel_a); free_ptr(pos_b); free_ptr(vel_b);
		free_ptr(density); free_ptr(pressure);
		free_ptr(keys_in); free_ptr(keys_out); free_ptr(idx_in); free_ptr(idx_out);
		free_ptr(fstart); free_ptr(bpos); free_ptr(bstart); free_ptr(xf_out);
		free_ptr(cub_tmp);
		if (ev0) { cudaEventDestroy(ev0); ev0 = nullptr; }
		if (ev1) { cudaEventDestroy(ev1); ev1 = nullptr; }
		ready = false;
	}
};

CudaSolver::CudaSolver() : impl(new Impl) {}
CudaSolver::~CudaSolver() {
	if (impl) {
		impl->release();
		delete impl;
	}
}

bool CudaSolver::device_available(std::string *r_name) {
	int count = 0;
	if (cudaGetDeviceCount(&count) != cudaSuccess || count <= 0) {
		return false;
	}
	if (r_name) {
		cudaDeviceProp prop;
		if (cudaGetDeviceProperties(&prop, 0) == cudaSuccess) {
			*r_name = prop.name;
		}
	}
	return true;
}

bool CudaSolver::initialize(const CudaConfig &c, const double *fluid_xyz, int n_fluid,
		const double *boundary_xyz, int n_boundary) {
	impl->release();
	if (n_fluid <= 0) {
		impl->err = "No hay particulas de fluido.";
		return false;
	}
	if (!device_available()) {
		impl->err = "No se encontro ninguna GPU NVIDIA con driver CUDA.";
		return false;
	}
	CUDA_TRY(cudaSetDevice(0));

	// ---- Rejilla (misma logica que Kernel::initialize) ----
	double cell = c.h * c.kernel_support;
	if (cell <= 0.0) {
		cell = c.h;
	}
	const double sx = c.area_max[0] - c.area_min[0];
	const double sy = c.area_max[1] - c.area_min[1];
	const double sz = c.area_max[2] - c.area_min[2];
	const int nx = std::max(1, int(sx / cell) + 1);
	const int ny = std::max(1, int(sy / cell) + 1);
	const int nz = std::max(1, int(sz / cell) + 1);
	const long long ncell_ll = (long long)nx * ny * nz;
	if (ncell_ll > 200000000LL) {
		impl->err = "La rejilla tiene demasiadas celdas; aumenta h o reduce el dominio.";
		return false;
	}
	impl->ncell = int(ncell_ll);
	impl->end_bit = 1;
	while ((1LL << impl->end_bit) < ncell_ll && impl->end_bit < 32) {
		impl->end_bit++;
	}
	for (int a = 0; a < 3; a++) {
		impl->area_min[a] = c.area_min[a];
	}

	// ---- Parametros ----
	const double alpha = 1.0 / (4.0 * M_PI * c.h * c.h * c.h);
	sph_gpu::Params &P = impl->P;
	P.nx = nx; P.ny = ny; P.nz = nz;
	P.n_fluid = n_fluid;
	P.n_boundary = n_boundary;
	P.inv_cell = float(1.0 / cell);
	P.lx = float(sx); P.ly = float(sy); P.lz = float(sz);
	P.h = float(c.h);
	P.inv_h = float(1.0 / c.h);
	P.support_sq = float((c.kernel_support * c.h) * (c.kernel_support * c.h));
	P.wcoef = float(kCorrectionConst * alpha);
	P.dwcoef = float(kCorrectionConst * alpha / (c.h * c.h));
	P.k = float(c.k); P.nu = float(c.nu); P.dt = float(c.timestep);
	P.mass = float(c.mass); P.rest_density = float(c.rest_density);
	P.gx = float(c.gravity[0]); P.gy = float(c.gravity[1]); P.gz = float(c.gravity[2]);

	// ---- Reserva de memoria ----
	const size_t nf = size_t(n_fluid);
	const size_t nb = size_t(std::max(1, n_boundary));
	CUDA_TRY(cudaMalloc(&impl->pos_a, nf * sizeof(float4)));
	CUDA_TRY(cudaMalloc(&impl->vel_a, nf * sizeof(float4)));
	CUDA_TRY(cudaMalloc(&impl->pos_b, nf * sizeof(float4)));
	CUDA_TRY(cudaMalloc(&impl->vel_b, nf * sizeof(float4)));
	CUDA_TRY(cudaMalloc(&impl->density, nf * sizeof(float)));
	CUDA_TRY(cudaMalloc(&impl->pressure, nf * sizeof(float)));
	CUDA_TRY(cudaMalloc(&impl->keys_in, nf * sizeof(unsigned int)));
	CUDA_TRY(cudaMalloc(&impl->keys_out, nf * sizeof(unsigned int)));
	CUDA_TRY(cudaMalloc(&impl->idx_in, nf * sizeof(unsigned int)));
	CUDA_TRY(cudaMalloc(&impl->idx_out, nf * sizeof(unsigned int)));
	CUDA_TRY(cudaMalloc(&impl->fstart, size_t(impl->ncell + 1) * sizeof(int)));
	CUDA_TRY(cudaMalloc(&impl->bpos, nb * sizeof(float4)));
	CUDA_TRY(cudaMalloc(&impl->bstart, size_t(impl->ncell + 1) * sizeof(int)));
	CUDA_TRY(cudaMalloc(&impl->xf_out, nf * 3 * sizeof(float4)));

	// ---- Fluido: posiciones relativas a area_min, velocidad 0 ----
	{
		std::vector<float4> h_pos(nf);
		for (size_t i = 0; i < nf; i++) {
			h_pos[i] = make_float4(float(fluid_xyz[3 * i + 0] - c.area_min[0]),
					float(fluid_xyz[3 * i + 1] - c.area_min[1]),
					float(fluid_xyz[3 * i + 2] - c.area_min[2]), 0.0f);
		}
		CUDA_TRY(cudaMemcpy(impl->pos_a, h_pos.data(), nf * sizeof(float4), cudaMemcpyHostToDevice));
		CUDA_TRY(cudaMemset(impl->vel_a, 0, nf * sizeof(float4)));
		CUDA_TRY(cudaMemset(impl->vel_b, 0, nf * sizeof(float4)));
	}

	// ---- Borde: rejilla estatica construida una sola vez en la CPU ----
	{
		std::vector<int> bs(size_t(impl->ncell) + 1, 0);
		std::vector<float4> sorted(nb, make_float4(0, 0, 0, 0));
		if (n_boundary > 0) {
			std::vector<float4> loc(n_boundary);
			std::vector<int> cellid(n_boundary);
			for (int i = 0; i < n_boundary; i++) {
				loc[i] = make_float4(float(boundary_xyz[3 * i + 0] - c.area_min[0]),
						float(boundary_xyz[3 * i + 1] - c.area_min[1]),
						float(boundary_xyz[3 * i + 2] - c.area_min[2]), 0.0f);
				const int gx = std::min(std::max(int(loc[i].x * P.inv_cell), 0), nx - 1);
				const int gy = std::min(std::max(int(loc[i].y * P.inv_cell), 0), ny - 1);
				const int gz = std::min(std::max(int(loc[i].z * P.inv_cell), 0), nz - 1);
				cellid[i] = (gz * ny + gy) * nx + gx;
				bs[size_t(cellid[i]) + 1]++;
			}
			for (int i = 0; i < impl->ncell; i++) {
				bs[size_t(i) + 1] += bs[size_t(i)];
			}
			std::vector<int> cursor(bs.begin(), bs.end() - 1);
			for (int i = 0; i < n_boundary; i++) {
				sorted[size_t(cursor[size_t(cellid[i])]++)] = loc[i];
			}
		}
		CUDA_TRY(cudaMemcpy(impl->bpos, sorted.data(), nb * sizeof(float4), cudaMemcpyHostToDevice));
		CUDA_TRY(cudaMemcpy(impl->bstart, bs.data(), bs.size() * sizeof(int), cudaMemcpyHostToDevice));
	}

	// ---- CUB: memoria temporal del radix sort, reservada una sola vez ----
	CUDA_TRY(cub::DeviceRadixSort::SortPairs(nullptr, impl->cub_bytes, impl->keys_in, impl->keys_out,
			impl->idx_in, impl->idx_out, n_fluid, 0, impl->end_bit));
	CUDA_TRY(cudaMalloc(&impl->cub_tmp, impl->cub_bytes));

	CUDA_TRY(cudaEventCreate(&impl->ev0));
	CUDA_TRY(cudaEventCreate(&impl->ev1));

	impl->ready = true;
	impl->err.clear();
	return true;
}

void CudaSolver::set_dynamic_params(double k, double nu, double dt, const double g[3]) {
	sph_gpu::Params &P = impl->P;
	P.k = float(k);
	P.nu = float(nu);
	P.dt = float(dt);
	P.gx = float(g[0]);
	P.gy = float(g[1]);
	P.gz = float(g[2]);
}

bool CudaSolver::step(int steps) {
	if (!impl->ready) {
		return false;
	}
	const sph_gpu::Params &P = impl->P;
	const int n = P.n_fluid;
	const int gb = blocks_for(n);

	CUDA_TRY(cudaEventRecord(impl->ev0));
	for (int s = 0; s < steps; s++) {
		sph_gpu::k_cell_keys<<<gb, kBlock>>>(P, impl->pos_a, impl->keys_in, impl->idx_in);
		CUDA_TRY(cub::DeviceRadixSort::SortPairs(impl->cub_tmp, impl->cub_bytes, impl->keys_in, impl->keys_out,
				impl->idx_in, impl->idx_out, n, 0, impl->end_bit));
		sph_gpu::k_cell_start<<<blocks_for(impl->ncell + 1), kBlock>>>(impl->keys_out, n, impl->ncell, impl->fstart);
		sph_gpu::k_gather<<<gb, kBlock>>>(n, impl->idx_out, impl->pos_a, impl->vel_a, impl->pos_b, impl->vel_b);
		sph_gpu::k_density<<<gb, kBlock>>>(P, impl->pos_b, impl->fstart, impl->bpos, impl->bstart,
				impl->density, impl->pressure);
		sph_gpu::k_forces_integrate<<<gb, kBlock>>>(P, impl->pos_b, impl->vel_b, impl->density, impl->pressure,
				impl->fstart, impl->bpos, impl->bstart, impl->pos_a, impl->vel_a);
	}
	CUDA_TRY(cudaGetLastError());
	CUDA_TRY(cudaEventRecord(impl->ev1));
	impl->pending_timing = true;
	return true;
}

bool CudaSolver::fetch_multimesh(const double w2n[12], float *out) {
	if (!impl->ready) {
		return false;
	}
	// Composicion en doble precision: nodo = M * (local + area_min) + t
	sph_gpu::XForm xf;
	for (int r = 0; r < 3; r++) {
		double t = w2n[r * 4 + 3];
		for (int col = 0; col < 3; col++) {
			xf.m[r * 3 + col] = float(w2n[r * 4 + col]);
			t += w2n[r * 4 + col] * impl->area_min[col];
		}
		xf.t[r] = float(t);
	}
	const int n = impl->P.n_fluid;
	sph_gpu::k_multimesh<<<blocks_for(n), kBlock>>>(n, impl->pos_a, xf, impl->xf_out);
	CUDA_TRY(cudaGetLastError());
	// Esta copia sincrona espera a todos los pasos pendientes.
	CUDA_TRY(cudaMemcpy(out, impl->xf_out, size_t(n) * 12 * sizeof(float), cudaMemcpyDeviceToHost));
	if (impl->pending_timing) {
		float ms = 0.0f;
		if (cudaEventElapsedTime(&ms, impl->ev0, impl->ev1) == cudaSuccess) {
			impl->gpu_ms = ms;
		}
		impl->pending_timing = false;
	}
	return true;
}

double CudaSolver::average_density() {
	if (!impl->ready) {
		return 0.0;
	}
	const int n = impl->P.n_fluid;
	std::vector<float> d(size_t(n));
	if (cudaMemcpy(d.data(), impl->density, size_t(n) * sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) {
		return 0.0;
	}
	double sum = 0.0;
	int valid = 0;
	for (float v : d) {
		if (v >= impl->P.rest_density) {
			sum += v;
			valid++;
		}
	}
	return valid ? sum / valid : 0.0;
}

int CudaSolver::fluid_count() const { return impl->ready ? impl->P.n_fluid : 0; }
double CudaSolver::last_gpu_ms() const { return impl->gpu_ms; }
const std::string &CudaSolver::last_error() const { return impl->err; }

} // namespace sph
