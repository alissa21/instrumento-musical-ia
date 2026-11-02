import queue
import threading

import serial

PUERTO = "COM8"       # el que muestra el Administrador de dispositivos para el CP210x
VELOCIDAD = 115200    # igual que monitor_speed / Serial.begin() del ESP32

_conexion = None

# Últimos instrumentos elegidos en la pantalla del ESP32 (None = todavía no se eligió)
instrumento_microfono = None
instrumento_teclado = None

# Un solo hilo lee el puerto y reparte las líneas, así esperar un instrumento
# y tocar el teclado pueden pasar al mismo tiempo sin robarse líneas.
_nuevo_microfono = threading.Event()
_nuevo_teclado = threading.Event()
_teclas = queue.Queue()


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


def _escuchar():
    """Corre en segundo plano: lee cada línea del ESP32 y la manda a donde corresponde."""
    global instrumento_microfono, instrumento_teclado
    while True:
        try:
            linea = _conexion.readline().decode("utf-8", errors="ignore").strip()
        except serial.SerialException as e:
            print(f"Se perdió la conexión con el ESP32: {e}")
            return

        if linea.startswith("SEL:"):
            # SEL:MIC:n o SEL:TEC:n
            partes = linea.split(":")
            if len(partes) != 3 or not partes[2].isdigit():
                continue
            if partes[1] == "MIC":
                instrumento_microfono = int(partes[2])
                _nuevo_microfono.set()
            elif partes[1] == "TEC":
                instrumento_teclado = int(partes[2])
                _nuevo_teclado.set()
        elif linea.startswith("ON:"):
            # ON:tecla:velocity
            partes = linea.split(":")
            if len(partes) != 3 or not partes[1].isdigit() or not partes[2].isdigit():
                continue
            velocidad = max(1, min(127, int(partes[2])))
            _teclas.put(("on", tecla_a_nota(int(partes[1])), velocidad))
        elif linea.startswith("OFF:") and linea[4:].isdigit():
            _teclas.put(("off", tecla_a_nota(int(linea[4:])), 0))


def esperar_instrumento_microfono():
    """Bloquea hasta que se elija en la pantalla el instrumento del micrófono (0-127), lo guarda y lo devuelve."""
    _nuevo_microfono.clear()   # descarta selecciones hechas antes de preguntar
    _nuevo_microfono.wait()
    return instrumento_microfono


def esperar_instrumento_teclado():
    """Bloquea hasta que se elija en la pantalla el instrumento del teclado (0-127), lo guarda y lo devuelve."""
    _nuevo_teclado.clear()     # descarta selecciones hechas antes de preguntar
    _nuevo_teclado.wait()
    return instrumento_teclado


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
