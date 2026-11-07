# Benchmark SPH desde terminal (sin Godot).
#
#   make              -> build/serial/sph_bench   (secuencial, linea base)
#   make OMP=1        -> build/omp/sph_bench      (compilado con -fopenmp)
#   make CUDA=1       -> build/cuda/sph_bench     (backend CUDA via nvcc)
#   make run          -> corrida de ejemplo
#   make verify       -> comprueba la ruta cronometrada contra Simulator::simulate_step()
#   make sweep        -> barrido de tamanos/hilos a results/*.csv (ver run_sweep.sh)
#   make clean
#
# Variables utiles:
#   CXX=clang++   OPT="-O3 -march=native"   SRC_DIR=<ruta>  (se autodetecta)
#   NVCC=/opt/cuda/bin/nvcc   CUDA_ARCH=sm_75   (o sm_50 si la GPU es Maxwell y
#                                               usas CUDA 12.x; CUDA 13 ya no
#                                               genera codigo para sm_50)
#
# En ejecucion: ./build/cuda/sph_bench --backend cuda --particles 8000

CXX     ?= g++
OPT     ?= -O2
OMP     ?= 0
CUDA    ?= 0
NVCC    ?= nvcc
CUDA_ARCH ?= sm_75
# Raiz de CUDA deducida de nvcc (sirve para -L.../lib64). Se puede sobrescribir.
CUDA_HOME ?= $(shell dirname $$(dirname $$(readlink -f $$(command -v $(NVCC)))))

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

ifeq ($(CUDA),1)
  ifeq ($(shell command -v $(NVCC)),)
    $(error CUDA=1 pero no se encontro '$(NVCC)'; instala CUDA 12.x o pasa NVCC=/ruta/a/nvcc)
  endif
  ifeq ($(OMP),1)
    VARIANT := cuda_omp
  else
    VARIANT := cuda
  endif
  CXXFLAGS += -DSPH_HAS_CUDA
  NVCCFLAGS += $(OPT) -std=c++17 -arch=$(CUDA_ARCH) -Xcompiler -fPIC -DSPH_HAS_CUDA -I$(SRC_DIR) -I.
  # cudart estatica: el binario no necesita libcudart.so al ejecutarse.
  LDFLAGS += -L$(CUDA_HOME)/lib64 -lcudart_static -lrt -lpthread -ldl
else ifeq ($(OMP),1)
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

ifeq ($(CUDA),1)
  CUDA_OBJS := $(BUILD)/backend_cuda.cuda.o $(BUILD)/cuda_sph_cuda.o
  OBJS      += $(CUDA_OBJS)
endif

.PHONY: all run run-cuda verify sweep cuda clean
all: $(BIN)

cuda:
	$(MAKE) CUDA=1

$(BIN): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD)/%.o: %.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/core_%.o: $(SRC_DIR)/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

ifeq ($(CUDA),1)
$(BUILD)/backend_cuda.cuda.o: backend_cuda.cu | $(BUILD)
	$(NVCC) $(NVCCFLAGS) -c $< -o $@

$(BUILD)/cuda_sph_cuda.o: $(SRC_DIR)/sph_cuda.cu | $(BUILD)
	$(NVCC) $(NVCCFLAGS) -c $< -o $@
endif

$(BUILD):
	mkdir -p $@

run: $(BIN)
	$(BIN) --particles 1500 --steps 300

run-cuda: cuda
	$(BUILD)/sph_bench --backend cuda --particles 1500 --steps 300

verify: $(BIN)
	$(BIN) --verify --particles 1500

sweep: $(BIN)
	BIN=$(BIN) ./run_sweep.sh

clean:
	rm -rf build

-include $(OBJS:.o=.d)
