#include "sph_cuda.h"

// Backend CUDA del solver SPH. Solo se compila cuando SPH_HAS_CUDA esta
// definido (ver SConstruct y bench/Makefile); en caso contrario se enlaza
// sph_cuda_stub.cpp.
#if defined(SPH_HAS_CUDA)

#include <cuda_runtime.h>
#include <thrust/device_ptr.h>
#include <thrust/execution_policy.h>
#include <thrust/scan.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace sph {

namespace {

// Misma correccion empirica que el solver CPU (sph_kernel.cpp).
constexpr float kCorrection = 0.999139f;

// Capacidad fija por particula para la lista de vecinos. Con un radio de
// soporte de 2h la malla regular produce ~33 vecinos; 128 deja margen para
// fronteras, esquinas y configuraciones con h pequena.
constexpr int kMaxNeighbors = 128;

constexpr int kBlockSize = 256;

struct Params {
	float h;
	float alpha;
	float support_sq;
	float cell_size;
	float min_x, min_y, min_z;
	float gravity_x, gravity_y, gravity_z;
	float k;
	float nu;
	float dt;
	int nx, ny, nz;
	int active;
};

__device__ __forceinline__ float kernel_w(float dx, float dy, float dz, float inv_h, float alpha) {
	const float q = sqrtf((dx * dx + dy * dy + dz * dz) * inv_h * inv_h);
	const float q1 = fmaxf(0.0f, 1.0f - q);
	const float q2 = fmaxf(0.0f, 2.0f - q);
	return kCorrection * alpha * (q2 * q2 * q2 - 4.0f * q1 * q1 * q1);
}

// Escribe el gradiente de W en (wx, wy, wz). dx/dy/dz son deltas en espacio
// real (pos_i - pos_j), igual que en sph_kernel.cpp.
__device__ __forceinline__ void kernel_derivative(float dx, float dy, float dz, float inv_h, float h, float alpha,
		float &wx, float &wy, float &wz) {
	const float nx = dx * inv_h;
	const float ny = dy * inv_h;
	const float nz = dz * inv_h;
	const float dist = sqrtf(nx * nx + ny * ny + nz * nz);
	const float q1 = fmaxf(0.0f, 1.0f - dist);
	const float q2 = fmaxf(0.0f, 2.0f - dist);
	const float inner = -3.0f * q2 * q2 + 12.0f * q1 * q1;
	float common = 0.0f;
	if (dist > 0.001f) {
		common = alpha * inner / (dist * h * h);
	}
	wx = kCorrection * dx * common;
	wy = kCorrection * dy * common;
	wz = kCorrection * dz * common;
}

__host__ __device__ __forceinline__ float kernel_factor(int dim, float h) {
	if (dim == 1) {
		return 1.0f / (6.0f * h);
	} else if (dim == 2) {
		return 5.0f / (14.0f * 3.14159265358979323846f * h * h);
	}
	return 1.0f / (4.0f * 3.14159265358979323846f * h * h * h);
}

__device__ __forceinline__ int cell_coord(float value, float min_value, float cell_size, int cells) {
	int c = int((value - min_value) / cell_size);
	c = min(max(c, 0), cells - 1);
	return c;
}

// ---------------------------------------------------------------------------
// Kernels
// ---------------------------------------------------------------------------

__global__ void k_probe() {}

__global__ void k_clear_int(int *p_data, int p_count) {
	const int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i < p_count) {
		p_data[i] = 0;
	}
}

__global__ void k_copy_int(const int *p_src, int *p_dst, int p_count) {
	const int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i < p_count) {
		p_dst[i] = p_src[i];
	}
}

// Pone la aceleracion a la gravedad. El solver CPU hace acc=0 y luego suma la
// gravedad; el resultado es identico y aqui ahorra una pasada.
__global__ void k_reset_acc(float *p_ax, float *p_ay, float *p_az, int p_active, Params p) {
	const int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i < p_active) {
		p_ax[i] = p.gravity_x;
		p_ay[i] = p.gravity_y;
		p_az[i] = p.gravity_z;
	}
}

__global__ void k_compute_cells(const float *p_px, const float *p_py, const float *p_pz, int *p_cell_of, int p_n,
		Params p) {
	const int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i < p_n) {
		const int gx = cell_coord(p_px[i], p.min_x, p.cell_size, p.nx);
		const int gy = cell_coord(p_py[i], p.min_y, p.cell_size, p.ny);
		const int gz = cell_coord(p_pz[i], p.min_z, p.cell_size, p.nz);
		p_cell_of[i] = (gz * p.ny + gy) * p.nx + gx;
	}
}

__global__ void k_count_cells(const int *p_cell_of, int *p_counts, int p_n) {
	const int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i < p_n) {
		atomicAdd(&p_counts[p_cell_of[i]], 1);
	}
}

