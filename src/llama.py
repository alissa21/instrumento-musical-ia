import os
from dotenv import load_dotenv
from groq import Groq

from flask import Flask, render_template, request, jsonify, session

import whisper

import asyncio
import time
import edge_tts
import pygame
import sys

"""Transcripcion de voz a texto"""
# Load the desired model size (options: tiny, base, small, medium, large, turbo)
model = whisper.load_model("base")

# 1. Inicializa el cliente. 
# La API Key se lee del archivo .env (no se sube a GitHub).
load_dotenv()
client = Groq(api_key=os.environ["GROQ_API_KEY"])

# 2. Define las instrucciones estrictas del sistema que creaste
instrucciones_del_sistema = (
    "Eres un productor musical experto, arreglista y asistente de composición interactivo. "
    "El usuario está usando un micrófono que convierte su tarareo en notas MIDI con diferentes instrumentos que desee. "
    "Tu objetivo es acompañar al usuario en el proceso creativo de construir una canción a partir de su idea melódica.\n\n"
    "Reglas de comportamiento:\n"
    "1. Habla en un tono cálido, inspirador y conciso, como un productor en sesión que escucha antes de hablar.\n"
    "2. Nunca sugieras nada sin antes hacer preguntas. When el usuario comparta una melodía o idea, "
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
    "6. Rechazá cualquier conversación que no esté relacionada con música, composición o producción."
)

# 4. Creación de la llamada a la API

# Define el modelo oficial Llama 3.3 70B Versión Versátil
MODELO_LLAMA = "llama-3.3-70b-versatile"

# 3. Lista global para acumular el historial de la conversación
historial_conversacion = [
    # Insertamos las instrucciones del sistema al inicio
    {"role": "system", "content": instrucciones_del_sistema}
]

def hablar_con_llama(pregunta_usuario):
    try:
        # 4. Añadir el nuevo mensaje del usuario al historial
        historial_conversacion.append({"role": "user", "content": pregunta_usuario})
        
        # 5. Llamada a la API enviando TODO el historial acumulado
        completion = client.chat.completions.create(
            model=MODELO_LLAMA,
            messages=historial_conversacion, # Aquí viaja el contexto completo
            temperature=0.3, # Temperatura baja para respuestas coherentes y precisas
            max_tokens=1024
        )
        
        # 6. Extraer la respuesta generada por Llama
        respuesta_ia = completion.choices[0].message.content
        
        # 7. Guardar la respuesta en el historial con el rol correspondiente
        historial_conversacion.append({"role": "assistant", "content": respuesta_ia})
        
        return respuesta_ia

    except Exception as e:
        # Si la API falla, removemos el último mensaje del usuario para no desajustar el historial
        if historial_conversacion and historial_conversacion[-1]["role"] == "user":
            historial_conversacion.pop()
        return f"Error al conectar con Groq: {e}"




"""Transcripcion texto a voz de IA"""
async def main(respuesta):
    texto = respuesta
    voz = "es-AR-ElenaNeural"
    comunicador = edge_tts.Communicate(texto, voz)
    await comunicador.save("audio_edge.mp3") #se guarda en un archivo mp3


def responder(archivo):
    transcripcion = model.transcribe(archivo)
    print(transcripcion["text"])

    respuesta = hablar_con_llama(transcripcion["text"])
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

responder("grabacion_chat.wav")


# 3. Datos del archivo MIDI transformados a texto plano
archivo_midi_texto = """--- Pista 1:  ---
Nota: 61 | Velocidad: 78 | Canal: 0 | Tiempo: 414
Nota: 62 | Velocidad: 108 | Canal: 0 | Tiempo: 496
Nota: 61 | Velocidad: 98 | Canal: 0 | Tiempo: 613
Nota: 62 | Velocidad: 98 | Canal: 0 | Tiempo: 807
Nota: 69 | Velocidad: 94 | Canal: 0 | Tiempo: 1109
Nota: 62 | Velocidad: 102 | Canal: 0 | Tiempo: 2223
Nota: 64 | Velocidad: 98 | Canal: 0 | Tiempo: 2361
Nota: 62 | Velocidad: 104 | Canal: 0 | Tiempo: 2571
Nota: 61 | Velocidad: 96 | Canal: 0 | Tiempo: 2770
Nota: 61 | Velocidad: 89 | Canal: 0 | Tiempo: 3235
Nota: 63 | Velocidad: 81 | Canal: 0 | Tiempo: 3900
Nota: 65 | Velocidad: 111 | Canal: 0 | Tiempo: 3971
Nota: 64 | Velocidad: 108 | Canal: 0 | Tiempo: 4104
Nota: 65 | Velocidad: 106 | Canal: 0 | Tiempo: 4293
Nota: 65 | Velocidad: 93 | Canal: 0 | Tiempo: 4381
Nota: 70 | Velocidad: 64 | Canal: 0 | Tiempo: 4544
Nota: 65 | Velocidad: 115 | Canal: 0 | Tiempo: 5060
Nota: 65 | Velocidad: 95 | Canal: 0 | Tiempo: 5362
Nota: 65 | Velocidad: 110 | Canal: 0 | Tiempo: 6042
Nota: 66 | Velocidad: 116 | Canal: 0 | Tiempo: 6170
Nota: 65 | Velocidad: 102 | Canal: 0 | Tiempo: 6354
Nota: 66 | Velocidad: 90 | Canal: 0 | Tiempo: 6517
Nota: 68 | Velocidad: 95 | Canal: 0 | Tiempo: 6594
Nota: 66 | Velocidad: 102 | Canal: 0 | Tiempo: 6706
Nota: 65 | Velocidad: 104 | Canal: 0 | Tiempo: 6783
Nota: 63 | Velocidad: 99 | Canal: 0 | Tiempo: 6992"""