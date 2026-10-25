extends MultiMeshInstance3D

func _ready():
	var mm = multimesh
	if not mm: return
	
	for i in range(mm.get_instance_count()):
		var t = Transform3D()
		
		# Ajustamos los rangos para que queden estrictamente dentro del cubo:
		# X y Z: Ancho y profundidad (ej. de -0.4 a 0.4)
		# Y: Altura dentro de la caja (ej. de 0.0 a 0.8 para que nazcan desde el suelo o la mitad)
		t.origin = Vector3(
			randf_range(-0.4, 0.4),
			randf_range(0.0, 0.8),
			randf_range(-0.4, 0.4)
		)
		
		mm.set_instance_transform(i, t)