__global__ void k_scatter_cells(const int *p_cell_of, int *p_cursors, int *p_cell_particles, int p_n) {
	const int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i < p_n) {
		const int slot = atomicAdd(&p_cursors[p_cell_of[i]], 1);
		p_cell_particles[slot] = i;
	}
}

__global__ void k_query_neighbors(const float *p_px, const float *p_py, const float *p_pz, const int *p_cell_of,
		const int *p_starts, const int *p_cell_particles, int *p_nbr, int *p_nbr_count, Params p) {
	const int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i >= p.active) {
		return;
	}

	const int cell = p_cell_of[i];
	const int gx = cell % p.nx;
	const int gy = (cell / p.nx) % p.ny;
	const int gz = cell / (p.nx * p.ny);

	int count = 0;
	const int base = i * kMaxNeighbors;

	for (int dz = -1; dz <= 1; ++dz) {
		const int cz = gz + dz;
		if (cz < 0 || cz >= p.nz) {
			continue;
		}
		for (int dy = -1; dy <= 1; ++dy) {
			const int cy = gy + dy;
			if (cy < 0 || cy >= p.ny) {
				continue;
			}
			for (int dx = -1; dx <= 1; ++dx) {
				const int cx = gx + dx;
				if (cx < 0 || cx >= p.nx) {
					continue;
				}
				const int c = (cz * p.ny + cy) * p.nx + cx;
				for (int slot = p_starts[c]; slot < p_starts[c + 1]; ++slot) {
					const int j = p_cell_particles[slot];
					const float ddx = p_px[i] - p_px[j];
					const float ddy = p_py[i] - p_py[j];
					const float ddz = p_pz[i] - p_pz[j];
					if (ddx * ddx + ddy * ddy + ddz * ddz < p.support_sq) {
						if (count < kMaxNeighbors) {
							p_nbr[base + count] = j;
						}
						++count;
					}
				}
			}
		}
	}

	p_nbr_count[i] = count < kMaxNeighbors ? count : kMaxNeighbors;
}

__global__ void k_density_pressure(const float *p_px, const float *p_py, const float *p_pz, const float *p_mass,
		const float *p_rest, const int *p_nbr, const int *p_nbr_count, float *p_density, float *p_pressure, Params p) {
	const int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i >= p.active) {
		return;
	}

	const float inv_h = 1.0f / p.h;
	const int base = i * kMaxNeighbors;
	const int count = p_nbr_count[i];

	float rho = 0.0f;
	for (int k = 0; k < count; ++k) {
		const int j = p_nbr[base + k];
		const float dx = p_px[i] - p_px[j];
		const float dy = p_py[i] - p_py[j];
		const float dz = p_pz[i] - p_pz[j];
		rho += p_mass[j] * kernel_w(dx, dy, dz, inv_h, p.alpha);
	}

	p_density[i] = rho;
	p_pressure[i] = fmaxf(p.k * (rho - p_rest[i]), 0.0f);
}

__global__ void k_pressure_forces(const float *p_px, const float *p_py, const float *p_pz, const float *p_mass,
		const float *p_density, const float *p_pressure, const unsigned char *p_stationary, const int *p_nbr,
		const int *p_nbr_count, float *p_ax, float *p_ay, float *p_az, Params p) {
	const int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i >= p.active) {
		return;
	}

	const float inv_h = 1.0f / p.h;
	const float rho_i = p_density[i];
	const float inv_rho_i_sq = rho_i > 0.0f ? 1.0f / (rho_i * rho_i) : 0.0f;
	const int base = i * kMaxNeighbors;
	const int count = p_nbr_count[i];

	float ax = p_ax[i];
	float ay = p_ay[i];
	float az = p_az[i];

	for (int k = 0; k < count; ++k) {
		const int j = p_nbr[base + k];
		const float dx = p_px[i] - p_px[j];
		const float dy = p_py[i] - p_py[j];
		const float dz = p_pz[i] - p_pz[j];

		float coef;
		if (p_stationary[j]) {
			coef = 2.0f * p_mass[j] * (p_pressure[i] * inv_rho_i_sq);
		} else {
			const float rho_j = p_density[j];
			const float inv_rho_j_sq = rho_j > 0.0f ? 1.0f / (rho_j * rho_j) : 0.0f;
			coef = p_mass[j] * (p_pressure[i] * inv_rho_i_sq + p_pressure[j] * inv_rho_j_sq);
		}

		float wx, wy, wz;
		kernel_derivative(dx, dy, dz, inv_h, p.h, p.alpha, wx, wy, wz);
		ax -= coef * wx;
		ay -= coef * wy;
		az -= coef * wz;
	}

	p_ax[i] = ax;
	p_ay[i] = ay;
	p_az[i] = az;
}

