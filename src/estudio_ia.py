"""Lo que el panel web (dashboard.py) hace con IA: hablar con Gemini y grabar una melodía.

Usa las mismas funciones que la app con el ESP32: gem.py (Whisper, Gemini y la voz) y
convertir.py / correccion.py (Basic Pitch y la tonalidad). La diferencia es que acá el audio
llega grabado desde el navegador y la voz de la IA se devuelve al navegador para que la
reproduzca, en vez de sonar en los parlantes con pygame.

Los módulos de IA se importan recién la primera vez que se usan: cargarlos tarda unos
segundos y así el panel abre rápido aunque solo se quiera generar un ensamble."""
import base64
import json
import os
import tempfile
import threading

import mido

# Una sola cosa a la vez: el chat de Gemini guarda el historial y la voz se escribe siempre
# en el mismo archivo (respuesta_ia.mp3), así que dos pedidos juntos se pisarían.
LOCK = threading.Lock()
MAX_AUDIO = 16_000_000   # bytes de WAV (unos 6 minutos a 22 kHz)

ultima_propuesta = ""    # lo último que propuso la IA, para saber si después el usuario lo aceptó
_voz_cargada = False


def _gem():
    """gem.py o chatgpt.py (según IA_CHAT en el .env) con la voz que se eligió en la app (config.json)."""
    global _voz_cargada
    from asistente import gem
    if not _voz_cargada and os.path.exists("config.json"):
        with open("config.json", encoding="utf-8") as f:
            gem.voz_actual = json.load(f).get("voz", gem.voz_actual)
        _voz_cargada = True
    return gem


def _guardar_wav(audio_b64, carpeta, nombre):
    datos = base64.b64decode(audio_b64, validate=True)
    if not 44 < len(datos) <= MAX_AUDIO or datos[:4] != b"RIFF" or datos[8:12] != b"WAVE":
        raise ValueError("La grabación llegó vacía o no es un WAV.")
    ruta = os.path.join(carpeta, nombre)
    with open(ruta, "wb") as f:
        f.write(datos)
    return ruta


def _voz(gem, texto, carpeta):
    """La respuesta en voz (ElevenLabs, o edge-tts / gTTS de respaldo), lista para el navegador.
    Se guarda en la carpeta temporal del pedido: si la app está abierta a la vez, ella usa
    respuesta_ia.mp3 y escribir los dos el mismo archivo fallaba."""
    archivo = gem.texto_a_voz(texto, archivo=os.path.join(carpeta, "voz.mp3"),
                              archivo_respaldo=os.path.join(carpeta, "voz_respaldo.mp3"))
    with open(archivo, "rb") as f:
        return "data:audio/mpeg;base64," + base64.b64encode(f.read()).decode()


def _melodia_para_gemini(midi_b64, carpeta):
    """La melodía cargada en el panel, pasada a texto, y su tonalidad (como en la app)."""
    import correccion
    ruta = os.path.join(carpeta, "melodia.mid")
    with open(ruta, "wb") as f:
        f.write(base64.b64decode(midi_b64, validate=True))
    tonalidad = correccion.texto_tonalidad(correccion.detectar_tonalidad(ruta))
    return correccion.midi_txt(ruta), tonalidad


def hablar(audio_b64=None, texto=None, midi_b64=None):
    """Un mensaje al chat, grabado (audio_b64) o escrito (texto). Devuelve lo que se entendió,
    la respuesta de la IA en texto y en voz y, si el usuario aceptó la propuesta de la IA, los
    datos del arreglo (instrumentos y carácter) para que el panel genere el acompañamiento.
    midi_b64 = la melodía cargada en el panel (sin melodía no hay nada que acompañar)."""
    global ultima_propuesta
    gem = _gem()
    with LOCK, tempfile.TemporaryDirectory() as carpeta:
        if audio_b64:
            mensaje = gem.transcribir(_guardar_wav(audio_b64, carpeta, "mensaje.wav"))
        else:
            mensaje = (texto or "").strip()
        if not mensaje:
            raise ValueError("No se entendió lo que dijiste. Probá de nuevo, un poco más cerca del micrófono.")
        print(f"Usuario (panel): {mensaje}")
        respuesta = gem.hablar_con_mi_gem(mensaje)
        print(f"IA (panel): {respuesta}")

        arreglo = None
        if midi_b64 and gem.acepto_sugerencia(ultima_propuesta, mensaje):
            melodia_txt, tonalidad = _melodia_para_gemini(midi_b64, carpeta)
            arreglo = gem.pedir_arreglo_json(f"Propuesta anterior de la IA: {ultima_propuesta}\n"
                                             f"Pedido del usuario: {mensaje}\n"
                                             f"Respuesta de la IA a ese pedido: {respuesta}",
                                             melodia_txt, tonalidad)
            print(f"Arreglo pedido (panel): {arreglo}")
            ultima_propuesta = ""
        else:
            ultima_propuesta = respuesta
        return {"mensaje": mensaje, "respuesta": respuesta, "audio": _voz(gem, respuesta, carpeta),
                "arreglo": arreglo}


def grabar_melodia(audio_b64):
    """Una melodía cantada: Basic Pitch la pasa a MIDI, se afina dentro de la escala (igual que
    en la app) y la IA la analiza. Devuelve el MIDI para cargarlo en el panel y lo que dijo la IA."""
    global ultima_propuesta
    import convertir
    import correccion
    gem = _gem()
    with LOCK, tempfile.TemporaryDirectory() as carpeta:
        wav = _guardar_wav(audio_b64, carpeta, "grabacion_panel.wav")
        with gem.cronometro("Basic Pitch (melodía a MIDI)"):
            archivo = convertir.conversion(wav)   # queda en midi/grabacion_panel_basic_pitchN.mid
        midi = mido.MidiFile(archivo)
        if not any(m.type == "note_on" and m.velocity for p in midi.tracks for m in p):
            raise ValueError("No se detectaron notas en la grabación. Probá cantar más fuerte o más cerca.")
        tonalidad = correccion.detectar_tonalidad(archivo)
        correccion.corregir(archivo, tonalidad)   # afina las notas dentro de la escala
        ritmo = correccion.cuantizar(archivo)     # tiempos -> negras, corcheas...
        if ritmo:
            print(f"Cuantización (panel): pulso de {ritmo['bpm']} BPM, notas movidas en promedio "
                  f"{ritmo['movimiento_medio_ms']} ms")
        respuesta = gem.hablar_con_mi_gem(
            gem.texto_analisis(correccion.midi_txt(archivo), correccion.texto_tonalidad(tonalidad)))
        ultima_propuesta = respuesta
        with open(archivo, "rb") as f:
            midi_b64 = base64.b64encode(f.read()).decode()
        return {"name": os.path.basename(archivo), "midi": midi_b64,
                "respuesta": respuesta, "audio": _voz(gem, respuesta, carpeta)}
