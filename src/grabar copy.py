import sounddevice as sd
import numpy as np
import scipy.io.wavfile as wav
from pynput import keyboard

# Configuraciones de audio
fs = 44100  # Frecuencia de muestreo
grabacion_chat = [] #Aca se guardan los bloques de audio grabados para charla
grabacion_melodia = [] #Aca se guardan los bloques de audio grabados para melodia
grabando = False
tipo = "" #Dependiendo lo que sea esta variable, va a guardar los bloques de audio en alguna de las dos variables grabacion

def callback(indata, frames, time, status):
    """Esta función se llama para cada bloque de audio."""
    
    if grabando and tipo == "melodia":
        grabacion_melodia.append(indata.copy())

    if grabando and tipo == "chat":
        grabacion_chat.append(indata.copy())

def on_press(key):
    global grabando
    global tipo
    if key == keyboard.KeyCode.from_char('r') or key == keyboard.KeyCode.from_char('c'):
        if key == keyboard.KeyCode.from_char('r'):
            tipo = "melodia"
        if key == keyboard.KeyCode.from_char('c'):
            tipo = "chat"

        if not grabando:
            print("Iniciando grabación...")
            grabando = True
            
        elif grabando:
            print("Grabación guardada.")
            grabando = False
            # Detener el listener o romper el bucle
            return False
    

# Iniciar la escucha del teclado
def grabar():

    tipos = ""
    with sd.InputStream(callback=callback, samplerate=fs, channels=1):

    # Si detecta que hay bloques de audio en la grabacion, los guarda en un archivo
    if len(grabacion_melodia) > 0:
        audio_completo = np.concatenate(grabacion_melodia, axis=0)
        wav.write('grabacion_melodia.wav', fs, audio_completo)
        print("Archivo 'grabacion_melodia.wav' guardado.")
        tipos = "melodia"
    
    if len(grabacion_chat) > 0:
        audio_completo = np.concatenate(grabacion_chat, axis=0)
        wav.write('grabacion_chat.wav', fs, audio_completo)
        print("Archivo 'grabacion_chat.wav' guardado.")
        tipos = "chat"
    
    return tipos
