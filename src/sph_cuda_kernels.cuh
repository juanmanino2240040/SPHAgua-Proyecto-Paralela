#ifndef SPH_CUDA_KERNELS_CUH
#define SPH_CUDA_KERNELS_CUH

// Kernels CUDA del solver SPH (port a GPU de sph_kernel.cpp + sph_simulator.cpp).
//
// Este archivo contiene SOLO codigo de dispositivo y no llama a la API runtime de
// CUDA, por eso tambien se puede compilar como C++ normal con un "shim" para
// validar la fisica en CPU (ver sph_cuda_emulation_test.cpp).
//
// Convenciones:
//  - Las posiciones viven en "espacio local del dominio": pos - area_min. Asi el
//    float32 conserva precision aunque el contenedor este lejos del origen.
//  - Las particulas de fluido se REORDENAN cada paso por celda (Z-order lineal).
//    Gracias a eso las celdas (x-1, x, x+1) de una fila son contiguas en memoria y
//    las 27 celdas se recorren como 9 rangos contiguos.
//  - Las particulas de borde son estaticas: su rejilla se construye una sola vez.

namespace sph_gpu {

struct Params {
	// Rejilla
	int nx, ny, nz;
	int n_fluid, n_boundary;
	float inv_cell;
	float lx, ly, lz; // tamano del dominio (area_max - area_min)
	// Kernel de suavizado
	float h, inv_h;
	float support_sq; // (kernel_support * h)^2
	float wcoef; // correction_const * alpha
	float dwcoef; // correction_const * alpha / h^2
	// Fisica
	float k, nu, dt;
	float mass, rest_density;
	float gx, gy, gz;
};

struct XForm { // espacio local del dominio -> espacio local del nodo Godot
	float m[9]; // 3x3 por filas
	float t[3];
};

__device__ __forceinline__ int clampi(int v, int lo, int hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

__device__ __forceinline__ void cell_coords(const Params &P, float x, float y, float z, int &gx, int &gy, int &gz) {
	gx = clampi(int(x * P.inv_cell), 0, P.nx - 1);
	gy = clampi(int(y * P.inv_cell), 0, P.ny - 1);
	gz = clampi(int(z * P.inv_cell), 0, P.nz - 1);
}

// W(q) = wcoef * (max(0,2-q)^3 - 4*max(0,1-q)^3), q = r/h  (spline cubico M4)
__device__ __forceinline__ float eval_W(const Params &P, float r2) {
	const float q = sqrtf(r2) * P.inv_h;
	const float q1 = fmaxf(0.0f, 1.0f - q);
	const float q2 = fmaxf(0.0f, 2.0f - q);
	return P.wcoef * (q2 * q2 * q2 - 4.0f * q1 * q1 * q1);
}

// grad W_ij = (x_i - x_j) * grad_factor
__device__ __forceinline__ float grad_factor(const Params &P, float r2) {
	const float q = sqrtf(r2) * P.inv_h;
	if (q <= 0.001f) {
		return 0.0f; // igual que el solver CPU: evita el underflow con r ~ 0
	}
	const float q1 = fmaxf(0.0f, 1.0f - q);
	const float q2 = fmaxf(0.0f, 2.0f - q);
	const float inner = -3.0f * q2 * q2 + 12.0f * q1 * q1;
	return P.dwcoef * inner / q;
}

// Llama a f(j) para cada particula j de las 27 celdas alrededor de (gx,gy,gz).
// `start` tiene ncell+1 entradas: la celda c ocupa [start[c], start[c+1]).
template <class F>
__device__ __forceinline__ void visit_neighbors(const Params &P, const int *start, int gx, int gy, int gz, F &&f) {
	const int x0 = gx > 0 ? gx - 1 : 0;
	const int x1 = gx < P.nx - 1 ? gx + 1 : P.nx - 1;
	for (int dz = -1; dz <= 1; dz++) {
		const int z = gz + dz;
		if (z < 0 || z >= P.nz) {
			continue;
		}
		for (int dy = -1; dy <= 1; dy++) {
			const int y = gy + dy;
			if (y < 0 || y >= P.ny) {
				continue;
			}
			const int row = (z * P.ny + y) * P.nx;
			const int b = start[row + x0];
			const int e = start[row + x1 + 1];
			for (int j = b; j < e; j++) {
				f(j);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// 1) Clave de celda de cada particula de fluido (+ indice para el sort)
// ---------------------------------------------------------------------------
__global__ void k_cell_keys(Params P, const float4 *pos, unsigned int *keys, unsigned int *idx) {
	const int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i >= P.n_fluid) {
		return;
	}
	int gx, gy, gz;
	cell_coords(P, pos[i].x, pos[i].y, pos[i].z, gx, gy, gz);
	keys[i] = (unsigned int)((gz * P.ny + gy) * P.nx + gx);
	idx[i] = (unsigned int)i;
}

// ---------------------------------------------------------------------------
// 2) start[c] = primer indice con clave >= c  (busqueda binaria por celda)
//    start[ncell] = n  ->  las celdas vacias quedan con rango vacio.
// ---------------------------------------------------------------------------
__global__ void k_cell_start(const unsigned int *keys, int n, int ncell, int *start) {
	const int c = blockIdx.x * blockDim.x + threadIdx.x;
	if (c > ncell) {
		return;
	}
	int lo = 0, hi = n;
	while (lo < hi) {
		const int mid = (lo + hi) >> 1;
		if (keys[mid] < (unsigned int)c) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	start[c] = lo;
}

// ---------------------------------------------------------------------------
// 3) Reordena posicion y velocidad segun el orden de celdas
// ---------------------------------------------------------------------------
__global__ void k_gather(int n, const unsigned int *idx, const float4 *pos_in, const float4 *vel_in,
		float4 *pos_out, float4 *vel_out) {
	const int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i >= n) {
		return;
	}
	const unsigned int src = idx[i];
	pos_out[i] = pos_in[src];
	vel_out[i] = vel_in[src];
}

// ---------------------------------------------------------------------------
// 4) Densidad y presion (primer pase de compute_pressure_forces)
// ---------------------------------------------------------------------------
__global__ void k_density(Params P, const float4 *pos, const int *fstart,
		const float4 *bpos, const int *bstart, float *density, float *pressure) {
	const int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i >= P.n_fluid) {
		return;
	}
	const float4 pi = pos[i];
	int gx, gy, gz;
	cell_coords(P, pi.x, pi.y, pi.z, gx, gy, gz);

	float rho = 0.0f;
	// Fluido (incluye la propia particula, igual que la lista de vecinos de la CPU).
	visit_neighbors(P, fstart, gx, gy, gz, [&](int j) {
		const float dx = pi.x - pos[j].x;
		const float dy = pi.y - pos[j].y;
		const float dz = pi.z - pos[j].z;
		const float r2 = dx * dx + dy * dy + dz * dz;
		if (r2 < P.support_sq) {
			rho += P.mass * eval_W(P, r2);
		}
	});
	// Borde
	visit_neighbors(P, bstart, gx, gy, gz, [&](int j) {
		const float dx = pi.x - bpos[j].x;
		const float dy = pi.y - bpos[j].y;
		const float dz = pi.z - bpos[j].z;
		const float r2 = dx * dx + dy * dy + dz * dz;
		if (r2 < P.support_sq) {
			rho += P.mass * eval_W(P, r2);
		}
	});

	density[i] = rho;
	pressure[i] = fmaxf(P.k * (rho - P.rest_density), 0.0f);
}

// ---------------------------------------------------------------------------
// 5) Fuerzas (gravedad + presion + viscosidad) + Euler-Cromer + limites.
//    Lee del buffer ordenado (pos/vel) y escribe en el buffer de estado
//    (pos_out/vel_out): sin carreras de datos.
// ---------------------------------------------------------------------------
__global__ void k_forces_integrate(Params P, const float4 *pos, const float4 *vel,
		const float *density, const float *pressure, const int *fstart,
		const float4 *bpos, const int *bstart, float4 *pos_out, float4 *vel_out) {
	const int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i >= P.n_fluid) {
		return;
	}
	const float4 pi = pos[i];
	const float4 vi = vel[i];
	const float rho_i = density[i];
	const float pr_i = pressure[i] / (rho_i * rho_i);
	const float eps = 0.01f * P.h * P.h;

	int gx, gy, gz;
	cell_coords(P, pi.x, pi.y, pi.z, gx, gy, gz);

	float ax = P.gx, ay = P.gy, az = P.gz;

	// Vecinos de fluido
	visit_neighbors(P, fstart, gx, gy, gz, [&](int j) {
		const float dx = pi.x - pos[j].x;
		const float dy = pi.y - pos[j].y;
		const float dz = pi.z - pos[j].z;
		const float r2 = dx * dx + dy * dy + dz * dz;
		if (r2 >= P.support_sq) {
			return;
		}
		const float g = grad_factor(P, r2);
		const float dWx = dx * g, dWy = dy * g, dWz = dz * g;
		const float rho_j = density[j];

		// presion
		const float cp = P.mass * (pr_i + pressure[j] / (rho_j * rho_j));
		ax -= cp * dWx;
		ay -= cp * dWy;
		az -= cp * dWz;

		// viscosidad (misma forma componente a componente que sph_simulator.cpp)
		const float cv = 2.0f * P.nu * P.mass / rho_j;
		const float vx = vi.x - vel[j].x;
		const float vy = vi.y - vel[j].y;
		const float vz = vi.z - vel[j].z;
		const float denom = r2 + eps;
		ax += cv * (vx * dx) / denom * dWx;
		ay += cv * (vy * dy) / denom * dWy;
		az += cv * (vz * dz) / denom * dWz;
	});

	// Vecinos de borde (estaticos, velocidad 0)
	visit_neighbors(P, bstart, gx, gy, gz, [&](int j) {
		const float dx = pi.x - bpos[j].x;
		const float dy = pi.y - bpos[j].y;
		const float dz = pi.z - bpos[j].z;
		const float r2 = dx * dx + dy * dy + dz * dz;
		if (r2 >= P.support_sq) {
			return;
		}
		const float g = grad_factor(P, r2);
		const float dWx = dx * g, dWy = dy * g, dWz = dz * g;

		const float cp = 2.0f * P.mass * pr_i;
		ax -= cp * dWx;
		ay -= cp * dWy;
		az -= cp * dWz;

		const float cv = 2.0f * P.nu * P.mass / rho_i;
		const float denom = r2 + eps;
		ax += cv * (vi.x * dx) / denom * dWx;
		ay += cv * (vi.y * dy) / denom * dWy;
		az += cv * (vi.z * dz) / denom * dWz;
	});

	// Euler-Cromer
	float vx = vi.x + ax * P.dt;
	float vy = vi.y + ay * P.dt;
	float vz = vi.z + az * P.dt;
	float px = pi.x + vx * P.dt;
	float py = pi.y + vy * P.dt;
	float pz = pi.z + vz * P.dt;

	// Limites del dominio (equivale a check_bounding_boxes con remove_escaped=false)
	if (px < 0.0f) { px = 0.0f; if (vx < 0.0f) vx = 0.0f; }
	else if (px > P.lx) { px = P.lx; if (vx > 0.0f) vx = 0.0f; }
	if (py < 0.0f) { py = 0.0f; if (vy < 0.0f) vy = 0.0f; }
	else if (py > P.ly) { py = P.ly; if (vy > 0.0f) vy = 0.0f; }
	if (pz < 0.0f) { pz = 0.0f; if (vz < 0.0f) vz = 0.0f; }
	else if (pz > P.lz) { pz = P.lz; if (vz > 0.0f) vz = 0.0f; }

	pos_out[i] = make_float4(px, py, pz, 0.0f);
	vel_out[i] = make_float4(vx, vy, vz, 0.0f);
}

// ---------------------------------------------------------------------------
// 6) Buffer del MultiMesh (TRANSFORM_3D = 12 floats por instancia, base identidad)
//    out = 3 float4 por particula: [1 0 0 ox][0 1 0 oy][0 0 1 oz]
// ---------------------------------------------------------------------------
__global__ void k_multimesh(int n, const float4 *pos, XForm xf, float4 *out) {
	const int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i >= n) {
		return;
	}
	const float4 p = pos[i];
	const float ox = xf.m[0] * p.x + xf.m[1] * p.y + xf.m[2] * p.z + xf.t[0];
	const float oy = xf.m[3] * p.x + xf.m[4] * p.y + xf.m[5] * p.z + xf.t[1];
	const float oz = xf.m[6] * p.x + xf.m[7] * p.y + xf.m[8] * p.z + xf.t[2];
	out[3 * i + 0] = make_float4(1.0f, 0.0f, 0.0f, ox);
	out[3 * i + 1] = make_float4(0.0f, 1.0f, 0.0f, oy);
	out[3 * i + 2] = make_float4(0.0f, 0.0f, 1.0f, oz);
}

} // namespace sph_gpu

#endif // SPH_CUDA_KERNELS_CUH