__global__ void k_viscous_forces(const float *p_px, const float *p_py, const float *p_pz, const float *p_vx,
		const float *p_vy, const float *p_vz, const float *p_mass, const float *p_density, const unsigned char *p_stationary,
		const int *p_nbr, const int *p_nbr_count, float *p_ax, float *p_ay, float *p_az, Params p) {
	const int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i >= p.active) {
		return;
	}

	const float inv_h = 1.0f / p.h;
	const float h2 = p.h * p.h;
	const int base = i * kMaxNeighbors;
	const int count = p_nbr_count[i];

	float ax = p_ax[i];
	float ay = p_ay[i];
	float az = p_az[i];

	for (int k = 0; k < count; ++k) {
		const int j = p_nbr[base + k];
		const float dx = p_px[i] - p_px[j];
		const float dy = p_py[i] - p_py[j];
		const float dz = p_pz[i] - p_pz[j];

		float coef;
		if (p_stationary[j]) {
			coef = 2.0f * p.nu * p_mass[j] / p_density[i];
		} else {
			coef = 2.0f * p.nu * p_mass[j] / p_density[j];
		}

		const float vx = p_vx[i] - p_vx[j];
		const float vy = p_vy[i] - p_vy[j];
		const float vz = p_vz[i] - p_vz[j];
		const float denom = dx * dx + dy * dy + dz * dz + 0.01f * h2;

		float wx, wy, wz;
		kernel_derivative(dx, dy, dz, inv_h, p.h, p.alpha, wx, wy, wz);
		ax += coef * (vx * dx) / denom * wx;
		ay += coef * (vy * dy) / denom * wy;
		az += coef * (vz * dz) / denom * wz;
	}

	p_ax[i] = ax;
	p_ay[i] = ay;
	p_az[i] = az;
}

__global__ void k_integrate(float *p_px, float *p_py, float *p_pz, float *p_vx, float *p_vy, float *p_vz,
		const float *p_ax, const float *p_ay, const float *p_az, int p_active, Params p) {
	const int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i < p_active) {
		const float vx = p_vx[i] + p_ax[i] * p.dt;
		const float vy = p_vy[i] + p_ay[i] * p.dt;
		const float vz = p_vz[i] + p_az[i] * p.dt;
		p_vx[i] = vx;
		p_vy[i] = vy;
		p_vz[i] = vz;
		p_px[i] += vx * p.dt;
		p_py[i] += vy * p.dt;
		p_pz[i] += vz * p.dt;
	}
}

__global__ void k_clamp_bounds(float *p_px, float *p_py, float *p_pz, float *p_vx, float *p_vy, float *p_vz,
		int p_active, float min_x, float min_y, float min_z, float max_x, float max_y, float max_z) {
	const int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i >= p_active) {
		return;
	}

	float px = p_px[i];
	float py = p_py[i];
	float pz = p_pz[i];
	float vx = p_vx[i];
	float vy = p_vy[i];
	float vz = p_vz[i];

	if (px < min_x) {
		px = min_x;
		if (vx < 0.0f) vx = 0.0f;
	} else if (px > max_x) {
		px = max_x;
		if (vx > 0.0f) vx = 0.0f;
	}
	if (py < min_y) {
		py = min_y;
		if (vy < 0.0f) vy = 0.0f;
	} else if (py > max_y) {
		py = max_y;
		if (vy > 0.0f) vy = 0.0f;
	}
	if (pz < min_z) {
		pz = min_z;
		if (vz < 0.0f) vz = 0.0f;
	} else if (pz > max_z) {
		pz = max_z;
		if (vz > 0.0f) vz = 0.0f;
	}

	p_px[i] = px;
	p_py[i] = py;
	p_pz[i] = pz;
	p_vx[i] = vx;
	p_vy[i] = vy;
	p_vz[i] = vz;
}

// ---------------------------------------------------------------------------
// Utilidades de host
// ---------------------------------------------------------------------------

