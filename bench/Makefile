# Benchmark SPH desde terminal (sin Godot).
#
#   make              -> build/serial/sph_bench   (secuencial, linea base)
#   make OMP=1        -> build/omp/sph_bench      (compilado con -fopenmp)
#   make run          -> corrida de ejemplo
#   make verify       -> comprueba la ruta cronometrada contra Simulator::simulate_step()
#   make sweep        -> barrido de tamanos/hilos a results/*.csv (ver run_sweep.sh)
#   make clean
#
# Variables utiles:  CXX=clang++   OPT="-O3 -march=native"   SRC_DIR=<ruta>  (se autodetecta)

CXX     ?= g++
OPT     ?= -O2
OMP     ?= 0
# Detecta donde estan los fuentes del solver:
#   bench/ dentro de src/c_code/  -> SRC_DIR=..
#   bench/ en la raiz del repo    -> SRC_DIR=../src/c_code
ifneq ($(wildcard ../sph_kernel.cpp),)
  SRC_DIR ?= ..
else
  SRC_DIR ?= ../src/c_code
endif

CXXFLAGS += $(OPT) -std=c++17 -Wall -Wextra -I$(SRC_DIR) -I. -MMD -MP
LDFLAGS  +=

ifeq ($(OMP),1)
  VARIANT   := omp
  CXXFLAGS  += -fopenmp
  LDFLAGS   += -fopenmp
else
  VARIANT   := serial
endif

BUILD := build/$(VARIANT)
BIN   := $(BUILD)/sph_bench

# Fuentes del benchmark + nucleo del solver (sin Godot).
BENCH_SRCS := bench.cpp scene_builder.cpp backend_cpu.cpp
CORE_SRCS  := $(SRC_DIR)/sph_kernel.cpp $(SRC_DIR)/sph_simulator.cpp
OBJS := $(addprefix $(BUILD)/,$(BENCH_SRCS:.cpp=.o)) \
        $(addprefix $(BUILD)/core_,$(notdir $(CORE_SRCS:.cpp=.o)))

.PHONY: all run verify sweep clean
all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD)/%.o: %.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/core_%.o: $(SRC_DIR)/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $@

run: $(BIN)
	$(BIN) --particles 1500 --steps 300

verify: $(BIN)
	$(BIN) --verify --particles 1500

sweep: $(BIN)
	BIN=$(BIN) ./run_sweep.sh

clean:
	rm -rf build

-include $(OBJS:.o=.d)
