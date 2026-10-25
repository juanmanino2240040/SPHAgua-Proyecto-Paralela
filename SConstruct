import os

# Versión de la API de Godot que usa el proyecto (ver project.godot -> config/features).
# Se puede sobreescribir en la línea de comandos con: scons api_version=4.7
api_version = ARGUMENTS.get("api_version", "4.7")

env = SConscript("godot-cpp/SConstruct", exports="api_version")

# Directorio de código fuente
env.Append(CPPPATH=["src/c_code/", "godot-cpp/include", "godot-cpp/gen/include"])

# Archivos fuente de nuestra extensión
sources = [
    "src/c_code/sph_kernel.cpp",
    "src/c_code/sph_simulator.cpp",
    "src/c_code/sph_fluid_region.cpp",
    "src/c_code/register_types.cpp",
]

# Compilar como librería compartida (.dll / .so / .dylib)
library = env.SharedLibrary("bin/sph_fluid_extension", sources)

Default(library)
