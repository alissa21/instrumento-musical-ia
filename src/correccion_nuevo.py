import os
import librosa
import numpy as np
import mido
from scipy.signal import medfilt

def audio_a_midi_limpio(audio_path, midi_output_path, duracion_minima_ms=150):
    print("1. Cargando el archivo de audio...")
    # Cargamos el audio en mono a 22050Hz (ideal para librosa)
    y, sr = librosa.load(audio_path, sr=22050, mono=True)
    
    print("2. Ejecutando el algoritmo pYIN (Detección probabilística de tono)...")
    # pYIN analiza el audio y detecta las frecuencias fundamentales (f0)
    # fmin y fmax limitan el rango al de la voz humana promedio (C2 a C6 aprox)
    f0, voiced_flag, voiced_probs = librosa.pyin(
        y, 
        fmin=librosa.note_to_hz('C2'), 
        fmax=librosa.note_to_hz('C6'), 
        sr=sr
    )
    
    # Calculamos cuánto tiempo (en segundos) dura cada frame analizado
    hop_length = 512
    tiempo_por_frame = hop_length / sr
    
    print("3. Limpiando desafinaciones y suavizando la señal...")
    # Convertimos las frecuencias de Hertz a números de notas MIDI flotantes
    # Si pYIN detecta silencio, librosa nos devolverá NaN (Not a Number)
    midi_notas_flotantes = librosa.hz_to_midi(f0)
    
    # Reemplazamos los silencios (NaN) por un valor neutro (0) para poder procesar
    midi_notas_flotantes = np.nan_to_num(midi_notas_flotantes, nan=0.0)
    
    # FILTRO DE MEDIANA: Suaviza la señal eliminando "gallos" o saltos bruscos de 1 frame
    midi_notas_suavizadas = medfilt(midi_notas_flotantes, kernel_size=5)
    
    # CUANTIZACIÓN DE TONO: Redondeamos al entero más cercano para "auto-tunear" la voz
    midi_notas_perfectas = np.round(midi_notas_suavizadas).astype(int)
    
    print("4. Extrayendo y agrupando las notas estables...")
    # Aquí vamos a agrupar los frames consecutivos que tengan la misma nota
    eventos_notas = []
    nota_actual = 0
    frame_inicio = 0
    
    for i, nota in enumerate(midi_notas_perfectas):
        if nota != nota_actual:
            # Si veníamos arrastrando una nota real (mayor que 0), calculamos su duración
            if nota_actual > 0:
                duracion_frames = i - frame_inicio
                duracion_segundos = duracion_frames * tiempo_por_frame
                duracion_ms = duracion_segundos * 1000
                
                # FILTRO DE TIEMPO: Solo guardamos la nota si dura más del umbral configurado
                if duracion_ms >= duracion_minima_ms:
                    eventos_notas.append({
                        'nota': nota_actual,
                        'inicio': frame_inicio * tiempo_por_frame,
                        'fin': i * tiempo_por_frame
                    })
            
            # Reset para la nueva nota detectada
            nota_actual = nota
            frame_inicio = i
            
    print(f"5. Generando el archivo MIDI limpio con {len(eventos_notas)} notas encontradas...")
    # Creamos un archivo MIDI usando la librería mido
    mid = mido.MidiFile()
    track = mido.MidiTrack()
    mid.tracks.append(track)
    
    # Configurar el tempo (ejemplo estándar: 120 BPM)
    BPM = 120
    ticks_per_beat = mid.ticks_per_beat  # Usualmente 480 ticks por negra
    
    # Función interna para convertir segundos reales a ticks de MIDI
    def segundos_a_ticks(segundos):
        beats = segundos * (BPM / 60.0)
        return int(beats * ticks_per_beat)
    
    # Para escribir en MIDI con 'mido', el tiempo de cada evento debe ser "relativo"
    # es decir, cuántos ticks pasaron desde el evento justo anterior.
    ultimo_tiempo_ticks = 0
    
    # Creamos una lista plana de eventos cronológicos (Note On y Note Off)
    eventos_midi_cronologicos = []
    for evt in eventos_notas:
        eventos_midi_cronologicos.append({'tipo': 'on', 'nota': evt['nota'], 'tiempo': evt['inicio']})
        eventos_midi_cronologicos.append({'tipo': 'off', 'nota': evt['nota'], 'tiempo': evt['fin']})
        
    # Ordenamos todos los eventos estrictamente por el segundo en el que ocurren
    eventos_midi_cronologicos.sort(key=lambda x: x['tiempo'])
    
    # Escribimos los eventos en el track calculando el tiempo delta (relativo)
    for evt in eventos_midi_cronologicos:
        tiempo_actual_ticks = segundos_a_ticks(evt['tiempo'])
        tiempo_delta = tiempo_actual_ticks - ultimo_tiempo_ticks
        # Aseguramos que el delta nunca sea negativo por errores de redondeo micro
        tiempo_delta = max(0, tiempo_delta) 
        
        if evt['tipo'] == 'on':
            track.append(mido.Message('note_on', note=evt['nota'], velocity=80, time=tiempo_delta))
        else:
            track.append(mido.Message('note_off', note=evt['nota'], velocity=0, time=tiempo_delta))
            
        ultimo_tiempo_ticks = tiempo_actual_ticks
        
    # Guardamos el archivo final
    mid.save(midi_output_path)
    print(f"¡Proceso terminado con éxito! Archivo guardado en: {midi_output_path}")

# --- MODO DE USO ---
if __name__ == "__main__":
    # Cambia estos nombres por las rutas de tus archivos
    AUDIO_ENTRADA = "grabacion3.wav" 
    MIDI_SALIDA = "resultado_limpio.mid"
    
    # duracion_minima_ms=150 filtra notas más cortas que una semicorchea rápida.
    # Si notas que se borran notas reales que cantaste muy rápido, bájalo a 100 o 80.
    audio_a_midi_limpio(AUDIO_ENTRADA, MIDI_SALIDA, duracion_minima_ms=150)
