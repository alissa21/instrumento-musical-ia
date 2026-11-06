import collections
import queue
import threading
import time

import serial

import gem
import proyectos
import reproducir

PUERTO = "COM8"       # el que muestra el Administrador de dispositivos para el CP210x
VELOCIDAD = 115200    # igual que monitor_speed / Serial.begin() del ESP32

_conexion = None
_lock_envio = threading.Lock()   # para que dos hilos no mezclen sus líneas al enviar

# Últimos instrumentos elegidos en la pantalla del ESP32 (0 = piano, el predeterminado)
instrumento_microfono = 0
instrumento_teclado = 0

# Un solo hilo lee el puerto y reparte las líneas:
# - las teclas van a su propia cola, para que el teclado suene al instante;
# - la perilla, el instrumento del teclado, la voz y la lista de proyectos se atienden en el momento;
# - todo lo demás (botones, menús, respuestas Sí / No) son pedidos que atiende main.py en orden.
_teclas = queue.Queue()
_pedidos = queue.Queue()
_apartados = collections.deque()   # pedidos que llegaron mientras se esperaba otra cosa

# ---------- Botones de grabación ----------
_BOTONES = {
    "G:M1": ("melodia", True),
    "G:M0": ("melodia", False),
    "G:C1": ("chat", True),
    "G:C0": ("chat", False),
    "G:T1": ("teclado", True),
    "G:T0": ("teclado", False),
}

# ---------- Respuestas de las pantallas de dos opciones ----------
_SI_NO = {
    "NOMBRE:SI": True,  "NOMBRE:NO": False,   # "¿Deseas ponerle un nombre?"
    "SUG:OK": True,     "SUG:NO": False,      # "¿Aceptar sugerencia?"
}
_CONFIRMACION = {
    "NOMBRE:OK": True,  "NOMBRE:REPETIR": False,   # "¿Es correcto?"
}


def conectar():
    """Abre el puerto una sola vez. Devuelve False si el ESP32 no está conectado."""
    global _conexion
    if _conexion is not None:
        return True
    try:
        # Se configura antes de abrir con DTR/RTS apagados para que el ESP32 no se reinicie al conectar.
        _conexion = serial.Serial()
        _conexion.port = PUERTO
        _conexion.baudrate = VELOCIDAD
        _conexion.timeout = 1
        _conexion.dtr = False
        _conexion.rts = False
        _conexion.open()
    except serial.SerialException as e:
        print(f"No se pudo abrir {PUERTO}: {e}")
        _conexion = None
        return False

    threading.Thread(target=_escuchar, daemon=True).start()
    return True


def enviar(linea):
    """Le manda una línea al ESP32 (por ejemplo PANTALLA:INICIO o LISTA:...)."""
    if _conexion is None:
        return
    with _lock_envio:
        _conexion.write((linea + "\n").encode("utf-8"))


def _es_numero(texto):
    return texto.lstrip("-").isdigit()


