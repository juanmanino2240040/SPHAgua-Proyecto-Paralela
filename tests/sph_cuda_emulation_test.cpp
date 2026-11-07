// Validacion de la fisica GPU SIN GPU: compila sph_cuda_kernels.cuh como C++ normal
// (emulando blockIdx/threadIdx con bucles) y lo compara particula a particula con el
// solver CPU original (sph_kernel.cpp + sph_simulator.cpp, en doble precision).
//
//   (desde la carpeta src/)  g++ -O2 -std=c++17 -I. tests/sph_cuda_emulation_test.cpp sph_kernel.cpp \
//       sph_simulator.cpp -o emu_test && ./emu_test
//
// Ojo: solo valida los kernels y el orden del pipeline, NO cudaMalloc/CUB/lanzamientos.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <unordered_set>
#include <vector>

// ---------------- shim minimo de CUDA ----------------
#define __global__
#define __device__
#define __host__
#define __forceinline__ inline
struct float4 { float x, y, z, w; };
static inline float4 make_float4(float x, float y, float z, float w) { return { x, y, z, w }; }
struct dim3_ { unsigned x = 0; };
static dim3_ blockIdx, threadIdx, blockDim;
template <class K, class... A>
static void launch(int blocks, int threads, K kernel, A... args) {
	blockDim.x = threads;
	for (int b = 0; b < blocks; b++) {
		blockIdx.x = b;
		for (int t = 0; t < threads; t++) {
			threadIdx.x = t;
			kernel(args...);
		}
	}
}
#include "sph_cuda_kernels.cuh"

#include "sph_kernel.h"
#include "sph_particle.h"
#include "sph_simulator.h"

using namespace sph;

// ---------------- escena (misma logica que build_particles) ----------------
static void build_scene(std::vector<Particle> &parts, Config &conf, double bmin[3], double bmax[3],
		int target, double fill, double rest_density, double k, double nu) {
	const double size[3] = { bmax[0] - bmin[0], bmax[1] - bmin[1], bmax[2] - bmin[2] };
	const double fluid_h = size[1] * fill;
	const double spacing = std::cbrt(size[0] * size[2] * fluid_h / double(target));
	const double mass = rest_density * spacing * spacing * spacing;
	int nx = std::max(1, int(size[0] / spacing) - 1);
	int nz = std::max(1, int(size[2] / spacing) - 1);
	int ny = std::max(1, int(fluid_h / spacing) - 1);
	const double x0 = bmin[0] + 0.5 * (size[0] - (nx - 1) * spacing);
	const double z0 = bmin[2] + 0.5 * (size[2] - (nz - 1) * spacing);
	const double y0 = bmin[1] + 2.0 * spacing;
	for (int ix = 0; ix < nx; ++ix)
		for (int iy = 0; iy < ny; ++iy)
			for (int iz = 0; iz < nz; ++iz) {
				Particle p;
				p.mass = mass;
				p.rest_density = rest_density;
				p.pos = Vec3(x0 + ix * spacing, y0 + iy * spacing, z0 + iz * spacing);
				parts.push_back(p);
			}
	std::unordered_set<long long> seen;
	auto add = [&](double px, double py, double pz) {
		long long a = std::llround((px - bmin[0]) / spacing), b = std::llround((py - bmin[1]) / spacing),
				  c = std::llround((pz - bmin[2]) / spacing);
		if (!seen.insert((a << 42) ^ (b << 21) ^ c).second) return;
		Particle p;
		p.is_stationary = true;
		p.mass = mass;
		p.rest_density = rest_density;
		p.pos = Vec3(px, py, pz);
		parts.push_back(p);
	};
	const double eps = spacing * 1e-4;
	for (int l = 0; l < 2; ++l) {
		const double yf = bmin[1] + (l + 0.5) * spacing, yc = bmax[1] - (l + 0.5) * spacing;
		for (double x = bmin[0] + 0.5 * spacing; x <= bmax[0] - 0.5 * spacing + eps; x += spacing)
			for (double z = bmin[2] + 0.5 * spacing; z <= bmax[2] - 0.5 * spacing + eps; z += spacing) {
				add(x, yf, z);
				add(x, yc, z);
			}
		const double xw0 = bmin[0] + (l + 0.5) * spacing, xw1 = bmax[0] - (l + 0.5) * spacing;
		for (double y = bmin[1] + 0.5 * spacing; y <= bmax[1] - 0.5 * spacing + eps; y += spacing)
			for (double z = bmin[2] + 0.5 * spacing; z <= bmax[2] - 0.5 * spacing + eps; z += spacing) {
				add(xw0, y, z);
				add(xw1, y, z);
			}
		const double zw0 = bmin[2] + (l + 0.5) * spacing, zw1 = bmax[2] - (l + 0.5) * spacing;
		for (double y = bmin[1] + 0.5 * spacing; y <= bmax[1] - 0.5 * spacing + eps; y += spacing)
			for (double x = bmin[0] + 0.5 * spacing; x <= bmax[0] - 0.5 * spacing + eps; x += spacing) {
				add(x, y, zw0);
				add(x, y, zw1);
			}
	}
	conf.particle_count = int(parts.size());
	conf.dim = 3;
	conf.h = spacing;
	conf.k = k;
	conf.nu = nu;
	conf.kernel_support = 2.0;
	conf.timestep = 0.0005;
	conf.gravity = Vec3(0, -9.81, 0);
	conf.area_min = Vec3(bmin[0], bmin[1], bmin[2]);
	conf.area_max = Vec3(bmax[0], bmax[1], bmax[2]);
}