void fnv_mix(uint64_t &h, double v) {
	uint64_t bits;
	std::memcpy(&bits, &v, sizeof(bits));
	for (int i = 0; i < 8; ++i) {
		h ^= (bits >> (8 * i)) & 0xffu;
		h *= 1099511628211ull;
	}
}

} // namespace

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct CudaSolver::Impl {
	Config conf;
	bool initialized = false;
	std::string error;

	int n = 0;
	int active = 0;
	int grid_nx = 0;
	int grid_ny = 0;
	int grid_nz = 0;
	int grid_max = 0;
	double cell_size = 0.0;

	float *px = nullptr, *py = nullptr, *pz = nullptr;
	float *vx = nullptr, *vy = nullptr, *vz = nullptr;
	float *ax = nullptr, *ay = nullptr, *az = nullptr;
	float *density = nullptr, *pressure = nullptr;
	float *rest_density = nullptr, *mass = nullptr;
	unsigned char *stationary = nullptr;

	int *nbr = nullptr;
	int *nbr_count = nullptr;

	int *cell_of = nullptr;
	int *cell_particles = nullptr;
	int *cell_counts = nullptr;
	int *cell_starts = nullptr;
	int *cell_cursors = nullptr;

	// Eventos para el perfilado por etapas (solo se crean si se piden tiempos).
	static constexpr int kPhaseEvents = 9;
	cudaEvent_t events[kPhaseEvents] = {};
	bool events_ready = false;

	~Impl() {
		release();
	}

	void release() {
		initialized = false;
		n = 0;
		active = 0;

		release_events();

		free_device((void **)&px);
		free_device((void **)&py);
		free_device((void **)&pz);
		free_device((void **)&vx);
		free_device((void **)&vy);
		free_device((void **)&vz);
		free_device((void **)&ax);
		free_device((void **)&ay);
		free_device((void **)&az);
		free_device((void **)&density);
		free_device((void **)&pressure);
		free_device((void **)&rest_density);
		free_device((void **)&mass);
		free_device((void **)&stationary);
		free_device((void **)&nbr);
		free_device((void **)&nbr_count);

		release_grid();
	}

	void release_grid() {
		free_device((void **)&cell_of);
		free_device((void **)&cell_particles);
		free_device((void **)&cell_counts);
		free_device((void **)&cell_starts);
		free_device((void **)&cell_cursors);
	}

	void release_events() {
		if (!events_ready) {
			return;
		}
		for (int i = 0; i < kPhaseEvents; ++i) {
			if (events[i]) {
				cudaEventDestroy(events[i]);
				events[i] = nullptr;
			}
		}
		events_ready = false;
	}

	bool ensure_events() {
		if (events_ready) {
			return true;
		}
		for (int i = 0; i < kPhaseEvents; ++i) {
			const cudaError_t e = cudaEventCreate(&events[i]);
			if (e != cudaSuccess) {
				fail_cuda("cudaEventCreate (perfilado)", e);
				release_events();
				return false;
			}
		}
		events_ready = true;
		return true;
	}

	static void free_device(void **p_ptr) {
		if (*p_ptr) {
			cudaFree(*p_ptr);
			*p_ptr = nullptr;
		}
	}

	void fail(const std::string &p_message) {
		error = p_message;
		initialized = false;
	}

	void fail_cuda(const char *p_where, cudaError_t p_error) {
		fail(std::string(p_where) + ": " + cudaGetErrorString(p_error));
	}

	bool alloc(void **p_ptr, size_t p_bytes, const char *p_what) {
		cudaError_t e = cudaMalloc(p_ptr, p_bytes);
		if (e != cudaSuccess) {
			fail_cuda(p_what, e);
			return false;
		}
		return true;
	}

	bool download(const float *p_device, float *p_host, size_t p_count, const char *p_what) {
		if (p_count == 0) {
			return true;
		}
		cudaError_t e = cudaMemcpy(p_host, p_device, p_count * sizeof(float), cudaMemcpyDeviceToHost);
		if (e != cudaSuccess) {
			fail_cuda(p_what, e);
			return false;
		}
		return true;
	}

	bool download_int(const int *p_device, int *p_host, size_t p_count, const char *p_what) {
		if (p_count == 0) {
			return true;
		}
		cudaError_t e = cudaMemcpy(p_host, p_device, p_count * sizeof(int), cudaMemcpyDeviceToHost);
		if (e != cudaSuccess) {
			fail_cuda(p_what, e);
			return false;
		}
		return true;
	}

	Params params() const {
		Params p{};
		p.h = float(conf.h);
		p.alpha = kernel_factor(conf.dim, float(conf.h));
		const double support = double(conf.kernel_support) * double(conf.h);
		p.support_sq = float(support * support);
		p.cell_size = float(cell_size);
		p.min_x = float(conf.area_min.x);
		p.min_y = float(conf.area_min.y);
		p.min_z = float(conf.area_min.z);
		p.gravity_x = float(conf.gravity.x);
		p.gravity_y = float(conf.gravity.y);
		p.gravity_z = float(conf.gravity.z);
		p.k = float(conf.k);
		p.nu = float(conf.nu);
		p.dt = float(conf.timestep);
		p.nx = grid_nx;
		p.ny = grid_ny;
		p.nz = grid_nz;
		p.active = active;
		return p;
	}

	bool configure_grid() {
		release_grid();

		cell_size = conf.h * conf.kernel_support;
		if (!(cell_size > 0.0)) {
			cell_size = conf.h > 0.0 ? conf.h : 0.1;
		}

		const double span_x = conf.area_max.x - conf.area_min.x;
		const double span_y = conf.area_max.y - conf.area_min.y;
		const double span_z = conf.area_max.z - conf.area_min.z;

		grid_nx = std::max(1, int(span_x / cell_size) + 1);
		grid_ny = std::max(1, int(span_y / cell_size) + 1);
		grid_nz = std::max(1, int(span_z / cell_size) + 1);
		grid_max = grid_nx * grid_ny * grid_nz;

		// Limite defensivo: una rejilla absurda suele indicar h=0 o un dominio
		// mal configurado, y agotaria la memoria de la GPU.
		if (grid_max <= 0 || size_t(grid_max) > (size_t(1) << 26)) {
			fail("rejilla CUDA demasiado grande (revisa smoothing_radius/kernel_support)");
			return false;
		}

		const size_t cells = size_t(grid_max) + 1;
		if (n > 0) {
			if (!alloc((void **)&cell_of, size_t(n) * sizeof(int), "cudaMalloc cell_of")) return false;
			if (!alloc((void **)&cell_particles, size_t(n) * sizeof(int), "cudaMalloc cell_particles")) return false;
		}
		if (!alloc((void **)&cell_counts, cells * sizeof(int), "cudaMalloc cell_counts")) return false;
		if (!alloc((void **)&cell_starts, cells * sizeof(int), "cudaMalloc cell_starts")) return false;
		if (!alloc((void **)&cell_cursors, cells * sizeof(int), "cudaMalloc cell_cursors")) return false;
		return true;
	}
};

