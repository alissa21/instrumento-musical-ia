#include <Arduino.h>
#include <TFT_eSPI.h>
TFT_eSPI tft;

// ---------- Pines ----------
// Teclado (cadena de 74HC165)
const int pinPL = 27;
const int pinCLK = 26;
const int pinData = 25;
// Joystick
#define VRX_PIN 34
#define VRY_PIN 35
#define SW_PIN  32
// Botones de grabación (a GND, con pull-up interno: presionado = LOW)
#define BOTON_MELODIA_PIN 33
#define BOTON_CHAT_PIN    14

// ---------- Ajustes del teclado ----------
// Cada tecla usa 2 pines: D(2n) = contacto de arriba (cierra primero),
// D(2n+1) = contacto de abajo (cierra al llegar al fondo). El tiempo entre los dos da el velocity.
const int NUM_CHIPS = 2;                     // cantidad de 74HC165 en cadena (máx. 4 -> 16 teclas)
const int NUM_TECLAS = NUM_CHIPS * 4;        // 8 pines por chip / 2 pines por tecla
const unsigned long ANTIRREBOTE_US = 10000;  // el contacto de arriba tiene que estar abierto este tiempo para contar como soltada
const unsigned long T_RAPIDO_US = 2000;      // tiempo entre contactos (o menos) que da velocity 127
const unsigned long T_LENTO_US = 80000;      // tiempo entre contactos (o más) que da velocity 1
const bool DEPURAR_VELOCITY = false;         // true = imprime el tiempo medido, sirve para ajustar T_RAPIDO_US / T_LENTO_US

// ---------- Ajustes del joystick ----------
const int ZONA_MUERTA = 500;                    // margen alrededor del centro que se ignora
const unsigned long PERIODO_JOYSTICK   = 20;    // ms entre lecturas del joystick (también hace de antirrebote del botón)
const unsigned long ESPERA_REPETICION  = 500;   // ms manteniendo antes de que empiece a avanzar solo
const unsigned long PERIODO_REPETICION = 100;   // ms entre pasos mientras se mantiene
const bool INVERTIR_X = false;   // cambiar si izquierda/derecha quedan al revés
const bool INVERTIR_Y = true;    // cambiar si arriba/abajo quedan al revés (true = mismo sentido que la versión anterior)

// ---------- Diseño de la pantalla (480x320 horizontal) ----------
const int LISTA_Y = 50;            // dónde empieza la lista (debajo del título)
const int FILA_ALTO = 32;
const int FILAS_VISIBLES = 8;      // 8 * 32 = 256 px -> la lista termina en y = 306
const int LISTA_ANCHO = 396;
const int BARRA_X = 400;           // barra de desplazamiento a la derecha de la lista
const int BARRA_ANCHO = 5;
const int VOLVER_X = 412;          // botón de volver, abajo a la derecha
const int VOLVER_Y = 262;
const int VOLVER_ANCHO = 62;
const int VOLVER_ALTO = 52;

