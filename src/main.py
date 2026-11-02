import threading
import grabar, convertir, correccion, gem, reproducir, esp32


def main():
    gem.leer("Hola. Bienvenido a la estacion musical con Inteligencia Artificial, estoy aqui para ayudarte a componer y a acompañar tus ideas musicales. Cuando quieras, puedes presionar C para hablar conmigo, o R para grabar una melodia que deseas convertir en algun instrumento.")

    hay_esp32 = esp32.conectar()
    if hay_esp32:
        print("Elige en la pantalla del ESP32 el instrumento del teclado y presiona el joystick para confirmar...")
        reproducir.elegir_instrumento(esp32.esperar_instrumento_teclado())
        # El teclado suena en segundo plano mientras se graba, se conversa o se reproduce una melodía.
        threading.Thread(target=tocar_teclado, daemon=True).start()
    else:
        print("No se encontró el ESP32 (revisa el puerto en esp32.py). Sigue sin el teclado.")

    try:
        while True:
            tipo = grabar.grabar()  # "melodia" | "chat" | False al presionar x
            if tipo == "melodia":
                procesar_melodia(hay_esp32)
            elif tipo == "chat":
                procesar_chat()
            if not tipo:
                break
    except KeyboardInterrupt:
        print("Saliendo...")
    finally:
        reproducir.cerrar()


def tocar_teclado():
    actual = esp32.instrumento_teclado
    while True:
        evento = esp32.leer_tecla()
        # Si se eligió otro instrumento de teclado en la pantalla, se cambia en vivo.
        if esp32.instrumento_teclado != actual:
            actual = esp32.instrumento_teclado
            reproducir.elegir_instrumento(actual)
        if evento is None:
            continue
        tipo, nota, velocidad = evento
        if tipo == "on":
            reproducir.tocar_nota(nota, velocidad)
        else:
            reproducir.soltar_nota(nota)


def procesar_chat():
    gem.responder("grabacion_chat.wav")


def procesar_melodia(hay_esp32):
    archivo_midi = convertir.conversion("grabacion_melodia.wav")
    texto = correccion.midi_txt(archivo_midi)
    if hay_esp32:
        print("Elige en la pantalla del ESP32 el instrumento del micrófono y presiona el joystick para confirmar...")
        instrumento = esp32.esperar_instrumento_microfono()
    else:
        print(instrumentos)
        instrumento = int(input("Elige tu instrumento:"))

    reproducir.reproducir_midi(archivo_midi, instrumento)

    gem.analizar_midi(texto)

instrumentos = """ Pianos (0-7)
0: Piano de cola acústico
1: Piano acústico brillante
2: Piano de cola eléctrico
3: Piano Honky-tonk
4: Piano eléctrico 1 (Rhodes)
5: Piano eléctrico 2 (Coro)
6: Clave (Harpsichord)
7: Clavinet

Percusión Cromática (8-15)
8: Celesta
9: Glockenspiel
10: Caja de música
11: Vibráfono
12: Marimba
13: Xilófono
14: Campanas tubulares
15: Dulcémele (Dulcimer)

Órganos (16-23)
16: Órgano de barras (Hammond)
17: Órgano percusivo
18: Órgano rock
19: Órgano de iglesia
20: Armonio (Reed Organ)
21: Acordeón
22: Armónica
23: Acordeón de tango (Bandoneón) 

Guitarras (24-31)
24: Guitarra acústica (nylon)
25: Guitarra acústica (acero)
26: Guitarra eléctrica (jazz)
27: Guitarra eléctrica (limpia)
28: Guitarra eléctrica (muteada)
29: Guitarra sobrealimentada (overdrive)
30: Guitarra distorsionada
31: Armónicos de guitarra 

Bajos (32-39)
32: Bajo acústico
33: Bajo eléctrico (dedos)
34: Bajo eléctrico (púa)
35: Bajo sin trastes (fretless)
36: Bajo Slap 1
37: Bajo Slap 2
38: Bajo sintetizado 1
39: Bajo sintetizado 2 

Cuerdas Solo (40-47)
40: Violín
41: Viola
42: Violonchelo (Cello)
43: Contrabajo
44: Cuerdas en trémolo
45: Cuerdas en pizzicato
46: Arpa orquestal
47: Timbales

Conjuntos (48-55)
48: Conjunto de cuerdas 1
49: Conjunto de cuerdas 2
50: Cuerdas sintetizadas 1
51: Cuerdas sintetizadas 2
52: Coro "Aahs"
53: Voces "Oohs"
54: Voz sintetizada
55: Golpe de orquesta (Orchestra Hit)

Metales (56-63)
56: Trompeta
57: Trombón
58: Tuba
59: Trompeta con sordina
60: Trompa (corno francés)
61: Sección de metales
62: Metales sintetizados 1
63: Metales sintetizados 2

Maderas (64-71)
64: Saxofón soprano
65: Saxofón alto
66: Saxofón tenor
67: Saxofón barítono
68: Oboe
69: Corno inglés
70: Fagot
71: Clarinete

Viento (72-79)
72: Pícolo
73: Flauta
74: Flauta dulce (Recorder)
75: Flauta de Pan
76: Soplido de botella
77: Shakuhachi
78: Silbido
79: Ocarina

Sintetizadores - Lead (80-87)
80: Lead 1 (onda cuadrada)
81: Lead 2 (onda de sierra)
82: Lead 3 (calíope)
83: Lead 4 (chiff)
84: Lead 5 (charang)
85: Lead 6 (voz)
86: Lead 7 (quintas)
87: Lead 8 (bajo + lead) 

Sintetizadores - Pad (88-95)
88: Pad 1 (new age)
89: Pad 2 (cálido)
90: Pad 3 (polysynth)
91: Pad 4 (coro)
92: Pad 5 (frotado)
93: Pad 6 (metálico)
94: Pad 7 (halo)
95: Pad 8 (barrido) [1]

Efectos de Sintetizador (96-103)
96: FX 1 (lluvia)
97: FX 2 (banda sonora)
98: FX 3 (cristal)
99: FX 4 (atmósfera)
100: FX 5 (brillo)
101: FX 6 (duendes)
102: FX 7 (ecos)
103: FX 8 (ciencia ficción)

Étnicos (104-111)
104: Sitar
105: Banjo
106: Shamisen
107: Koto
108: Kalimba
109: Gaita
110: Fiddle (Violín folk)
111: Shanai 

Percusión (112-119)
112: Campanilla (Tinkle Bell)
113: Agogô
114: Tambores metálicos (Steel Drums)
115: Woodblock
116: Tambor Taiko
117: Tom melódico
118: Tambor sintetizado
119: Platillo invertido
Efectos de Sonido (120-127)
120: Ruido de traste de guitarra
121: Ruido de respiración
122: Costa/Mar
123: Canto de pájaro
124: Timbre de teléfono
125: Helicóptero
126: Aplausos
127: Disparo de arma
""" 

main()