// ---------------------------------------------------------------------------
// Implementacion publica
// ---------------------------------------------------------------------------

CudaSolver::CudaSolver() :
		impl(new Impl()) {}

CudaSolver::~CudaSolver() {
	delete impl;
}

bool CudaSolver::runtime_available() {
	int count = 0;
	if (cudaGetDeviceCount(&count) != cudaSuccess || count <= 0) {
		cudaGetLastError();
		return false;
	}
	if (cudaSetDevice(0) != cudaSuccess) {
		cudaGetLastError();
		return false;
	}

	k_probe<<<1, 1>>>();
	// Ojo: el error "no kernel image is available" aparece en cudaGetLastError,
	// no en cudaDeviceSynchronize (que puede devolver cudaSuccess).
	cudaError_t e = cudaGetLastError();
	if (e == cudaSuccess) {
		e = cudaDeviceSynchronize();
	}
	if (e != cudaSuccess) {
		// Limpia el error para no contaminar llamadas posteriores.
		cudaGetLastError();
		return false;
	}
	return true;
}

bool CudaSolver::initialize(const Config &p_conf, const std::vector<Particle> &p_particles) {
	impl->release();
	impl->error.clear();
	impl->conf = p_conf;

	if (p_conf.remove_escaped) {
		impl->fail("el backend CUDA no soporta remove_escaped_particles (usa el backend CPU)");
		return false;
	}
	if (!runtime_available()) {
		impl->fail("no hay una GPU CUDA compatible (o el binario no tiene codigo para su arquitectura)");
		return false;
	}

	// Mismo invariante que Simulator::initialize: fluido primero y frontera
	// despues. std::partition conserva el orden relativo de cada grupo.
	std::vector<Particle> parts = p_particles;
	auto first_stationary = std::partition(parts.begin(), parts.end(),
			[](const Particle &p) { return !p.is_stationary; });

	impl->conf.active_particles = int(first_stationary - parts.begin());
	impl->conf.particle_count = int(parts.size());
	impl->n = int(parts.size());
	impl->active = impl->conf.active_particles;

	const double cell_volume = std::pow(impl->conf.h, double(impl->conf.dim));
	for (Particle &p : parts) {
		if (p.rest_density <= 0.0 && cell_volume > 0.0) {
			p.rest_density = p.mass / cell_volume;
		}
	}

	if (impl->n == 0) {
		impl->initialized = true;
		return true;
	}

	const size_t n = size_t(impl->n);
	std::vector<float> h_px(n), h_py(n), h_pz(n);
	std::vector<float> h_vx(n), h_vy(n), h_vz(n);
	std::vector<float> h_density(n, 0.0f), h_pressure(n, 0.0f);
	std::vector<float> h_rest(n), h_mass(n);
	std::vector<unsigned char> h_stationary(n);

	for (size_t i = 0; i < n; ++i) {
		const Particle &p = parts[i];
		h_px[i] = float(p.pos.x);
		h_py[i] = float(p.pos.y);
		h_pz[i] = float(p.pos.z);
		h_vx[i] = float(p.speed.x);
		h_vy[i] = float(p.speed.y);
		h_vz[i] = float(p.speed.z);
		h_rest[i] = float(p.rest_density);
		h_mass[i] = float(p.mass);
		h_stationary[i] = p.is_stationary ? 1 : 0;
	}

	auto upload = [&](void **p_device, const void *p_host, size_t p_bytes, const char *p_what) {
		if (!impl->alloc(p_device, p_bytes, p_what)) {
			return false;
		}
		const cudaError_t e = cudaMemcpy(*p_device, p_host, p_bytes, cudaMemcpyHostToDevice);
		if (e != cudaSuccess) {
			impl->fail_cuda(p_what, e);
			return false;
		}
		return true;
	};

	const size_t floats = n * sizeof(float);
	if (!upload((void **)&impl->px, h_px.data(), floats, "cudaMemcpy pos.x")) return false;
	if (!upload((void **)&impl->py, h_py.data(), floats, "cudaMemcpy pos.y")) return false;
	if (!upload((void **)&impl->pz, h_pz.data(), floats, "cudaMemcpy pos.z")) return false;
	if (!upload((void **)&impl->vx, h_vx.data(), floats, "cudaMemcpy vel.x")) return false;
	if (!upload((void **)&impl->vy, h_vy.data(), floats, "cudaMemcpy vel.y")) return false;
	if (!upload((void **)&impl->vz, h_vz.data(), floats, "cudaMemcpy vel.z")) return false;
	if (!upload((void **)&impl->density, h_density.data(), floats, "cudaMemcpy density")) return false;
	if (!upload((void **)&impl->pressure, h_pressure.data(), floats, "cudaMemcpy pressure")) return false;
	if (!upload((void **)&impl->rest_density, h_rest.data(), floats, "cudaMemcpy rest_density")) return false;
	if (!upload((void **)&impl->mass, h_mass.data(), floats, "cudaMemcpy mass")) return false;
	if (!upload((void **)&impl->stationary, h_stationary.data(), n * sizeof(unsigned char), "cudaMemcpy stationary")) return false;

	if (!impl->alloc((void **)&impl->ax, floats, "cudaMalloc acc.x")) return false;
	if (!impl->alloc((void **)&impl->ay, floats, "cudaMalloc acc.y")) return false;
	if (!impl->alloc((void **)&impl->az, floats, "cudaMalloc acc.z")) return false;
	if (!impl->alloc((void **)&impl->nbr, n * kMaxNeighbors * sizeof(int), "cudaMalloc neighbors")) return false;
	if (!impl->alloc((void **)&impl->nbr_count, n * sizeof(int), "cudaMalloc neighbor_count")) return false;

	if (!impl->configure_grid()) {
		impl->release();
		return false;
	}

	impl->initialized = true;

	// Comprobacion real de que los kernels pueden ejecutarse en esta GPU (por
	// ejemplo, que nvcc genero codigo para su compute capability).
	cudaError_t e = cudaGetLastError();
	if (e != cudaSuccess) {
		impl->fail_cuda("inicializacion CUDA", e);
		impl->release();
		return false;
	}
	return true;
}

