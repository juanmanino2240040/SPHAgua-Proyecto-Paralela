#!/usr/bin/env bash
# Barrido de tamanos (y de hilos si el binario es OpenMP) -> un CSV.
#
#   ./run_sweep.sh                                   # usa build/serial/sph_bench
#   BIN=build/omp/sph_bench ./run_sweep.sh           # barre hilos 1,2,4..nproc
#   OUT=results/todo.csv BIN=build/serial/sph_bench ./run_sweep.sh
#   OUT=results/todo.csv BIN=build/omp/sph_bench   ./run_sweep.sh   # mismo CSV => speedup vs serie
#
# Variables: SIZES, THREADS, REPEAT, WARMUP, STEPS, OUT, EXTRA (args extra para sph_bench).
set -uo pipefail
cd "$(dirname "$0")"

BIN=${BIN:-build/serial/sph_bench}
SIZES=${SIZES:-"1500 4000 8000 16000"}
REPEAT=${REPEAT:-3}
WARMUP=${WARMUP:-20}
OUT=${OUT:-results/sweep_$(date +%Y%m%d_%H%M%S).csv}
EXTRA=${EXTRA:-}

if [ ! -x "$BIN" ]; then
	echo "No existe $BIN: compila antes con 'make' o 'make OMP=1'" >&2
	exit 1
fi

OMP=$("$BIN" --info | awk '$1=="omp"{print $2}')
if [ "$OMP" != "1" ]; then
	THREADS="1" # binario secuencial: --threads no aplica
elif [ -z "${THREADS:-}" ]; then
	max=$(nproc)
	THREADS=""
	t=1
	while [ "$t" -lt "$max" ]; do
		THREADS="$THREADS $t"
		t=$((t * 2))
	done
	THREADS="$THREADS $max"
fi

mkdir -p "$(dirname "$OUT")"

for n in $SIZES; do
	# Trabajo total aprox. constante: pocos pasos si hay muchas particulas.
	steps=${STEPS:-$((2000000 / n))}
	[ "$steps" -lt 20 ] && steps=20
	[ "$steps" -gt 400 ] && steps=400
	for t in $THREADS; do
		echo ">> N=$n hilos=$t pasos=$steps repeticiones=$REPEAT ($BIN)"
		"$BIN" --particles "$n" --steps "$steps" --warmup "$WARMUP" --repeat "$REPEAT" \
			--threads "$t" --quiet --csv "$OUT" $EXTRA
		rc=$?
		[ $rc -ne 0 ] && echo "   (sph_bench termino con codigo $rc; revisa los avisos)" >&2
	done
done

echo "CSV: $OUT"
echo "Resumen: python3 summarize.py $OUT"
exit 0
