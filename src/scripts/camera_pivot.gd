extends Node3D

@export var sensitivity: float = 0.005
@export var tilt_lower_limit: float = deg_to_rad(-60.0)
@export var tilt_upper_limit: float = deg_to_rad(60.0)

@onready var camera_3d: Camera3D = $Camera3D # Ajusta la ruta si no usas SpringArm3D

func _ready() -> void:
	# Opcional: Bloquea y oculta el cursor del ratón (estilo juego en 3D)
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion:
		# Rotación horizontal (en el eje Y del pivote)
		rotate_y(-event.relative.x * sensitivity)
		
		# Rotación vertical (en el eje X del SpringArm o Cámara)
		camera_3d.rotate_x(-event.relative.y * sensitivity)
		camera_3d.rotation.x = clamp(camera_3d.rotation.x, tilt_lower_limit, tilt_upper_limit)
@export var move_speed: float = 10.0

func _process(delta: float) -> void:
	var input_dir := Vector3.ZERO
	if Input.is_key_pressed(KEY_W):
		input_dir.z -= 1
	if Input.is_key_pressed(KEY_S):
		input_dir.z += 1
	if Input.is_key_pressed(KEY_A):
		input_dir.x -= 1
	if Input.is_key_pressed(KEY_D):
		input_dir.x += 1
		
	input_dir = input_dir.normalized()
	# Transforma la dirección local de la cámara al espacio global
	translate(global_transform.basis * input_dir * move_speed * delta)