// ---------- Lista de instrumentos General MIDI (0-127) ----------
// El número de cada posición es el mismo "program" que usa FluidSynth.
const int NUM_INSTRUMENTOS = 128;
const char* const INSTRUMENTOS[NUM_INSTRUMENTOS] = {
  // 0-7 Pianos
  "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano", "Honky-tonk Piano",
  "Electric Piano 1", "Electric Piano 2", "Harpsichord", "Clavinet",
  // 8-15 Percusión cromática
  "Celesta", "Glockenspiel", "Music Box", "Vibraphone",
  "Marimba", "Xylophone", "Tubular Bells", "Dulcimer",
  // 16-23 Órganos
  "Drawbar Organ", "Percussive Organ", "Rock Organ", "Church Organ",
  "Reed Organ", "Accordion", "Harmonica", "Tango Accordion",
  // 24-31 Guitarras
  "Acoustic Guitar (nylon)", "Acoustic Guitar (steel)", "Electric Guitar (jazz)", "Electric Guitar (clean)",
  "Electric Guitar (muted)", "Overdriven Guitar", "Distortion Guitar", "Guitar Harmonics",
  // 32-39 Bajos
  "Acoustic Bass", "Electric Bass (finger)", "Electric Bass (pick)", "Fretless Bass",
  "Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2",
  // 40-47 Cuerdas
  "Violin", "Viola", "Cello", "Contrabass",
  "Tremolo Strings", "Pizzicato Strings", "Orchestral Harp", "Timpani",
  // 48-55 Ensambles
  "String Ensemble 1", "String Ensemble 2", "Synth Strings 1", "Synth Strings 2",
  "Choir Aahs", "Voice Oohs", "Synth Choir", "Orchestra Hit",
  // 56-63 Bronces
  "Trumpet", "Trombone", "Tuba", "Muted Trumpet",
  "French Horn", "Brass Section", "Synth Brass 1", "Synth Brass 2",
  // 64-71 Lengüeta
  "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax",
  "Oboe", "English Horn", "Bassoon", "Clarinet",
  // 72-79 Vientos
  "Piccolo", "Flute", "Recorder", "Pan Flute",
  "Blown Bottle", "Shakuhachi", "Whistle", "Ocarina",
  // 80-87 Synth Lead
  "Lead 1 (square)", "Lead 2 (sawtooth)", "Lead 3 (calliope)", "Lead 4 (chiff)",
  "Lead 5 (charang)", "Lead 6 (voice)", "Lead 7 (fifths)", "Lead 8 (bass + lead)",
  // 88-95 Synth Pad
  "Pad 1 (new age)", "Pad 2 (warm)", "Pad 3 (polysynth)", "Pad 4 (choir)",
  "Pad 5 (bowed)", "Pad 6 (metallic)", "Pad 7 (halo)", "Pad 8 (sweep)",
  // 96-103 Synth FX
  "FX 1 (rain)", "FX 2 (soundtrack)", "FX 3 (crystal)", "FX 4 (atmosphere)",
  "FX 5 (brightness)", "FX 6 (goblins)", "FX 7 (echoes)", "FX 8 (sci-fi)",
  // 104-111 Étnicos
  "Sitar", "Banjo", "Shamisen", "Koto",
  "Kalimba", "Bagpipe", "Fiddle", "Shanai",
  // 112-119 Percusión
  "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock",
  "Taiko Drum", "Melodic Tom", "Synth Drum", "Reverse Cymbal",
  // 120-127 Efectos
  "Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet",
  "Telephone Ring", "Helicopter", "Applause", "Gunshot"
};

// ---------- Estado del teclado ----------
enum EstadoTecla { SUELTA, BAJANDO, APRETADA };
struct Tecla {
  EstadoTecla estado = SUELTA;
  unsigned long inicio = 0;         // micros() cuando cerró el contacto de arriba
  unsigned long ultimoCerrado = 0;  // micros() de la última vez que se vio cerrado el contacto de arriba
};
Tecla teclas[NUM_TECLAS];

// ---------- Estado del menú ----------
enum Direccion { NINGUNA, ARRIBA, ABAJO, IZQUIERDA, DERECHA };
enum Pantalla { MENU_MODO, MENU_INSTRUMENTOS };
enum Modo { MICROFONO = 0, TECLADO = 1 };
enum Foco { FOCO_LISTA, FOCO_VOLVER };

const char* const NOMBRE_MODO[] = { "Microfono", "Teclado" };
const char* const CODIGO_MODO[] = { "MIC", "TEC" };   // lo que va en la línea SEL: por serial

int centroX = 2048;
int centroY = 2048;

Pantalla pantalla = MENU_MODO;
int modo = MICROFONO;          // opción marcada en el primer menú (y en la que se entró)
Foco foco = FOCO_LISTA;        // en el menú de instrumentos: la lista o el botón de volver
int indice = 0;                // instrumento marcado (0-127)
int primeraVisible = 0;        // instrumento que aparece en la primera fila de la pantalla
int seleccionado[2] = { -1, -1 };   // instrumento elegido para cada modo (-1 = ninguno todavía)

// ---------- Estado de la grabación ----------
// Cada toque del botón invierte su estado (no hace falta mantenerlo presionado).
bool grabandoMelodia = false;
bool grabandoChat = false;
const unsigned long ANTIRREBOTE_BOTON_MS = 50;

// =====================================================================
//  Teclado
// =====================================================================

