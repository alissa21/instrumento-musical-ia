import fluidsynth
import time
import mido
sf2_path = r"C:\Users\User\Documents\instrumento-musical-ia\assets\soundfonts\FluidR3_GM.sf2" #soundfont con los instrumentos
# midi_path = r"C:\Users\User\Documents\instrumento-musical-ia\midi\grabacion_basic_pitch1.mid" 
#midi_path = r"C:\Users\User\Documents\instrumento-musical-ia\resultado_limpio.mid" 
midi_path = r"C:\Users\User\Documents\instrumento-musical-ia\pruebas_sonido\grabacion_basic_pitc.mid" 

fs = fluidsynth.Synth()
fs.setting("synth.gain", 1.0)
fs.start(driver="dsound") 
# 3. Cargar SoundFont
sfid = fs.sfload(sf2_path)

# 0 = Piano, 24 = Guitarra, 40 = Violín, 73 = Flauta


def reproducir_midi(midi_path, INSTRUMENTO_DESEADO):
    resultado = fs.program_select(0, sfid, 0, INSTRUMENTO_DESEADO)
    print(f"[debug] instrumento recibido: {INSTRUMENTO_DESEADO!r} | program_select devolvio: {resultado} (0 = OK)")
    mid = mido.MidiFile(midi_path)
    # mido.MidiFile carga el archivo y .play() genera los mensajes con el delay correcto
    for msg in mido.MidiFile(midi_path).play():
        # Filtrar solo los mensajes que generan sonido
        # El parametro msg.channel es 0, porque el instrumento lo elegimos nosotros. 
        if msg.type == 'note_on':
            # msg.channel, msg.note, msg.velocity
            fs.noteon(0, msg.note, msg.velocity)    
        elif msg.type == 'note_off':
            fs.noteoff(0, msg.note)  
        elif msg.type == 'control_change':
            fs.cc(0, msg.control, msg.value)     
        elif msg.type == 'program_change':
            continue 

    # El sintetizador no se libera acá: lo sigue usando el teclado. Se libera con cerrar().
    print("Fin de la reproducción.")

# ---------- Tocar en vivo (teclado del ESP32) ----------
# Canal propio para que el instrumento del teclado no pise al de la melodía (canal 0).
CANAL_TECLADO = 1


def elegir_instrumento(INSTRUMENTO_DESEADO):
    fs.program_select(CANAL_TECLADO, sfid, 0, INSTRUMENTO_DESEADO)


def tocar_nota(nota, velocidad):
    # velocidad = velocity MIDI (1-127) medido por el ESP32 con los dos contactos de la tecla
    fs.noteon(CANAL_TECLADO, nota, velocidad)


def soltar_nota(nota):
    fs.noteoff(CANAL_TECLADO, nota)


def cerrar():
    fs.delete()

# pruebas
# reproducir_midi(100)