void CudaSolver::update_conf(const Config &p_conf) {
	const Config old = impl->conf;
	const bool grid_changed = old.h != p_conf.h || old.kernel_support != p_conf.kernel_support ||
			old.area_min.x != p_conf.area_min.x || old.area_min.y != p_conf.area_min.y ||
			old.area_min.z != p_conf.area_min.z || old.area_max.x != p_conf.area_max.x ||
			old.area_max.y != p_conf.area_max.y || old.area_max.z != p_conf.area_max.z;

	impl->conf = p_conf;
	impl->conf.active_particles = impl->active;
	impl->conf.particle_count = impl->n;

	if (impl->initialized && grid_changed) {
		if (!impl->configure_grid()) {
			impl->release();
		}
	}
}

void CudaSolver::simulate_step() {
	simulate_step(nullptr);
}

void CudaSolver::simulate_step(CudaPhaseTimes *p_times) {
	if (!impl->initialized || impl->active <= 0) {
		return;
	}

	const bool timed = p_times != nullptr;
	if (timed && !impl->ensure_events()) {
		return;
	}
	auto mark = [&](int p_index) {
		if (timed) {
			cudaEventRecord(impl->events[p_index]);
		}
	};

	const Params p = impl->params();
	const int n = impl->n;
	const int active = impl->active;
	const int grid_count = impl->grid_max + 1;

	const int blocks_n = (n + kBlockSize - 1) / kBlockSize;
	const int blocks_active = (active + kBlockSize - 1) / kBlockSize;

	mark(0);
	k_reset_acc<<<blocks_active, kBlockSize>>>(impl->ax, impl->ay, impl->az, active, p);
	mark(1);

	k_clear_int<<<(grid_count + kBlockSize - 1) / kBlockSize, kBlockSize>>>(impl->cell_counts, grid_count);

	k_compute_cells<<<blocks_n, kBlockSize>>>(impl->px, impl->py, impl->pz, impl->cell_of, n, p);
	k_count_cells<<<blocks_n, kBlockSize>>>(impl->cell_of, impl->cell_counts, n);

	// Prefijo exclusivo: cell_starts[c] = primer slot de la celda c. El ultimo
	// elemento (grid_max) queda con el total de particulas.
	thrust::exclusive_scan(thrust::device, impl->cell_counts, impl->cell_counts + grid_count, impl->cell_starts);
	k_copy_int<<<(grid_count + kBlockSize - 1) / kBlockSize, kBlockSize>>>(impl->cell_starts, impl->cell_cursors,
			grid_count);

	k_scatter_cells<<<blocks_n, kBlockSize>>>(impl->cell_of, impl->cell_cursors, impl->cell_particles, n);
	mark(2);

	k_query_neighbors<<<blocks_active, kBlockSize>>>(impl->px, impl->py, impl->pz, impl->cell_of, impl->cell_starts,
			impl->cell_particles, impl->nbr, impl->nbr_count, p);
	mark(3);

	k_density_pressure<<<blocks_active, kBlockSize>>>(impl->px, impl->py, impl->pz, impl->mass, impl->rest_density,
			impl->nbr, impl->nbr_count, impl->density, impl->pressure, p);
	mark(4);

	k_pressure_forces<<<blocks_active, kBlockSize>>>(impl->px, impl->py, impl->pz, impl->mass, impl->density,
			impl->pressure, impl->stationary, impl->nbr, impl->nbr_count, impl->ax, impl->ay, impl->az, p);
	mark(5);

	k_viscous_forces<<<blocks_active, kBlockSize>>>(impl->px, impl->py, impl->pz, impl->vx, impl->vy, impl->vz,
			impl->mass, impl->density, impl->stationary, impl->nbr, impl->nbr_count, impl->ax, impl->ay, impl->az, p);
	mark(6);

	k_integrate<<<blocks_active, kBlockSize>>>(impl->px, impl->py, impl->pz, impl->vx, impl->vy, impl->vz, impl->ax,
			impl->ay, impl->az, active, p);
	mark(7);

	k_clamp_bounds<<<blocks_active, kBlockSize>>>(impl->px, impl->py, impl->pz, impl->vx, impl->vy, impl->vz, active,
			float(impl->conf.area_min.x), float(impl->conf.area_min.y), float(impl->conf.area_min.z),
			float(impl->conf.area_max.x), float(impl->conf.area_max.y), float(impl->conf.area_max.z));
	mark(8);

	const cudaError_t e = cudaPeekAtLastError();
	if (e != cudaSuccess) {
		impl->fail_cuda("simulate_step", e);
		return;
	}

	if (!timed) {
		return;
	}

	// Los eventos se leen de golpe al final: sincronizar tras cada etapa
	// falsearia el tiempo (serializaria la cola de lanzamientos).
	if (cudaEventSynchronize(impl->events[8]) != cudaSuccess) {
		impl->fail_cuda("cudaEventSynchronize (perfilado)", cudaGetLastError());
		return;
	}
	auto span = [&](int p_from, int p_to, double &r_dst) {
		float ms = 0.0f;
		if (cudaEventElapsedTime(&ms, impl->events[p_from], impl->events[p_to]) == cudaSuccess) {
			r_dst += double(ms);
		}
	};
	span(0, 1, p_times->other); // reset de aceleraciones + gravedad
	span(1, 2, p_times->grid); // celdas/conteo/prefijo/dispersion
	span(2, 3, p_times->query);
	span(3, 4, p_times->kernel); // W + densidad + presion (fusionado)
	span(4, 5, p_times->pressure); // gradiente + aceleracion por presion
	span(5, 6, p_times->viscosity);
	span(6, 7, p_times->integrate);
	span(7, 8, p_times->bounds);
}