// Lee toda la cadena de 74HC165. Bit N del resultado = pin N (1 = contacto cerrado).
// El chip conectado directo al ESP32 da los pines 0-7, el siguiente 8-15, etc.
// La tecla T usa los bits 2T (arriba) y 2T+1 (abajo): chip 0 -> teclas 0-3, chip 1 -> teclas 4-7.
uint32_t leerTeclas() {
  digitalWrite(pinPL, LOW);
  delayMicroseconds(5);
  digitalWrite(pinPL, HIGH);
  uint32_t estado = 0;
  for (int chip = 0; chip < NUM_CHIPS; chip++) {
    // Los botones dan 0 al presionarse, así que se invierte: 1 = presionada.
    estado |= (uint32_t)(byte)~shiftIn(pinData, pinCLK, MSBFIRST) << (8 * chip);
  }
  return estado;
}

// Convierte el tiempo entre el contacto de arriba y el de abajo en velocity MIDI (1-127).
// Escala logarítmica: así las diferencias entre toques rápidos se notan tanto como entre lentos.
int calcularVelocity(unsigned long dt) {
  if (dt <= T_RAPIDO_US) return 127;
  if (dt >= T_LENTO_US) return 1;
  float x = log((float)dt / T_RAPIDO_US) / log((float)T_LENTO_US / T_RAPIDO_US);   // 0 = rápido, 1 = lento
  return 127 - (int)(x * 126 + 0.5f);
}

// Se llama en cada vuelta del loop, sin espera: cuanto más seguido se lea, más preciso el velocity.
// Por tecla: SUELTA -> (cierra arriba) BAJANDO -> (cierra abajo) APRETADA -> (abre arriba) SUELTA
void actualizarTeclado() {
  uint32_t lectura = leerTeclas();
  unsigned long ahora = micros();

  for (int i = 0; i < NUM_TECLAS; i++) {
    bool arriba = lectura & (1UL << (2 * i));
    bool abajo  = lectura & (1UL << (2 * i + 1));
    Tecla &t = teclas[i];

    if (arriba) t.ultimoCerrado = ahora;
    // Recién se considera soltada si el contacto de arriba lleva ANTIRREBOTE_US abierto (filtra el rebote).
    bool soltada = !arriba && (ahora - t.ultimoCerrado) >= ANTIRREBOTE_US;

    switch (t.estado) {
      case SUELTA:
        if (arriba) {
          t.estado = BAJANDO;
          t.inicio = ahora;
        }
        break;
      case BAJANDO:
        if (abajo) {
          unsigned long dt = ahora - t.inicio;
          if (DEPURAR_VELOCITY) Serial.printf("Tecla %d: %lu us\n", i, dt);
          Serial.printf("ON:%d:%d\n", i, calcularVelocity(dt));   // se presionó (línea para src/esp32.py)
          t.estado = APRETADA;
        } else if (soltada) {
          t.estado = SUELTA;   // se soltó sin llegar al fondo: no suena
        }
        break;
      case APRETADA:
        if (soltada) {
          Serial.printf("OFF:%d\n", i);   // se soltó
          t.estado = SUELTA;
        }
        break;
    }
  }
}

// =====================================================================
//  Joystick
// =====================================================================

// Promedia varias lecturas con el joystick en reposo para saber dónde está su centro real.
int calibrarCentro(int pin) {
  long suma = 0;
  for (int i = 0; i < 32; i++) {
    suma += analogRead(pin);
    delay(2);   // en setup() está bien bloquear: todavía no corre nada más
  }
  return suma / 32;
}

// Devuelve hacia dónde está inclinado el joystick. Las diagonales cuentan como NINGUNA.
Direccion leerDireccion() {
  int x = analogRead(VRX_PIN);
  int y = analogRead(VRY_PIN);

  bool xMovido = abs(x - centroX) > ZONA_MUERTA;
  bool yMovido = abs(y - centroY) > ZONA_MUERTA;

  if (xMovido && !yMovido) {
    bool menor = (x < centroX) != INVERTIR_X;
    return menor ? IZQUIERDA : DERECHA;
  }
  if (yMovido && !xMovido) {
    bool menor = (y < centroY) != INVERTIR_Y;
    return menor ? ARRIBA : ABAJO;
  }
  return NINGUNA;
}

