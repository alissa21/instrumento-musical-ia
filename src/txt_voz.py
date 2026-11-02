import asyncio
import time
import edge_tts
import pygame
import sys

pygame.mixer.init()

async def main():
    texto = "fmskfa"
    voz = "es-ES-AlvaroNeural" # Voz en español de España, puedes usar latinas como es-AR
    comunicador = edge_tts.Communicate(texto, voz)
    await comunicador.save("audio_edge.mp3")

asyncio.run(main())

pygame.mixer.music.load("audio_edge.mp3")
pygame.mixer.music.play(0)

while pygame.mixer.music.get_busy():
    time.sleep(0.1)  # Pequeña pausa para no saturar el procesador

print("Audio finalizado. Cerrando programa.")
pygame.mixer.quit()