void CudaSolver::synchronize() const {
	Impl *self = const_cast<Impl *>(impl);
	if (!self->initialized) {
		return;
	}
	const cudaError_t e = cudaDeviceSynchronize();
	if (e != cudaSuccess) {
		self->fail_cuda("cudaDeviceSynchronize", e);
		cudaGetLastError(); // limpia el error pegajoso
	}
}

int CudaSolver::copy_positions(std::vector<Vec3> &r_out) const {
	Impl *self = const_cast<Impl *>(impl);
	if (!self->initialized || self->active <= 0) {
		r_out.clear();
		return 0;
	}

	const int active = self->active;
	std::vector<float> hx(size_t(active), 0.0f), hy(size_t(active), 0.0f), hz(size_t(active), 0.0f);
	if (!self->download(self->px, hx.data(), size_t(active), "cudaMemcpy pos.x (host)") ||
			!self->download(self->py, hy.data(), size_t(active), "cudaMemcpy pos.y (host)") ||
			!self->download(self->pz, hz.data(), size_t(active), "cudaMemcpy pos.z (host)")) {
		r_out.clear();
		return 0;
	}

	r_out.resize(size_t(active));
	for (int i = 0; i < active; ++i) {
		r_out[size_t(i)] = Vec3(double(hx[size_t(i)]), double(hy[size_t(i)]), double(hz[size_t(i)]));
	}
	return active;
}

