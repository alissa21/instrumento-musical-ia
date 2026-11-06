import json
import os
import threading
import time

import grabar, convertir, correccion, gem, reproducir, esp32, proyectos, sugerencia

CONFIG = "config.json"

# Cada tipo de pista usa siempre el mismo canal MIDI (ver Multipista con FluidSynth)
CANAL_MELODIA = 0
CANAL_TECLADO = 1
CANAL_IA = 2
NOMBRE_PISTA = {CANAL_MELODIA: "la melodía", CANAL_TECLADO: "el teclado", CANAL_IA: "la sugerencia de la IA"}

proyecto = None            # proyecto abierto (archivo MIDI multipista) o None en la pantalla de inicio
nombre = None              # nombre del archivo del proyecto, sin .mid
ultima_propuesta = ""      # última idea que propuso la IA, por si el usuario la acepta
grabacion_teclado = None   # notas tocadas mientras se graba el teclado (None = no se está grabando)
inicio_teclado = 0.0


# =====================================================================
#  Configuración (primer inicio)
# =====================================================================

def cargar_config():
    if not os.path.exists(CONFIG):
        return None  # no existe: es el primer inicio
    with open(CONFIG, encoding="utf-8") as f:
        return json.load(f)


def guardar_config(usuario, voz):
    with open(CONFIG, "w", encoding="utf-8") as f:
        json.dump({"usuario": usuario, "voz": voz}, f, ensure_ascii=False)


def primer_inicio():
    """Bienvenida -> elegir la voz de la IA -> decir el nombre del usuario. Se hace una sola vez."""
    esp32.enviar("PANTALLA:BIENVENIDA")
    voz = esp32.esperar_voz()   # después de elegirla, el ESP32 muestra solo la pantalla del micrófono
    gem.voz_actual = voz
    gem.leer("[warmly] Hola, voy a ser tu asistente musical. ¿Cómo te llamas? "
             "Presiona el botón de chat, dime tu nombre y vuelve a presionarlo.", expresar=False)
    usuario = proyectos.pedir_nombre("usuario")   # micrófono y "¿Es correcto?"
    guardar_config(usuario, voz)
    return {"usuario": usuario, "voz": voz}


# =====================================================================
#  Programa principal
# =====================================================================

def main():
    if not esp32.conectar():
        print("No se encontró el ESP32 (revisa PUERTO en esp32.py). El instrumento no funciona sin él.")
        return

    # El teclado suena en segundo plano mientras se graba, se conversa o se reproduce el proyecto.
    reproducir.elegir_instrumento(esp32.instrumento_teclado)
    threading.Thread(target=tocar_teclado, daemon=True).start()

    # Python le indica al ESP32 qué pantalla mostrar, porque los datos guardados están en la computadora.
    config = cargar_config()
    if config is None:
        config = primer_inicio()
    else:
        gem.voz_actual = config["voz"]
    esp32.enviar(f"PANTALLA:INICIO:{proyectos.sin_tildes(config['usuario'])}")
    gem.leer(f"[warmly] Hola, {config['usuario']}. ¿Qué vamos a componer hoy?", expresar=False)

    try:
        while True:
            pedido = esp32.leer_pedido()
            if pedido is not None:
                atender(pedido, config)
    except KeyboardInterrupt:
        print("Saliendo...")
    finally:
        if proyecto is not None:
            proyectos.guardar(proyecto, nombre)
        reproducir.cerrar()


def atender(pedido, config):
    """Hace lo que se pidió desde el ESP32 (botones y opciones del menú)."""
    global proyecto, nombre
    tipo = pedido[0]

    # ----- Pantalla de inicio -----
    if tipo == "NUEVO":
        nombre = proyectos.preguntar_nombre_proyecto()   # "¿Deseas ponerle un nombre al proyecto?"
        proyecto = proyectos.nuevo()
        proyectos.guardar(proyecto, nombre)
        abrir_pantalla_principal()

    elif tipo == "ABRIR":
        nombres = proyectos.listar()
        if pedido[1] < len(nombres):
            nombre = nombres[pedido[1]]
            proyecto = proyectos.abrir(nombre)
            abrir_pantalla_principal()
            reproducir.reproducir_proyecto(proyecto)

    elif tipo == "RENOMBRAR":
        nombres = proyectos.listar()
        if pedido[1] < len(nombres):
            nuevo = proyectos.limpiar_nombre(proyectos.pedir_nombre("proyecto"))
            proyectos.renombrar(nombres[pedido[1]], nuevo)
            esp32.enviar("LISTA:" + ",".join(proyectos.listar()))   # vuelve a Proyectos

    elif tipo == "VOZ":   # Ajustes > Voz de la IA (esp32.py ya cambió gem.voz_actual)
        config["voz"] = pedido[1]
        guardar_config(config["usuario"], config["voz"])
        gem.leer("[cheerfully] ¡Hola! Así suena mi nueva voz.", expresar=False)

    # ----- Dentro de un proyecto -----
    elif proyecto is None:
        return   # el resto de los pedidos solo sirven con un proyecto abierto

    elif tipo == "BOTON":
        atender_boton(pedido)

    elif tipo == "PLAY":
        reproducir.reproducir_proyecto(proyecto)

    elif tipo == "MOVER":   # MOVER:canal:tiempos
        proyectos.mover_pista(proyecto, pedido[1], pedido[2])
        proyectos.guardar(proyecto, nombre)

    elif tipo == "PISTA":   # PISTA:canal:accion[:valor]
        atender_pista(pedido[1], pedido[2], pedido[3])

    elif tipo == "CERRAR":  # Ajustes > Volver al inicio
        proyectos.guardar(proyecto, nombre)
        proyecto = None