// =====================================================================
//  Pantalla: menú de modo (Microfono / Teclado)
// =====================================================================

void dibujarBotonModo(int m) {
  const int ancho = 300, alto = 70;
  int x = (tft.width() - ancho) / 2;
  int y = 90 + m * 100;
  bool marcado = (m == modo);
  uint16_t fondo = marcado ? TFT_CYAN : TFT_BLACK;
  uint16_t texto = marcado ? TFT_BLACK : TFT_WHITE;

  tft.fillRoundRect(x, y, ancho, alto, 10, fondo);
  tft.drawRoundRect(x, y, ancho, alto, 10, TFT_CYAN);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(texto, fondo);
  tft.drawString(NOMBRE_MODO[m], x + ancho / 2, y + alto / 2, 4);
}

void dibujarMenuModo() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Elegir entrada", tft.width() / 2, 20, 4);
  dibujarBotonModo(MICROFONO);
  dibujarBotonModo(TECLADO);
}

// =====================================================================
//  Pantalla: menú de instrumentos
// =====================================================================

// Dibuja una fila de la lista. Si el instrumento no está en pantalla, no hace nada.
void dibujarFila(int i) {
  int fila = i - primeraVisible;
  if (i < 0 || fila < 0 || fila >= FILAS_VISIBLES) return;

  int y = LISTA_Y + fila * FILA_ALTO;
  bool marcado = (i == indice);
  bool elegido = (i == seleccionado[modo]);

  uint16_t fondo = TFT_BLACK;
  if (marcado) fondo = (foco == FOCO_LISTA) ? TFT_CYAN : TFT_DARKGREY;
  uint16_t texto = marcado ? TFT_BLACK : (elegido ? TFT_GREEN : TFT_WHITE);

  tft.fillRect(0, y, LISTA_ANCHO, FILA_ALTO, fondo);

  // Si el nombre no entra con la fuente grande, se usa una más chica.
  String nombre = String(i) + ": " + INSTRUMENTOS[i];
  const int anchoMaximo = LISTA_ANCHO - 8 - 26;   // deja lugar para el punto de "elegido"
  int fuente = (tft.textWidth(nombre, 4) <= anchoMaximo) ? 4 : 2;

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(texto, fondo);
  tft.drawString(nombre, 8, y + FILA_ALTO / 2, fuente);

  // Punto verde = instrumento elegido actualmente para este modo.
  if (elegido) tft.fillCircle(LISTA_ANCHO - 14, y + FILA_ALTO / 2, 6, TFT_GREEN);
}

void dibujarBarra() {
  const int alto = FILAS_VISIBLES * FILA_ALTO;
  int altoCursor = alto * FILAS_VISIBLES / NUM_INSTRUMENTOS;
  int yCursor = LISTA_Y + (long)(alto - altoCursor) * primeraVisible / (NUM_INSTRUMENTOS - FILAS_VISIBLES);
  tft.fillRect(BARRA_X, LISTA_Y, BARRA_ANCHO, alto, TFT_DARKGREY);
  tft.fillRect(BARRA_X, yCursor, BARRA_ANCHO, altoCursor, TFT_CYAN);
}

void dibujarLista() {
  for (int i = primeraVisible; i < primeraVisible + FILAS_VISIBLES; i++) dibujarFila(i);
  dibujarBarra();
}

// Botón con una flecha hacia la izquierda, en la esquina de abajo a la derecha.
void dibujarVolver() {
  bool marcado = (foco == FOCO_VOLVER);
  uint16_t fondo = marcado ? TFT_CYAN : TFT_BLACK;
  uint16_t flecha = marcado ? TFT_BLACK : TFT_CYAN;

  tft.fillRoundRect(VOLVER_X, VOLVER_Y, VOLVER_ANCHO, VOLVER_ALTO, 8, fondo);
  tft.drawRoundRect(VOLVER_X, VOLVER_Y, VOLVER_ANCHO, VOLVER_ALTO, 8, TFT_CYAN);

  int cx = VOLVER_X + VOLVER_ANCHO / 2;
  int cy = VOLVER_Y + VOLVER_ALTO / 2;
  tft.fillTriangle(cx - 18, cy, cx - 4, cy - 12, cx - 4, cy + 12, flecha);   // punta
  tft.fillRect(cx - 4, cy - 4, 20, 9, flecha);                                // palo
}

