import asyncio
import json
import os
import re
import time

from dotenv import load_dotenv
from google import genai
from google.genai import types
import whisper
import edge_tts
from gtts import gTTS
import pygame
from elevenlabs.client import ElevenLabs

import reproducir

load_dotenv()

MODELO = "gemini-2.5-flash"

# =====================================================================
#  Transcripción de voz a texto (Whisper)
# =====================================================================
# Tamaños de modelo: tiny, base, small, medium, large, turbo
model = whisper.load_model("base")


def transcribir(archivo):
    return model.transcribe(archivo, language="es")["text"].strip()


# =====================================================================
#  Chat con Gemini
# =====================================================================
instrucciones_del_sistema = (
    "Eres un productor musical experto, arreglista y asistente de composición interactivo. "
    "El usuario está usando un instrumento que convierte su tarareo en notas MIDI con el instrumento que desee, "
    "y un teclado que mide la fuerza de cada tecla. Cada canción es un proyecto con varias pistas. "
    "Tu objetivo es acompañar al usuario en el proceso creativo de construir una canción a partir de su idea melódica.\n\n"
    "Reglas de comportamiento:\n"
    "1. Habla en un tono cálido, inspirador y conciso, como un productor en sesión que escucha antes de hablar.\n"
    "2. Nunca sugieras nada sin antes hacer preguntas. Cuando el usuario comparta una melodía o idea, "
    "primero pregúntale qué quiere expresar con ella, qué estilo o artista lo inspira, y qué emoción quiere generar. "
    "Haz una sola pregunta por vez y espera la respuesta antes de continuar.\n"
    "3. Una vez que tengas contexto suficiente, haz UNA sola sugerencia concreta. "
    "Puede ser una progresión de acordes, una línea de bajo, un ritmo o una textura, pero solo una. "
    "Después de darla, pregunta si quiere escucharla o si prefiere explorar otra dirección.\n"
    "4. Cuando el usuario acepta una sugerencia, el sistema la toca automáticamente junto con su melodía. "
    "En ese caso responde con una sola frase corta, por ejemplo avisando que ya la va a escuchar.\n"
    "5. Tus respuestas van a ser transcriptas a voz. Escribe en párrafos y oraciones, sin listas, sin asteriscos, sin símbolos.\n"
    "6. Explica las sugerencias en lenguaje simple, sin asumir conocimientos musicales.\n"
    "7. Rechaza cualquier conversación que no esté relacionada con música, composición o producción."
)

client = genai.Client(api_key=os.environ["GEMINI_API_KEY"])

# El chat guarda el historial, así Gemini recuerda cada mensaje de la conversación.
mi_chat_gemini = client.chats.create(
    model=MODELO,
    config=types.GenerateContentConfig(
        system_instruction=instrucciones_del_sistema,
        temperature=0.3,
    ),
)


def hablar_con_mi_gem(mensaje):
    try:
        return mi_chat_gemini.send_message(mensaje).text
    except Exception as e:
        print(f"Error al conectar con Gemini: {e}")
        return "Perdón, no me pude conectar. ¿Puedes repetirlo en un momento?"


# =====================================================================
#  Voz de la IA (ElevenLabs, con edge-tts como respaldo)
# =====================================================================
# ID de las 4 voces elegidas en la biblioteca de ElevenLabs
VOCES = {
    0: "ID_VOZ_1",
    1: "ID_VOZ_2",
    2: "ID_VOZ_3",
    3: "ID_VOZ_4",
}
# Voces de edge-tts parecidas, por si ElevenLabs no responde
VOCES_EDGE = {
    0: "es-AR-ElenaNeural",
    1: "es-AR-TomasNeural",
    2: "es-MX-DaliaNeural",
    3: "es-MX-JorgeNeural",
}
voz_actual = 0  # la cambia el menú de la pantalla

# La clave se guarda en la variable de entorno ELEVENLABS_API_KEY (archivo .env), no en el código.
_clave_voz = os.getenv("ELEVENLABS_API_KEY")
cliente_voz = ElevenLabs(api_key=_clave_voz) if _clave_voz else None

INSTRUCCIONES_EXPRESION = (
    "Recibís un texto que va a decir en voz alta un asistente musical. "
    "Agregale etiquetas de audio de ElevenLabs entre corchetes, en inglés, "
    "como [laughs], [sighs], [excited], [softly], [curious] o [pause], "
    "donde ayuden a expresar la emoción del mensaje. "
    "No cambies, agregues ni quites ninguna palabra del texto. "
    "Usá como máximo tres etiquetas. Respondé solo con el texto con las etiquetas."
)


def agregar_expresion(texto):
    """Pedido aparte del chat: agrega etiquetas de audio sin cambiar ninguna palabra."""
    try:
        respuesta = client.models.generate_content(
            model=MODELO,
            contents=texto,
            config=types.GenerateContentConfig(
                system_instruction=INSTRUCCIONES_EXPRESION,
                temperature=0.3,
            ),
        )
        return respuesta.text.strip()
    except Exception:
        return texto  # si falla, la voz sale igual, sin etiquetas


def sin_etiquetas(texto):
    """Saca las etiquetas [excited], [softly]... para que edge-tts no las lea en voz alta."""
    return re.sub(r"\[[^\]]*\]\s*", "", texto).strip()


async def voz_edge(texto):
    """Respaldo: edge-tts (gratis). Si también falla, se usa la voz de Google (gTTS)."""
    try:
        comunicador = edge_tts.Communicate(texto, VOCES_EDGE[voz_actual])
        await comunicador.save("audio_edge.mp3")
    except Exception as e:
        print(f"edge-tts falló ({e}), se usa gTTS")
        gTTS(texto, lang="es", tld="com.ar").save("audio_edge.mp3")


