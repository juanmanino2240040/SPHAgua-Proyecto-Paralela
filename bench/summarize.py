#!/usr/bin/env python3
"""Resume uno o varios CSV de sph_bench.

Por cada (backend, particulas objetivo) toma como base la corrida secuencial
(omp=0) o, si no existe, la de menos hilos, y calcula para el resto:
  speedup    = t_base / t
  eficiencia = speedup / hilos
  verificacion: hash identico a la base, o diferencia relativa del checksum.

Uso:  python3 summarize.py results/*.csv [--phases]
Solo usa la biblioteca estandar.
"""
import argparse
import csv
import statistics
import sys
from collections import defaultdict

PHASES = [
    ("grid", "rejilla"),
    ("query", "consulta"),
    ("kernel", "kernel"),
    ("kernel_deriv", "gradiente"),
    ("pressure", "presion"),
    ("viscosity", "viscosidad"),
    ("integrate", "integracion"),
    ("bounds", "limites"),
    ("other", "otros"),
]


def med(rows, col):
    return statistics.median(float(r[col]) for r in rows)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", nargs="+", help="CSV generados por sph_bench --csv / run_sweep.sh")
    ap.add_argument("--phases", action="store_true", help="muestra tambien el %% de tiempo por fase")
    args = ap.parse_args()

    rows = []
    for path in args.csv:
        with open(path, newline="") as f:
            rows += list(csv.DictReader(f))
    if not rows:
        sys.exit("Los CSV no tienen filas.")

    # (backend, N) -> {(omp, hilos): [filas de repeticiones]}
    groups = defaultdict(lambda: defaultdict(list))
    for r in rows:
        groups[(r["backend"], int(r["particles_target"]))][(int(r["omp"]), int(r["threads"]))].append(r)

    head = f"{'backend':7} {'N obj.':>7} {'fluido':>7} {'build':>6} {'hilos':>5} {'ms/paso':>9} {'min':>9} {'speedup':>8} {'efic.':>6} {'us/part.':>9}  verificacion"
    print(head)
    print("-" * len(head))
    unstable = False
    for (backend, n) in sorted(groups):
        variants = groups[(backend, n)]
        base_key = (0, 1) if (0, 1) in variants else min(variants, key=lambda k: (k[1], k[0]))
        base = variants[base_key]
        base_t = med(base, "ms_step")
        base_hash = base[0]["hash"]
        base_chk = float(base[0]["checksum"])
        for key in sorted(variants):
            rs = variants[key]
            omp, thr = key
            t = med(rs, "ms_step")
            tmin = min(float(r["ms_step"]) for r in rs)
            speedup = base_t / t
            fluid = int(rs[0]["fluid"])
            if rs[0]["hash"] == base_hash:
                check = "= base (bit a bit)"
            else:
                rel = abs(float(rs[0]["checksum"]) - base_chk) / max(abs(base_chk), 1e-300)
                check = f"checksum dif. rel {rel:.1e}"
            if any(r["finite"] != "1" for r in rs):
                check += "  [NaN/inf!]"
                unstable = True
            # En GPU no hay "hilos", asi que la eficiencia no aplica.
            eff = f"{100 * speedup / thr:5.0f}%" if thr > 0 else "  gpu"
            print(f"{backend:7} {n:7d} {fluid:7d} {'omp' if omp else 'serie':>6} {thr:5d} {t:9.3f} {tmin:9.3f} {speedup:8.2f} {eff:>6} {1000 * t / fluid:9.3f}  {check}")
            if args.phases:
                tot = sum(med(rs, f"ms_{c}") for c, _ in PHASES if f"ms_{c}" in rs[0])
                parts = "  ".join(f"{label} {100 * med(rs, f'ms_{c}') / tot:4.1f}%" for c, label in PHASES[:6] if f'ms_{c}' in rs[0])
                print(f"{'':32}fases: {parts}")

    print()
    print(f"Base de speedup: corrida secuencial (omp=0) si existe; mediana de las repeticiones. Filas leidas: {len(rows)}.")
    if unstable:
        print("AVISO: hay corridas con NaN/inf; sus tiempos no son validos.", file=sys.stderr)


if __name__ == "__main__":
    main()