CudaStats CudaSolver::stats() const {
	CudaStats s;
	Impl *self = const_cast<Impl *>(impl);
	s.active = self->active;
	s.total = self->n;
	if (!self->initialized || self->active <= 0) {
		return s;
	}

	const size_t n = size_t(self->active);
	std::vector<float> h_px(n), h_py(n), h_pz(n), h_vx(n), h_vy(n), h_vz(n);
	std::vector<float> h_density(n), h_rest(n);
	std::vector<int> h_nbr_count(n);

	if (!self->download(self->px, h_px.data(), n, "cudaMemcpy stats posx") ||
			!self->download(self->py, h_py.data(), n, "cudaMemcpy stats posy") ||
			!self->download(self->pz, h_pz.data(), n, "cudaMemcpy stats posz") ||
			!self->download(self->vx, h_vx.data(), n, "cudaMemcpy stats velx") ||
			!self->download(self->vy, h_vy.data(), n, "cudaMemcpy stats vely") ||
			!self->download(self->vz, h_vz.data(), n, "cudaMemcpy stats velz") ||
			!self->download(self->density, h_density.data(), n, "cudaMemcpy stats density") ||
			!self->download(self->rest_density, h_rest.data(), n, "cudaMemcpy stats rest") ||
			!self->download_int(self->nbr_count, h_nbr_count.data(), n, "cudaMemcpy stats neighbors")) {
		return s;
	}

	double neighbors = 0.0;
	double checksum = 0.0;
	double speed_sum = 0.0;
	double max_speed = 0.0;
	double density_sum = 0.0;
	int valid_density = 0;
	int undersampled = 0;
	uint64_t hash = 1469598103934665603ull;

	for (size_t i = 0; i < n; ++i) {
		const double px = double(h_px[i]);
		const double py = double(h_py[i]);
		const double pz = double(h_pz[i]);
		const double vx = double(h_vx[i]);
		const double vy = double(h_vy[i]);
		const double vz = double(h_vz[i]);
		const double speed = std::sqrt(vx * vx + vy * vy + vz * vz);

		neighbors += double(h_nbr_count[i]);
		checksum += px + py + pz + speed;
		speed_sum += speed;
		if (speed > max_speed) {
			max_speed = speed;
		}

		const double density = double(h_density[i]);
		const double rest = double(h_rest[i]);
		if (density >= rest) {
			density_sum += density;
			++valid_density;
		} else {
			++undersampled;
		}

		const double values[6] = { px, py, pz, vx, vy, vz };
		for (double value : values) {
			if (!std::isfinite(value)) {
				s.finite = false;
			}
			fnv_mix(hash, value);
		}
	}

	s.avg_neighbors = neighbors / double(n);
	s.avg_density = valid_density > 0 ? density_sum / double(valid_density) : 0.0;
	s.avg_speed = speed_sum / double(n);
	s.cfl = self->conf.h > 0.0 ? max_speed / self->conf.h * self->conf.timestep : 0.0;
	s.undersampled = undersampled;
	s.checksum = checksum;
	s.hash = hash;
	return s;
}

double CudaSolver::average_density() const {
	Impl *self = const_cast<Impl *>(impl);
	if (!self->initialized || self->active <= 0) {
		return 0.0;
	}

	const size_t n = size_t(self->active);
	std::vector<float> h_density(n), h_rest(n);
	if (!self->download(self->density, h_density.data(), n, "cudaMemcpy avg density") ||
			!self->download(self->rest_density, h_rest.data(), n, "cudaMemcpy avg rest")) {
		return 0.0;
	}

	double sum = 0.0;
	int valid = 0;
	for (size_t i = 0; i < n; ++i) {
		if (double(h_density[i]) >= double(h_rest[i])) {
			sum += double(h_density[i]);
			++valid;
		}
	}
	return valid > 0 ? sum / double(valid) : 0.0;
}

int CudaSolver::active_particles() const {
	return impl->active;
}

int CudaSolver::total_particles() const {
	return impl->n;
}

bool CudaSolver::is_initialized() const {
	return impl->initialized;
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

#endif // SPH_HAS_CUDA
