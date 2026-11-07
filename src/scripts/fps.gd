extends Label

var frame_times: Array = []
const MAX_SAMPLES: int = 60 # Promedio de los últimos 60 fotogramas para estabilizar la lectura

func _ready() -> void:
	# Configuración automática de posición y estilo visual
	offset_left = 16
	offset_top = 16
	z_index = 4096
	add_theme_font_size_override("font_size", 22)
	add_theme_color_override("font_color", Color(0, 1, 0.5, 1)) # Verde agua brillante

func _process(delta: float) -> void:
	# 1. Obtener FPS actuales
	var fps: int = Engine.get_frames_per_second()
	
	# 2. Calcular Frametime en milisegundos
	var current_ms: float = delta * 1000.0
	
	# Añadir a la lista para calcular la media móvil
	frame_times.append(current_ms)
	if frame_times.size() > MAX_SAMPLES:
		frame_times.pop_front()
	
	var sum: float = 0.0
	for t in frame_times:
		sum += t
	var avg_ms: float = sum / frame_times.size()
	
	# 3. Actualizar el texto en pantalla con ambos valores
	text = "FPS: %d | Frame: %.2f ms (Avg: %.2f ms)" % [fps, current_ms, avg_ms]