void dibujarMenuInstrumentos() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(String("Instrumento - ") + NOMBRE_MODO[modo], 10, 12, 4);
  tft.drawFastHLine(0, LISTA_Y - 6, tft.width(), TFT_CYAN);
  dibujarLista();
  dibujarVolver();
}

// =====================================================================
//  Navegación
// =====================================================================

void entrarAInstrumentos() {
  pantalla = MENU_INSTRUMENTOS;
  foco = FOCO_LISTA;
  // Arranca parado en el instrumento que ya estaba elegido para este modo (o en el 0).
  indice = (seleccionado[modo] >= 0) ? seleccionado[modo] : 0;
  primeraVisible = constrain(indice - FILAS_VISIBLES / 2, 0, NUM_INSTRUMENTOS - FILAS_VISIBLES);
  Serial.printf("Menu de instrumentos para %s\n", NOMBRE_MODO[modo]);
  dibujarMenuInstrumentos();
}

void volverAMenuModo() {
  pantalla = MENU_MODO;
  Serial.println("Menu principal");
  dibujarMenuModo();
}

void cambiarFoco(Foco nuevo) {
  if (nuevo == foco) return;
  foco = nuevo;
  dibujarFila(indice);   // la fila marcada cambia de color según tenga el foco o no
  dibujarVolver();
}

// Suma o resta al índice sin salirse de 0-127, desplazando la lista si hace falta.
void moverIndice(int paso) {
  int nuevo = constrain(indice + paso, 0, NUM_INSTRUMENTOS - 1);
  if (nuevo == indice) return;

  int anterior = indice;
  indice = nuevo;

  int primeraAnterior = primeraVisible;
  if (indice < primeraVisible) primeraVisible = indice;
  else if (indice >= primeraVisible + FILAS_VISIBLES) primeraVisible = indice - FILAS_VISIBLES + 1;

  if (primeraVisible != primeraAnterior) {
    dibujarLista();          // se desplazó: hay que redibujar todas las filas
  } else {
    dibujarFila(anterior);   // solo cambió la marca: alcanza con las dos filas involucradas
    dibujarFila(indice);
  }
}

void moverVertical(int paso) {
  if (pantalla == MENU_MODO) {
    int nuevo = constrain(modo + paso, 0, 1);
    if (nuevo != modo) {
      int anterior = modo;
      modo = nuevo;
      dibujarBotonModo(anterior);
      dibujarBotonModo(modo);
    }
  } else if (foco == FOCO_VOLVER) {
    cambiarFoco(FOCO_LISTA);   // desde el botón de volver, arriba/abajo regresa a la lista
  } else {
    moverIndice(paso);
  }
}

void moverHorizontal(Direccion dir) {
  if (pantalla != MENU_INSTRUMENTOS) return;
  if (dir == DERECHA) cambiarFoco(FOCO_VOLVER);
  else if (dir == IZQUIERDA) cambiarFoco(FOCO_LISTA);
}

void presionar() {
  if (pantalla == MENU_MODO) {
    entrarAInstrumentos();
    return;
  }
  if (foco == FOCO_VOLVER) {
    volverAMenuModo();
    return;
  }

  int anterior = seleccionado[modo];
  seleccionado[modo] = indice;
  dibujarFila(anterior);
  dibujarFila(indice);

  Serial.printf("Seleccionaste (%s): [%d] %s\n", NOMBRE_MODO[modo], indice, INSTRUMENTOS[indice]);
  Serial.printf("SEL:%s:%d\n", CODIGO_MODO[modo], indice);   // línea para Python: SEL:MIC:n o SEL:TEC:n
}