def _escuchar():
    """Corre en segundo plano: lee cada línea del ESP32 y la manda a donde corresponde."""
    global instrumento_microfono, instrumento_teclado
    while True:
        try:
            linea = _conexion.readline().decode("utf-8", errors="ignore").strip()
        except serial.SerialException as e:
            print(f"Se perdió la conexión con el ESP32: {e}")
            return
        if not linea:
            continue
        partes = linea.split(":")

        # ----- Teclas: ON:tecla:velocity / OFF:tecla -----
        if partes[0] == "ON" and len(partes) == 3 and partes[1].isdigit() and partes[2].isdigit():
            velocidad = max(1, min(127, int(partes[2])))
            _teclas.put(("on", tecla_a_nota(int(partes[1])), velocidad))
        elif partes[0] == "OFF" and len(partes) == 2 and partes[1].isdigit():
            _teclas.put(("off", tecla_a_nota(int(partes[1])), 0))

        # ----- Botones de grabación: G:M1 / G:C0 / G:T1 ... -----
        elif linea in _BOTONES:
            _pedidos.put(("BOTON",) + _BOTONES[linea])

        # ----- Instrumentos: SEL:MIC:n / SEL:TEC:n -----
        elif partes[0] == "SEL" and len(partes) == 3 and partes[2].isdigit():
            if partes[1] == "MIC":
                instrumento_microfono = int(partes[2])
            elif partes[1] == "TEC":
                instrumento_teclado = int(partes[2])
                reproducir.elegir_instrumento(instrumento_teclado)   # cambia en vivo

        # ----- Perilla: POT:VOL:90 (todo el proyecto) o POT:REV:90:2 (solo la pista del canal 2) -----
        elif partes[0] == "POT" and len(partes) in (3, 4) and all(p.isdigit() for p in partes[2:]):
            canal = int(partes[3]) if len(partes) == 4 else None
            reproducir.aplicar_perilla(partes[1], int(partes[2]), canal)

        # ----- Voz de la IA: VOZ:n -----
        elif partes[0] == "VOZ" and len(partes) == 2 and partes[1].isdigit():
            gem.voz_actual = int(partes[1])
            _pedidos.put(("VOZ", int(partes[1])))   # main.py la guarda en config.json

        # ----- Proyectos -----
        elif linea == "PROY:PEDIR":
            enviar("LISTA:" + ",".join(proyectos.listar()))
        elif linea == "PROY:NUEVO":
            _pedidos.put(("NUEVO",))
        elif linea.startswith("PROY:ABRIR:") and linea[11:].isdigit():
            _pedidos.put(("ABRIR", int(linea[11:])))
        elif linea.startswith("PROY:RENOMBRAR:") and linea[15:].isdigit():
            _pedidos.put(("RENOMBRAR", int(linea[15:])))
        elif linea == "PROY:PLAY":
            _pedidos.put(("PLAY",))
        elif linea == "PROY:CERRAR":
            _pedidos.put(("CERRAR",))

        # ----- Respuestas Sí / No -----
        elif linea in _SI_NO:
            _pedidos.put(("SI_NO", _SI_NO[linea]))
        elif linea in _CONFIRMACION:
            _pedidos.put(("CONFIRMACION", _CONFIRMACION[linea]))

        # ----- Pistas: MOVER:canal:tiempos / PISTA:canal:accion[:valor] -----
        elif partes[0] == "MOVER" and len(partes) == 3 and partes[1].isdigit() and _es_numero(partes[2]):
            _pedidos.put(("MOVER", int(partes[1]), int(partes[2])))
        elif partes[0] == "PISTA" and len(partes) in (3, 4) and partes[1].isdigit():
            valor = int(partes[3]) if len(partes) == 4 and partes[3].isdigit() else None
            _pedidos.put(("PISTA", int(partes[1]), partes[2], valor))

        else:
            print(f"ESP32: {linea}")   # mensajes de depuración del ESP32


# ---------- Pedidos para main.py ----------

def _siguiente(timeout):
    """Primero los pedidos apartados, después los que van llegando."""
    if _apartados:
        return _apartados.popleft()
    try:
        return _pedidos.get(timeout=timeout)
    except queue.Empty:
        return None


def leer_pedido(timeout=1):
    """Devuelve el próximo pedido del ESP32, o None si no llegó nada en ese tiempo."""
    return _siguiente(timeout)


def esperar(tipo, timeout=None):
    """Espera un pedido de ese tipo. Los demás que lleguen mientras tanto se apartan
    y se atienden después, en el mismo orden. Devuelve None si se cumple el timeout."""
    otros = []
    limite = None if timeout is None else time.time() + timeout
    try:
        while True:
            restante = None if limite is None else max(0, limite - time.time())
            pedido = _siguiente(restante)
            if pedido is None:
                return None
            if pedido[0] == tipo:
                return pedido
            otros.append(pedido)
    finally:
        _apartados.extendleft(reversed(otros))


def devolver(pedido):
    """Vuelve a poner un pedido al principio, para que lo lea otra función."""
    _apartados.appendleft(pedido)


def esperar_si_no():
    """"¿Deseas ponerle un nombre?" o "¿Aceptar sugerencia?": True = Sí, False = No."""
    return esperar("SI_NO")[1]


def esperar_confirmacion():
    """"¿Es correcto?": True = Sí, False = No (repetir)."""
    return esperar("CONFIRMACION")[1]


def esperar_voz():
    """Espera que se elija la voz de la IA en la pantalla (0 a 3)."""
    return esperar("VOZ")[1]


def leer_boton():
    """Espera el próximo toque de un botón de grabación.
    Devuelve ("melodia" | "chat" | "teclado", True al encender / False al apagar),
    o None si no llegó nada en 1 s."""
    pedido = esperar("BOTON", timeout=1)
    return None if pedido is None else pedido[1:]


# ---------- Teclado (74HC165) ----------
NOTA_BASE = 60   # tecla 0 = Do central (C4); cada tecla siguiente sube un semitono


def tecla_a_nota(tecla):
    """Traduce el número de tecla que manda el ESP32 a una nota MIDI."""
    return NOTA_BASE + tecla


def leer_tecla():
    """Espera el próximo evento del teclado.
    Devuelve ("on", nota, velocity) al presionar (velocity 1-127), ("off", nota, 0) al soltar,
    o None si no llegó nada en 1 s."""
    try:
        return _teclas.get(timeout=1)
    except queue.Empty:
        return None