def texto_a_voz(texto, expresar=True):
    """Convierte el texto en voz y devuelve el nombre del archivo de audio.
    expresar=False para las frases fijas, que ya llevan sus etiquetas escritas a mano."""
    try:
        if cliente_voz is None:
            raise RuntimeError("falta ELEVENLABS_API_KEY")
        audio = cliente_voz.text_to_speech.convert(
            text=agregar_expresion(texto) if expresar else texto,  # con etiquetas de audio
            voice_id=VOCES[voz_actual],
            model_id="eleven_v4_turbo",
            output_format="mp3_44100_128",
        )
        with open("respuesta_ia.mp3", "wb") as f:
            for parte in audio:  # el audio llega en partes y se van escribiendo
                f.write(parte)
        return "respuesta_ia.mp3"
    except Exception as e:
        print(f"ElevenLabs no respondió ({e}). Se usa edge-tts.")
        asyncio.run(voz_edge(sin_etiquetas(texto)))  # edge-tts recibe el texto sin etiquetas
        return "audio_edge.mp3"


def reproducir_audio(archivo):
    """Reproduce un audio (la voz de la IA o una frase ya grabada) con el volumen de la perilla."""
    if not pygame.mixer.get_init():
        pygame.mixer.init()
    pygame.mixer.music.load(archivo)
    pygame.mixer.music.set_volume(reproducir.volumen_general / 127)
    pygame.mixer.music.play()
    while pygame.mixer.music.get_busy():
        time.sleep(0.1)  # pequeña pausa para no saturar el procesador
    pygame.mixer.music.unload()  # suelta el archivo para poder sobrescribirlo la próxima vez


def leer(texto, expresar=True):
    """Dice en voz alta un texto (una respuesta de la IA o una frase fija)."""
    print(f"IA: {texto}")
    reproducir_audio(texto_a_voz(texto, expresar))


# =====================================================================
#  Conversación
# =====================================================================

def hablar(mensaje):
    """Le manda un mensaje al chat y dice la respuesta en voz alta. Devuelve la respuesta."""
    respuesta = hablar_con_mi_gem(mensaje)
    leer(respuesta)
    return respuesta


def responder(archivo):
    """Transcribe el mensaje de voz grabado con el botón de chat y responde en voz alta.
    Devuelve (mensaje del usuario, respuesta de la IA), o None si no se entendió nada."""
    mensaje = transcribir(archivo)
    print(f"Usuario: {mensaje}")
    if not mensaje:
        return None
    return mensaje, hablar(mensaje)


def analizar_midi(midi_txt, tonalidad):
    """Le pasa a la IA la melodía desglosada en texto para que la analice y empiece a preguntar."""
    return hablar(
        "Este es el archivo MIDI, pasado a texto, de la melodía que acabo de cantar. "
        f"Tonalidad detectada: {tonalidad}. ¿Podrías analizarla?\n{midi_txt}"
    )


# =====================================================================
#  Pedidos separados del chat
# =====================================================================

# ---------- ¿El usuario aceptó la sugerencia? ----------
INSTRUCCIONES_ACEPTA = (
    "Recibís la última propuesta musical que hizo un asistente y lo que le respondió la persona. "
    "Respondé solamente SI si la persona está de acuerdo con la propuesta y quiere escucharla, "
    "o NO en cualquier otro caso."
)


def acepto_sugerencia(propuesta, mensaje):
    try:
        respuesta = client.models.generate_content(
            model=MODELO,
            contents=f"Propuesta del asistente: {propuesta}\nRespuesta de la persona: {mensaje}",
            config=types.GenerateContentConfig(
                system_instruction=INSTRUCCIONES_ACEPTA,
                temperature=0,  # respuesta exacta, sin creatividad
            ),
        )
        return respuesta.text.strip().upper().startswith("SI")
    except Exception:
        return False


# ---------- Sugerencia en formato JSON ----------
def pedir_sugerencia_json(idea_aceptada, melodia_txt, tonalidad):
    pedido = (
        "Convertí esta sugerencia en datos. Respondé solo con JSON con las claves "
        "instrumento (número 0-127 de General MIDI) y acordes "
        "(lista de objetos con cifrado y tiempos).\n"
        f"Sugerencia aceptada: {idea_aceptada}\n"
        f"Tonalidad de la melodía: {tonalidad}\n"
        f"Melodía: {melodia_txt}"
    )
    respuesta = client.models.generate_content(
        model=MODELO,
        contents=pedido,
        config=types.GenerateContentConfig(
            response_mime_type="application/json",  # obliga a responder en JSON
            temperature=0.3,
        ),
    )
    return json.loads(respuesta.text)  # convierte el texto JSON en un diccionario


# ---------- Corrección del nombre ----------
INSTRUCCIONES_NOMBRE = (
    "Recibís la transcripción de una persona diciendo un nombre. Puede incluir aclaraciones "
    "sobre cómo se escribe, por ejemplo 'con doble s', 'con y griega' o 'con hache'. "
    "También recibís los intentos anteriores que la persona marcó como incorrectos. "
    "Respondé solamente con el nombre escrito correctamente, sin comillas ni explicaciones."
)


def corregir_nombre(transcripcion, rechazados):
    respuesta = client.models.generate_content(
        model=MODELO,
        contents=f"Transcripción: {transcripcion}\nIntentos rechazados: {', '.join(rechazados)}",
        config=types.GenerateContentConfig(
            system_instruction=INSTRUCCIONES_NOMBRE,
            temperature=0,  # respuesta exacta, sin creatividad
        ),
    )
    return respuesta.text.strip()
