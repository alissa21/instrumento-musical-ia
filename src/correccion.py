import os
from mido import MidiFile
from music21 import converter, analysis
# Cargar el archivo MIDI
archivo = 'pruebas_sonido/grabacion_basic_pitc.mid'

def deteccion_nota():
    midi_stream = converter.parse(archivo)
    # Analizar la tonalidad y escala
    key = midi_stream.analyze('key')
    print(f"Tonalidad detectada: {key.tonic.name} {key.mode}")
    print(f"Escala: {key.pitches}")

# 1. Cargar el archivo MIDI
# archivo_midi = MidiFile('pruebas_sonido/grabacion_basic_pitc.mid')

# 2. Iterar sobre todas las pistas

def midi_txt(archivo):
    archivo_midi = MidiFile(archivo)
    texto = " "
    for i, pista in enumerate(archivo_midi.tracks):   
        tiempo_acumulado = 0
    
        for mensaje in pista:
        # Los mensajes tienen un tiempo "delta" (tiempo desde el evento anterior)
            tiempo_acumulado += mensaje.time
        
        # Filtrar solo los mensajes de nota musical
            if mensaje.type == 'note_on' and mensaje.velocity > 0:
                texto = f"{texto} \n Nota: {mensaje.note} | Velocidad:  {mensaje.velocity} |Canal: {mensaje.channel} | Tiempo: {tiempo_acumulado}"
                # print(texto)

    return texto