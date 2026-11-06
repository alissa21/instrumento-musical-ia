import sounddevice as sd
import numpy as np
import scipy.io.wavfile as wav
import esp32

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


# Espera los botones del ESP32: el primer toque (1) empieza a grabar, el segundo (0) termina
def grabar():
    global grabando
    global tipo

    if not esp32.conectar():
        print("No se encontró el ESP32: no se puede grabar sin los botones.")
        return False

    # Iniciar la grabación de audio en segundo plano
    with sd.InputStream(callback=callback, samplerate=fs, channels=1):
        print("Presione el boton de melodia para comenzar a grabar melodia, vuelva a presionarlo para guardar la grabacion.")
        print("Presione el boton de chat para comenzar a grabar mensaje a IA, vuelva a presionarlo para guardar la grabacion.")
        while True:
            evento = esp32.leer_boton()
            if evento is None:
                continue
            boton, encendido = evento

            if encendido and not grabando:
                tipo = boton
                grabando = True
                print("Iniciando grabación...")
            elif not encendido and grabando and boton == tipo:
                grabando = False
                print("Grabación guardada.")
                break

    # Si detecta que hay bloques de audio en la grabacion, los guarda en un archivo
    if tipo == "melodia" and len(grabacion_melodia) > 0:
        audio_completo = np.concatenate(grabacion_melodia, axis=0)
        wav.write('grabacion_melodia.wav', fs, audio_completo)
        print("Archivo 'grabacion_melodia.wav' guardado.")
        grabacion_melodia.clear() #vacio para siguiente audio
        return "melodia"

    if tipo == "chat" and len(grabacion_chat) > 0:
        audio_completo = np.concatenate(grabacion_chat, axis=0)
        wav.write('grabacion_chat.wav', fs, audio_completo)
        print("Archivo 'grabacion_chat.wav' guardado.")
        grabacion_chat.clear() #vacio para siguiente audio
        return "chat"

    return ""