// Se llama en cada vuelta del loop, pero solo trabaja cada PERIODO_JOYSTICK ms.
// No bloquea: si no le toca, sale enseguida y el loop sigue con el teclado.
void actualizarJoystick() {
  static unsigned long ultimaLectura = 0;
  unsigned long ahora = millis();
  if (ahora - ultimaLectura < PERIODO_JOYSTICK) return;
  ultimaLectura = ahora;

  // --- Movimiento ---
  static Direccion dirAnterior = NINGUNA;
  static unsigned long proximaRepeticion = 0;

  Direccion dir = leerDireccion();
  int paso = 0;
  if (dir == ABAJO) paso = +1;
  else if (dir == ARRIBA) paso = -1;

  if (dir != dirAnterior) {
    // Flanco: se acaba de mover (o cambió de dirección) -> un paso inmediato
    if (paso != 0) {
      moverVertical(paso);
      proximaRepeticion = ahora + ESPERA_REPETICION;
    } else if (dir == IZQUIERDA || dir == DERECHA) {
      moverHorizontal(dir);
    }
  } else if (paso != 0 && (long)(ahora - proximaRepeticion) >= 0) {
    // Sigue en la misma dirección y ya pasó el tiempo -> repetición (solo arriba/abajo)
    moverVertical(paso);
    proximaRepeticion = ahora + PERIODO_REPETICION;
  }
  dirAnterior = dir;

  // --- Botón ---
  static bool presionadoAnterior = false;
  bool presionado = digitalRead(SW_PIN) == LOW;
  if (presionado && !presionadoAnterior) presionar();
  presionadoAnterior = presionado;
}

// =====================================================================
//  Botones de grabación
// =====================================================================

// Devuelve true solo en el momento en que se presiona (flanco), ignorando el rebote.
bool seToco(int pin, bool &anterior, unsigned long &ultimoCambio) {
  bool presionado = digitalRead(pin) == LOW;
  unsigned long ahora = millis();
  if (presionado == anterior || ahora - ultimoCambio < ANTIRREBOTE_BOTON_MS) return false;
  anterior = presionado;
  ultimoCambio = ahora;
  return presionado;
}

// Cada toque enciende o apaga su grabación y lo avisa por serial (línea para src/esp32.py):
// G:M1 / G:M0 = melodía encendida / apagada, G:C1 / G:C0 = chat encendido / apagado.
// Mientras una grabación está encendida, el otro botón se ignora (se graba una sola cosa a la vez).
void actualizarBotones() {
  static bool melodiaAnterior = false, chatAnterior = false;
  static unsigned long melodiaCambio = 0, chatCambio = 0;

  if (seToco(BOTON_MELODIA_PIN, melodiaAnterior, melodiaCambio) && !grabandoChat) {
    grabandoMelodia = !grabandoMelodia;
    Serial.printf("G:M%d\n", grabandoMelodia ? 1 : 0);
  }
  if (seToco(BOTON_CHAT_PIN, chatAnterior, chatCambio) && !grabandoMelodia) {
    grabandoChat = !grabandoChat;
    Serial.printf("G:C%d\n", grabandoChat ? 1 : 0);
  }
}

// =====================================================================

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  // Teclado
  pinMode(pinPL, OUTPUT);
  pinMode(pinCLK, OUTPUT);
  pinMode(pinData, INPUT);
  digitalWrite(pinPL, HIGH);
  digitalWrite(pinCLK, LOW);
  // Parte del estado real para no mandar notas falsas al arrancar:
  // una tecla ya apretada queda como APRETADA sin mandar ON.
  uint32_t inicial = leerTeclas();
  for (int i = 0; i < NUM_TECLAS; i++) {
    if (inicial & (1UL << (2 * i))) teclas[i].estado = APRETADA;
  }

  // Joystick. No tocarlo durante el arranque: acá mide su centro.
  pinMode(SW_PIN, INPUT_PULLUP);
  centroX = calibrarCentro(VRX_PIN);
  centroY = calibrarCentro(VRY_PIN);
  Serial.printf("Centro medido: X=%d  Y=%d\n", centroX, centroY);

  // Botones de grabación
  pinMode(BOTON_MELODIA_PIN, INPUT_PULLUP);
  pinMode(BOTON_CHAT_PIN, INPUT_PULLUP);

  // Pantalla
  tft.init();
  tft.setRotation(1);            // horizontal (480x320)
  dibujarMenuModo();
}

void loop() {
  actualizarJoystick();
  actualizarBotones();
  actualizarTeclado();
}
