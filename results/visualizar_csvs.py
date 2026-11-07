import os
import glob
import matplotlib.pyplot as plt
import pandas as pd

# Ruta de la carpeta
folder_path = "/home/giorgio/Descargas/results"

# Encontrar todos los archivos CSV
csv_files = glob.glob(os.path.join(folder_path, "*.csv"))

print(f"Archivos CSV encontrados: {len(csv_files)}\n")

for csv_file in csv_files:
    print(f"Procesando: {os.path.basename(csv_file)}")
    
    # Leer el CSV
    df = pd.read_csv(csv_file)
    
    # Generar nombre de imagen basado en el archivo CSV
    image_path = os.path.join(folder_path, f"{os.path.splitext(os.path.basename(csv_file))[0]}.png")
    
    # Intentar generar un gráfico simple
    try:
        # Si el DataFrame tiene columnas numéricas, graficar las primeras dos
        numeric_cols = df.select_dtypes(include=['number']).columns
        
        if len(numeric_cols) >= 2:
            plt.figure(figsize=(10, 6))
            col_list = list(numeric_cols)
            plt.plot(df[col_list[0]], label=col_list[0], marker='o')
            if len(col_list) >= 3:
                plt.plot(df[col_list[1]], label=col_list[1], marker='x')
            plt.title(f'Gráfico de {os.path.basename(csv_file)}')
            plt.xlabel('Índice')
            plt.ylabel('Valor')
            plt.legend()
            plt.grid(True)
            plt.tight_layout()
            plt.savefig(image_path)
            plt.close()
            print(f"  ✓ Gráfico guardado: {image_path}")
        else:
            # Si no hay columnas numéricas, mostrar información de las columnas
            print(f"  - No hay suficientes columnas numéricas para graficar en {os.path.basename(csv_file)}")
            # Guardar una imagen con el nombre de las columnas
            plt.figure(figsize=(10, 6))
            plt.text(0.5, 0.5, f'Archivo: {os.path.basename(csv_file)}\nColumnas: {list(df.columns)}', 
                    ha='center', va='center', fontsize=12)
            plt.axis('off')
            plt.tight_layout()
            plt.savefig(image_path)
            plt.close()
            print(f"  ✓ Imagen de información guardada: {image_path}")
            
    except Exception as e:
        print(f"  ✗ Error al procesar {os.path.basename(csv_file)}: {e}")
        import traceback
        traceback.print_exc()
    
    print()

print("¡Proceso completado!")