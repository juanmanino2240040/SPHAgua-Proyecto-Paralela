extends Camera3D

@export_category("Configuración")
@export var velocidad_movimiento: float = 10.0
@export var velocidad_multiplicada: float = 25.0 # Al mantener Shift
@export var sensibilidad_raton: float = 0.002

var rotacion_x: float = 0.0
var rotacion_y: float = 0.0

func _ready() -> void:
	# Captura el ratón para poder girar la cámara infinitamente
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

func _unhandled_input(event: InputEvent) -> void:
	# Control de rotación con el movimiento del ratón
	if event is InputEventMouseMotion:
		rotacion_x -= event.relative.x * sensibilidad_raton
		rotacion_y -= event.relative.y * sensibilidad_raton
		rotacion_y = clamp(rotacion_y, deg_to_rad(-89), deg_to_rad(89)) # Evita que se voltee de cabeza
		
		transform.basis = Basis() # Reinicia base
		rotate_object_local(Vector3.UP, rotacion_x)
		rotate_object_local(Vector3.RIGHT, rotacion_y)

	# Permitir liberar el ratón con la tecla ESCAPE
	if event.is_action_pressed("ui_cancel"):
		if Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
			Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
		else:
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

func _process(delta: float) -> void:
	if Input.mouse_mode != Input.MOUSE_MODE_CAPTURED:
		return # No mover si el ratón no está capturado

	var direccion := Vector3.ZERO
	
	# Determinar velocidad actual (Normal o Rápida con Shift)
	var velocidad_actual = velocidad_movimiento
	if Input.is_key_pressed(KEY_SHIFT):
		velocidad_actual = velocidad_multiplicada

	# Movimiento horizontal (W, A, S, D)
	if Input.is_key_pressed(KEY_W):
		direccion -= global_transform.basis.z
	if Input.is_key_pressed(KEY_S):
		direccion += global_transform.basis.z
	if Input.is_key_pressed(KEY_A):
		direccion -= global_transform.basis.x
	if Input.is_key_pressed(KEY_D):
		direccion += global_transform.basis.x

	# SUBIR Y BAJAR (Espacio para subir, Q o Control para bajar)
	if Input.is_key_pressed(KEY_SPACE):
		direccion += Vector3.UP
	if Input.is_key_pressed(KEY_Q) or Input.is_key_pressed(KEY_CTRL):
		direccion += Vector3.DOWN

	# Aplicar el movimiento de forma fluida
	if direccion != Vector3.ZERO:
		direccion = direccion.normalized()
		global_position += direccion * velocidad_actual * delta