def abrir_pantalla_principal():
    enviar_pistas()
    esp32.enviar(f"PANTALLA:PRINCIPAL:{nombre}")


def enviar_pistas():
    """Le manda al ESP32 los canales de las pistas del proyecto, por ejemplo PISTAS:0,1,2."""
    esp32.enviar("PISTAS:" + ",".join(str(canal) for canal in proyectos.canales(proyecto)))


def agregar_pista(pista):
    """Pone la pista en el proyecto (reemplaza la que había en ese canal) y guarda."""
    proyectos.poner_pista(proyecto, pista)
    proyectos.guardar(proyecto, nombre)   # se guarda cada vez que se agrega una pista
    enviar_pistas()


# =====================================================================
#  Botones de grabación
# =====================================================================

def atender_boton(pedido):
    global grabacion_teclado, inicio_teclado
    _, boton, encendido = pedido

    if boton == "teclado":
        # El teclado sigue sonando como siempre; mientras tanto se anotan las notas con su tiempo.
        if encendido:
            inicio_teclado = time.time()
            grabacion_teclado = []
        elif grabacion_teclado is not None:
            guardar_pista_teclado()

    elif encendido:
        # grabar.py espera el toque que empieza la grabación: se le devuelve el que ya llegó.
        esp32.devolver(pedido)
        tipo = grabar.grabar()   # graba hasta que se vuelve a tocar el mismo botón
        if tipo == "melodia":
            procesar_melodia()
        elif tipo == "chat":
            procesar_chat()


def procesar_melodia():
    global ultima_propuesta
    archivo_midi = convertir.conversion("grabacion_melodia.wav")   # Basic Pitch
    tonalidad = correccion.detectar_tonalidad(archivo_midi)        # music21
    correccion.corregir(archivo_midi, tonalidad)                   # afina las notas dentro de la escala

    # La pista se crea con el instrumento elegido en ese momento para el micrófono.
    pista = proyectos.midi_a_pista(archivo_midi, CANAL_MELODIA, esp32.instrumento_microfono,
                                   proyecto.ticks_per_beat)
    agregar_pista(pista)
    reproducir.reproducir_proyecto(proyecto)

    ultima_propuesta = gem.analizar_midi(correccion.midi_txt(archivo_midi), tonalidad)


def procesar_chat():
    global ultima_propuesta
    resultado = gem.responder("grabacion_chat.wav")
    if resultado is None:
        return   # no se entendió nada
    mensaje, respuesta = resultado

    # Si el usuario está de acuerdo con la idea que propuso la IA, se arma y se reproduce.
    if ultima_propuesta and gem.acepto_sugerencia(ultima_propuesta, mensaje):
        generar_sugerencia(ultima_propuesta)
        ultima_propuesta = ""
    else:
        ultima_propuesta = respuesta


def generar_sugerencia(idea):
    """Gemini decide qué tocar (JSON con acordes) y sugerencia.py arma la pista y la propone."""
    try:
        archivo = os.path.join(proyectos.CARPETA, nombre + ".mid")
        datos = gem.pedir_sugerencia_json(idea, correccion.midi_txt(archivo),
                                          correccion.detectar_tonalidad(archivo))
        sugerencia.proponer_sugerencia(datos, proyecto, nombre)   # "¿Aceptar sugerencia?" Sí / No
        enviar_pistas()
    except Exception as e:
        print(f"No se pudo generar la sugerencia: {e}")
        esp32.enviar("PANTALLA:PRINCIPAL")
        gem.leer("Perdón, no pude preparar la sugerencia. Probemos de nuevo en un momento.", expresar=False)


# =====================================================================
#  Opciones de una pista (Pistas > pista elegida)
# =====================================================================

def atender_pista(canal, accion, valor):
    if accion == "SOLA":       # Reproducir sola
        reproducir.reproducir_pista(proyecto, canal)

    elif accion == "INST" and valor is not None:   # Instrumento
        proyectos.cambiar_instrumento(proyecto, canal, valor)
        proyectos.guardar(proyecto, nombre)

    elif accion == "IA":       # Generar con IA
        generar_sugerencia(f"Un acompañamiento de acordes para {NOMBRE_PISTA.get(canal, 'esta pista')}")

    elif accion == "BORRAR":   # Borrar
        proyectos.borrar_pista(proyecto, canal)
        proyectos.guardar(proyecto, nombre)
        enviar_pistas()


# =====================================================================
#  Teclado
# =====================================================================

def tocar_teclado():
    """Corre en su propio hilo: cada tecla suena al instante con el velocity que midió el ESP32."""
    while True:
        evento = esp32.leer_tecla()
        if evento is None:
            continue
        tipo, nota, velocidad = evento
        if tipo == "on":
            reproducir.tocar_nota(nota, velocidad)
        else:
            reproducir.soltar_nota(nota)

        grabacion = grabacion_teclado   # copia local: el hilo principal la puede cambiar a None
        if grabacion is not None:
            grabacion.append((time.time() - inicio_teclado, tipo, nota, velocidad))


def guardar_pista_teclado():
    global grabacion_teclado
    eventos, grabacion_teclado = grabacion_teclado, None
    if not eventos:
        return   # no se tocó nada
    pista = proyectos.teclado_a_pista(eventos, CANAL_TECLADO, esp32.instrumento_teclado,
                                      proyecto.ticks_per_beat)
    agregar_pista(pista)


if __name__ == "__main__":
    main()