int main() {
	double bmin[3] = { 3.0, 10.0, -4.0 }, bmax[3] = { 4.0, 11.0, -3.0 }; // lejos del origen a proposito
	std::vector<Particle> parts;
	Config conf;
	build_scene(parts, conf, bmin, bmax, 1500, 0.5, 1000.0, 5000.0, 0.2);

	// ---- CPU de referencia ----
	Simulator sim;
	Kernel kernel;
	sim.initialize(conf, parts);
	kernel = Kernel(sim.get_conf());
	kernel.initialize(sim.get_particle_data());
	sim.set_kernel(&kernel);
	const int nf = sim.get_conf().active_particles;
	const int nb = sim.get_conf().particle_count - nf;
	std::vector<Particle> init = sim.get_particles(); // estado inicial (ya particionado)

	// ---- GPU emulada: parametros identicos a CudaSolver::initialize ----
	const double cell = conf.h * conf.kernel_support;
	const double alpha = 1.0 / (4.0 * M_PI * conf.h * conf.h * conf.h);
	const double corr = 0.999139;
	sph_gpu::Params P{};
	P.nx = std::max(1, int((bmax[0] - bmin[0]) / cell) + 1);
	P.ny = std::max(1, int((bmax[1] - bmin[1]) / cell) + 1);
	P.nz = std::max(1, int((bmax[2] - bmin[2]) / cell) + 1);
	P.n_fluid = nf;
	P.n_boundary = nb;
	P.inv_cell = float(1.0 / cell);
	P.lx = float(bmax[0] - bmin[0]); P.ly = float(bmax[1] - bmin[1]); P.lz = float(bmax[2] - bmin[2]);
	P.h = float(conf.h);
	P.inv_h = float(1.0 / conf.h);
	P.support_sq = float(cell * cell);
	P.wcoef = float(corr * alpha);
	P.dwcoef = float(corr * alpha / (conf.h * conf.h));
	P.k = float(conf.k); P.nu = float(conf.nu); P.dt = float(conf.timestep);
	P.mass = float(init[0].mass); P.rest_density = float(init[0].rest_density);
	P.gx = 0; P.gy = -9.81f; P.gz = 0;
	const int ncell = P.nx * P.ny * P.nz;

	std::vector<float4> pos_a(nf), vel_a(nf, make_float4(0, 0, 0, 0)), pos_b(nf), vel_b(nf);
	std::vector<int> ident(nf); // solo para el test: identidad original de cada hueco
	std::iota(ident.begin(), ident.end(), 0);
	std::vector<int> ident_b(nf);
	for (int i = 0; i < nf; i++)
		pos_a[i] = make_float4(float(init[i].pos.x - bmin[0]), float(init[i].pos.y - bmin[1]), float(init[i].pos.z - bmin[2]), 0);

	// borde: rejilla estatica (misma construccion que en el host)
	std::vector<float4> bpos(std::max(1, nb));
	std::vector<int> bstart(ncell + 1, 0);
	{
		std::vector<float4> loc(nb);
		std::vector<int> cid(nb);
		for (int i = 0; i < nb; i++) {
			const Vec3 &q = init[nf + i].pos;
			loc[i] = make_float4(float(q.x - bmin[0]), float(q.y - bmin[1]), float(q.z - bmin[2]), 0);
			int gx, gy, gz;
			sph_gpu::cell_coords(P, loc[i].x, loc[i].y, loc[i].z, gx, gy, gz);
			cid[i] = (gz * P.ny + gy) * P.nx + gx;
			bstart[cid[i] + 1]++;
		}
		for (int i = 0; i < ncell; i++) bstart[i + 1] += bstart[i];
		std::vector<int> cur(bstart.begin(), bstart.end() - 1);
		for (int i = 0; i < nb; i++) bpos[cur[cid[i]]++] = loc[i];
	}

	std::vector<unsigned> keys(nf), idx(nf), keys_s(nf), idx_s(nf);
	std::vector<int> fstart(ncell + 1);
	std::vector<float> density(nf), pressure(nf);
	const int B = 128, gb = (nf + B - 1) / B;

	auto gpu_step = [&]() {
		launch(gb, B, sph_gpu::k_cell_keys, P, pos_a.data(), keys.data(), idx.data());
		std::vector<unsigned> order(nf);
		std::iota(order.begin(), order.end(), 0u);
		std::stable_sort(order.begin(), order.end(), [&](unsigned a, unsigned b) { return keys[a] < keys[b]; });
		for (int i = 0; i < nf; i++) { keys_s[i] = keys[order[i]]; idx_s[i] = idx[order[i]]; }
		launch((ncell + 1 + B - 1) / B, B, sph_gpu::k_cell_start, keys_s.data(), nf, ncell, fstart.data());
		launch(gb, B, sph_gpu::k_gather, nf, idx_s.data(), pos_a.data(), vel_a.data(), pos_b.data(), vel_b.data());
		for (int i = 0; i < nf; i++) ident_b[i] = ident[idx_s[i]];
		launch(gb, B, sph_gpu::k_density, P, pos_b.data(), fstart.data(), bpos.data(), bstart.data(), density.data(), pressure.data());
		launch(gb, B, sph_gpu::k_forces_integrate, P, pos_b.data(), vel_b.data(), density.data(), pressure.data(),
				fstart.data(), bpos.data(), bstart.data(), pos_a.data(), vel_a.data());
		ident = ident_b; // pos_a conserva el orden de pos_b
	};

	std::printf("fluido=%d borde=%d h=%.4f celdas=%dx%dx%d\n", nf, nb, conf.h, P.nx, P.ny, P.nz);
	std::printf("%6s %14s %14s %16s %16s\n", "paso", "rho_medio CPU", "rho_medio GPU", "err_pos max / h", "err_pos rms / h");

	int step = 0;
	for (int target : { 1, 10, 50, 100, 200 }) {
		while (step < target) {
			sim.simulate_step();
			gpu_step();
			step++;
		}
		double emax = 0, esum = 0;
		for (int i = 0; i < nf; i++) {
			const Vec3 &c = sim.get_particles()[ident[i]].pos;
			const double dx = c.x - bmin[0] - pos_a[i].x, dy = c.y - bmin[1] - pos_a[i].y, dz = c.z - bmin[2] - pos_a[i].z;
			const double e = std::sqrt(dx * dx + dy * dy + dz * dz) / conf.h;
			emax = std::max(emax, e);
			esum += e * e;
		}
		double rho_gpu = 0; int valid = 0;
		for (float d : density) if (d >= P.rest_density) { rho_gpu += d; valid++; }
		rho_gpu = valid ? rho_gpu / valid : 0;
		std::printf("%6d %14.4f %14.4f %16.3e %16.3e\n", step, sim.get_average_density(), rho_gpu, emax, std::sqrt(esum / nf));
	}
	return 0;
}
