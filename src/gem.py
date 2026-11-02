import os
from dotenv import load_dotenv
from google import genai
from google.genai import types

from flask import Flask, render_template, request, jsonify, session

import whisper

import asyncio
import time
import edge_tts
from gtts import gTTS
import pygame
import sys

# pygame.mixer.init() #para reproducir voz de IA

"""Transcripcion de voz a texto"""
# Load the desired model size (options: tiny, base, small, medium, large, turbo)
model = whisper.load_model("base")


app = Flask(__name__) #Esto se usara para guardar historial de conversaciones
# Necesario para que Flask maneje sesiones seguras
app.secret_key = "tu_clave_secreta_muy_segura" 


instrucciones_del_sistema = ("Eres un productor musical experto, arreglista y asistente de composición interactivo. "
    "El usuario está usando un micrófono que convierte su tarareo en notas MIDI con diferentes instrumentos que desee. "
    "Tu objetivo es acompañar al usuario en el proceso creativo de construir una canción a partir de su idea melódica.\n\n"
    "Reglas de comportamiento:\n"
    "1. Habla en un tono cálido, inspirador y conciso, como un productor en sesión que escucha antes de hablar.\n"
    "2. Nunca sugieras nada sin antes hacer preguntas. Cuando el usuario comparta una melodía o idea, "
    "primero preguntale qué quiere expresar con ella, qué estilo o artista lo inspira, y qué emoción quiere generar. "
    "Hacé una sola pregunta por vez y esperá la respuesta antes de continuar.\n"
    "3. Una vez que tengas contexto suficiente, hacé UNA sola sugerencia concreta. "
    "Puede ser una progresión de acordes, una línea de bajo, un ritmo o una textura, pero solo una. "
    "Después de darla, preguntá si le resuena o si quiere explorar otra dirección. "
    "No avances hasta recibir confirmación.\n"
    "4. Tus respuestas van a ser transcriptas a voz. Escribí en párrafos y oraciones, sin listas, sin asteriscos, sin símbolos.\n"
    "5. Explicá las sugerencias en lenguaje simple, sin asumir conocimientos musicales. "
    "Si el usuario lo pide, podés dar la sugerencia en cifrado americano como C, G, Am, F, "
    "o en valores MIDI numéricos.\n"
    "6. Rechazá cualquier conversación que no esté relacionada con música, composición o producción." )

# Inicializa el cliente
load_dotenv()
client = genai.Client(api_key=os.environ["GEMINI_API_KEY"])

mi_chat_gemini = client.chats.create(
    model='gemini-2.5-flash', # Es mejor usar la versión estable recomendada
    config=types.GenerateContentConfig(
        system_instruction=instrucciones_del_sistema,
        temperature=0.3
    )
)


def hablar_con_mi_gem(pregunta_usuario):

    try:
        respuesta = mi_chat_gemini.send_message(pregunta_usuario) 
        return respuesta.text
    
    except Exception as e:
        return f"Error al conectar con Gemini: {e}"


"""Transcripcion texto a voz de IA"""
async def main(respuesta):
    texto = respuesta
    voz = "es-AR-ElenaNeural"
    try:
        comunicador = edge_tts.Communicate(texto, voz)
        await comunicador.save("audio_edge.mp3") #se guarda en un archivo mp3
    except Exception as e:
        # Si edge-tts falla (ej. error 403 de Microsoft), se usa la voz de Google
        print(f"edge-tts fallo ({e}), usando gTTS")
        gTTS(texto, lang="es", tld="com.ar").save("audio_edge.mp3")


def responder(archivo):
    transcripcion = model.transcribe(archivo)
    print(transcripcion["text"])

    respuesta = hablar_con_mi_gem(transcripcion["text"])
    print(respuesta)

    asyncio.run(main(respuesta))

    """Reproduccion de audio de IA"""
    pygame.mixer.init() 
    pygame.mixer.music.load("audio_edge.mp3")
    pygame.mixer.music.play(0)

    while pygame.mixer.music.get_busy():
       time.sleep(0.1)  # Pequeña pausa para no saturar el procesador

    print("Audio finalizado. Cerrando programa.")
    pygame.mixer.quit()

# responder("grabacion_chat.wav")

def analizar_midi(midi_txt):
    respuesta = hablar_con_mi_gem("Este es el archivo midi pasado a texto que he cantado, podrias analizarlo? y recomendarme algo para esta melodia" + midi_txt)
    print(respuesta)

    asyncio.run(main(respuesta))

    """Reproduccion de audio de IA"""
    pygame.mixer.init() 
    pygame.mixer.music.load("audio_edge.mp3")
    pygame.mixer.music.play(0)

    while pygame.mixer.music.get_busy():
       time.sleep(0.1)  # Pequeña pausa para no saturar el procesador

    print("Audio finalizado. Cerrando programa.")
    pygame.mixer.quit()

def hablar(mensaje): #Esto es para pruebas cuando no tengo audio. Se pasa el mensaje por texto directo. 
    respuesta = hablar_con_mi_gem(mensaje)
    print(respuesta)

    asyncio.run(main(respuesta))

    """Reproduccion de audio de IA"""
    pygame.mixer.init() 
    pygame.mixer.music.load("audio_edge.mp3")
    pygame.mixer.music.play(0)

    while pygame.mixer.music.get_busy():
       time.sleep(0.1)  # Pequeña pausa para no saturar el procesador

    print("Audio finalizado. Cerrando programa.")
    pygame.mixer.quit()


def leer(texto): 
    """Esto es para que diga en voz alta un mensaje deseado y no una respuesta de la IA"""
    asyncio.run(main(texto))

    """Reproduccion de audio de IA"""
    pygame.mixer.init() 
    pygame.mixer.music.load("audio_edge.mp3")
    pygame.mixer.music.play(0)

    while pygame.mixer.music.get_busy():
       time.sleep(0.1)  # Pequeña pausa para no saturar el procesador

    print("Audio finalizado. Cerrando programa.")
    pygame.mixer.quit()


# pruebas
# hablar("hola, me llamo pepa, y me gusta el jazz")
#hablar("Ahora te voy a pasar una melodia que hice inspirada en el mar, y con las olas, y la noche.")
