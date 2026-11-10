import mido
from music21 import harmony

import esp32
import gem
import proyectos
import reproducir

# La sugerencia de la IA siempre usa el canal 2 (ver Multipista con FluidSynth)
CANAL_IA = 2


def sugerencia_a_midi(datos, canal, ticks_por_tiempo):
    """Convierte la sugerencia de Gemini (un diccionario leído de su JSON) en una pista MIDI.
    Gemini decide qué tocar (los acordes en cifrado americano y cuántos tiempos dura cada uno)
    y acá Python decide cómo suena: music21 traduce cada cifrado a sus notas y mido arma la pista."""
    pista = mido.MidiTrack()
    pista.append(mido.MetaMessage("track_name", name="Sugerencia IA"))

    instrumento = datos.get("instrumento", 0)
    if not isinstance(instrumento, int) or not 0 <= instrumento <= 127:
        instrumento = 0   # la IA mandó cualquier cosa: piano
    pista.append(mido.Message("program_change", channel=canal, program=instrumento, time=0))

    for acorde in datos.get("acordes", []):
        try:
            # music21 traduce el cifrado ("Am") a sus notas MIDI (57, 60, 64)
            notas = [p.midi for p in harmony.ChordSymbol(acorde["cifrado"]).pitches]
            duracion = int(acorde["tiempos"] * ticks_por_tiempo)
        except Exception:
            continue  # acorde mal escrito: se saltea
        if not notas or duracion <= 0:
            continue

        for n in notas:  # todas las notas del acorde empiezan juntas
            pista.append(mido.Message("note_on", channel=canal, note=n,
                                      velocity=70, time=0))
        # En MIDI el tiempo de cada mensaje es relativo al anterior (tiempo delta).
        # Por eso solo el primer note_off lleva la duración y los demás llevan 0:
        # se apagan en el mismo instante.
        for i, n in enumerate(notas):
            pista.append(mido.Message("note_off", channel=canal, note=n,
                                      velocity=0, time=duracion if i == 0 else 0))
    return pista


def proponer_sugerencia(datos, proyecto, nombre):
    """Arma la pista de la sugerencia, la hace escuchar junto con el resto del proyecto
    y pregunta en la pantalla "¿Aceptar sugerencia?". Devuelve True si se aceptó."""
    pista = sugerencia_a_midi(datos, CANAL_IA, proyecto.ticks_per_beat)

    # Copia del proyecto con la sugerencia, solo para escucharla (sin tocar el proyecto guardado).
    # Si ya había una sugerencia aceptada antes, en la prueba se escucha la nueva en su lugar.
    prueba = mido.MidiFile(type=1, ticks_per_beat=proyecto.ticks_per_beat)
    prueba.tracks = [p for p in proyecto.tracks
                     if proyectos.canal_de_pista(p) != CANAL_IA] + [pista]

    esp32.enviar("PANTALLA:SUGERENCIA")  # "¿Aceptar sugerencia?" Sí / No
    reproducir.reproducir_proyecto(prueba)

    if esp32.esperar_si_no():  # Sí: se agrega (reemplazando la anterior) y se guarda
        proyectos.poner_pista(proyecto, pista)
        proyectos.guardar(proyecto, nombre)
        return True

    # No: se descarta. Como el chat guarda el historial, la IA sabe qué propuso
    # y pregunta qué cambiaría el usuario. Responder no es obligatorio.
    gem.hablar("El usuario escuchó la sugerencia y la rechazó. "
               "Preguntale en una sola frase qué le gustaría cambiar.")
    return False
