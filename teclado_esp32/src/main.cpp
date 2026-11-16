#include <Arduino.h>
#include <TFT_eSPI.h>
TFT_eSPI tft;

// =====================================================================
//  Pines
// =====================================================================
// Teclado (cadena de 74HC165)
const int pinPL = 27;
const int pinCLK = 26;
const int pinData = 25;
// Joystick
#define VRX_PIN 34
#define VRY_PIN 35
#define SW_PIN  32
// Botones de control (a GND, con pull-up interno: presionado = LOW)
#define BOTON_MELODIA_PIN 33   // grabar melodía
#define BOTON_CHAT_PIN    14   // hablar con la IA
#define BOTON_TECLADO_PIN 13   // grabar lo tocado en el piano
// Potenciómetro (ADC1, solo entrada)
#define POT_PIN 36

// =====================================================================
//  Ajustes
// =====================================================================
// ---------- Teclado ----------
// Cada tecla usa 2 entradas: D(2n) = switch de arriba (se activa primero),
// D(2n+1) = switch de abajo (se activa al llegar al fondo). El tiempo entre los dos da el velocity.
const int NUM_CHIPS = 3;                     // una octava: 12 teclas x 2 switches = 24 entradas (máx. 4 chips)
const int NUM_TECLAS = NUM_CHIPS * 4;        // 8 entradas por chip / 2 switches por tecla
const unsigned long K_VELOCITY = 2000;       // velocity = 127 - tiempo / K (tiempo en microsegundos)
const unsigned long ANTIRREBOTE_US = 10000;  // el switch de arriba tiene que estar abierto este tiempo para contar como soltada
const bool DEPURAR_VELOCITY = false;         // true = imprime el tiempo medido, sirve para ajustar K_VELOCITY

// ---------- Joystick ----------
const int ZONA_MUERTA = 500;                    // margen alrededor del centro que se ignora
const unsigned long PERIODO_JOYSTICK   = 20;    // ms entre lecturas del joystick (también hace de antirrebote del botón)
const unsigned long ESPERA_REPETICION  = 500;   // ms manteniendo antes de que empiece a avanzar solo
const unsigned long PERIODO_REPETICION = 100;   // ms entre pasos mientras se mantiene
const bool INVERTIR_X = false;   // cambiar si izquierda/derecha quedan al revés
const bool INVERTIR_Y = true;    // cambiar si arriba/abajo quedan al revés

// ---------- Botones de control y potenciómetro ----------
const unsigned long ANTIRREBOTE_BOTON_MS = 50;
const unsigned long PERIODO_POT = 20;   // ms entre lecturas del potenciómetro
// false si el potenciómetro no está conectado: el GPIO36 no tiene pull-up interno y,
// al aire, mandaría valores al azar (el volumen subiría y bajaría solo).
const bool HAY_POTENCIOMETRO = false;

// ---------- Diseño de la pantalla (480x320 horizontal) ----------
// Listas (menús con desplazamiento)
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
// Pantalla principal del proyecto
const int AJUSTES_X = 366, AJUSTES_Y = 6, AJUSTES_ANCHO = 108, AJUSTES_ALTO = 40;   // arriba a la derecha
const int CAJA_Y = 56, CAJA_ALTO = 184, CAJA_ANCHO = 225;                           // los dos recuadros del medio
const int CAJA_INSTRUMENTOS_X = 10, CAJA_PISTAS_X = 245;
const int ABAJO_Y = 250, ABAJO_ALTO = 62;                                           // barra de abajo
const int REPRODUCIR_X = 195, REPRODUCIR_ANCHO = 90;                                // en el medio
const int EFECTOS_X = 366, EFECTOS_ANCHO = 108;                                     // abajo a la derecha

// =====================================================================
//  Textos
// =====================================================================
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

// Las fuentes de la pantalla no tienen tildes ni signos de apertura (¿ ¡), por eso no se usan.
const char* const VOCES[4] = { "Maya", "Antonio", "Sol", "Mario" };
const char* const OPCIONES_INICIO[2] = { "Crear nuevo proyecto", "Proyectos" };
const char* const OPCIONES_DE_PISTA[6] = {
  "Reproducir sola", "Instrumento", "Mover en el tiempo", "Efectos", "Generar con IA", "Borrar"
};
const char* const NOMBRE_PERILLA[3] = { "Volumen general", "Reverb", "Chorus" };
const char* const CORTO_PERILLA[3]  = { "Volumen", "Reverb", "Chorus" };   // para el botón Efectos
const char* const CODIGO_PERILLA[3] = { "VOL", "REV", "CHO" };              // lo que va en la línea POT: por serial
const char* const CODIGO_MODO[2]    = { "MIC", "TEC" };                     // lo que va en la línea SEL: por serial

// =====================================================================
//  Estado
// =====================================================================
enum Direccion { NINGUNA, ARRIBA, ABAJO, IZQUIERDA, DERECHA };

// Todas las pantallas. Las del proyecto van al final: dentroDelProyecto() compara con PRINCIPAL.
enum Pantalla {
  ESPERANDO,                                     // mensaje mientras trabaja Python
  BIENVENIDA, ELEGIR_VOZ,                        // primer inicio
  MICROFONO, CONFIRMAR_NOMBRE,                   // decir un nombre (usuario o proyecto) y confirmarlo
  INICIO, LISTA_PROYECTOS, OPCIONES_PROYECTO,    // pantalla de inicio
  PREGUNTA_NOMBRE,                               // "Deseas ponerle un nombre al proyecto?"
  PRINCIPAL, MENU_INSTRUMENTOS, LISTA_INSTRUMENTOS, PISTAS, OPCIONES_PISTA,   // dentro del proyecto
  EFECTOS_PISTA, MOVER_PISTA, AJUSTES, EFECTOS, SUGERENCIA
};
enum Foco { FOCO_LISTA, FOCO_VOLVER };                                          // en las listas
enum FocoPrincipal { F_AJUSTES, F_INSTRUMENTOS, F_PISTAS, F_REPRODUCIR, F_EFECTOS };   // en la pantalla principal
enum ModoPerilla { VOLUMEN, REVERB, CHORUS };                                   // qué controla la perilla
const int MODO_MIC = 0, MODO_TEC = 1, MODO_PISTA = 2;                           // para qué es la lista de instrumentos

Pantalla pantalla = ESPERANDO;
int centroX = 2048;
int centroY = 2048;

// ---------- Listas ----------
Foco foco = FOCO_LISTA;        // la lista o el botón de volver
int indice = 0;                // opción marcada
int primeraVisible = 0;        // opción que aparece en la primera fila de la pantalla

// ---------- Datos que manda Python ----------
const int MAX_PROYECTOS = 20;
const int MAX_PISTAS = 16;
String usuario = "";
String proyectoActual = "";
String proyectos[MAX_PROYECTOS];
int cantidadProyectos = 0;
int pistas[MAX_PISTAS];        // canal MIDI de cada pista del proyecto abierto
String nombresPistas[MAX_PISTAS];   // nombre de cada pista, en el mismo orden ("Melodia 2")
int cantidadPistas = 0;
String nombreTranscripto = "";

// ---------- Lo que se eligió en los menús ----------
bool primerInicio = false;     // true mientras se hace el primer inicio
int vozElegida = -1;           // -1 = no se eligió en esta sesión
int modo = MODO_MIC;
int seleccionado[2] = { 0, 0 };   // instrumento del micrófono y del teclado (0 = piano, el predeterminado)
int proyectoMarcado = 0;       // posición en la lista de proyectos
int pistaMarcada = 0;          // canal de la pista elegida
int opcionInicio = 0;          // 0 = Crear nuevo proyecto, 1 = Proyectos
int opcionSiNo = 0;            // 0 = Si, 1 = No
FocoPrincipal focoPrincipal = F_INSTRUMENTOS;
int desplazamiento = 0;        // tiempos que se movió la pista en la pantalla Mover

// ---------- Perilla ----------
ModoPerilla modoPerilla = VOLUMEN;   // arranca en volumen (predeterminado); lo cambia el menú Efectos
int canalPerilla = -1;               // -1 = todo el proyecto; si no, solo esa pista (Efectos de una pista)
int potAnterior = -1;
unsigned long ultimaLecturaPot = 0;

// ---------- Grabación ----------
// Cada toque del botón invierte su estado (no hace falta mantenerlo presionado).
bool grabandoMelodia = false;
bool grabandoChat = false;
bool grabandoTeclado = false;

// ---------- Teclado ----------
enum EstadoTecla { SUELTA, BAJANDO, APRETADA };
struct Tecla {
  EstadoTecla estado = SUELTA;
  unsigned long t1 = 0;             // micros() cuando se activó el switch de arriba
  unsigned long ultimoCerrado = 0;  // micros() de la última vez que se vio activado el switch de arriba
};
Tecla teclas[NUM_TECLAS];

// ---------- Lectura de lo que manda Python ----------
String entrada = "";

// Funciones que se usan antes de estar escritas
void abrirLista(Pantalla p, int marcado);
void abrirBienvenida();
void abrirInicio();
void abrirPrincipal(bool reiniciarFoco);
void abrirMicrofono();
void abrirSiNo(Pantalla p);
void abrirMover();
void mostrarMensaje(const char* texto);
void dibujarElementoPrincipal(FocoPrincipal f);
void dibujarGrabacion();
void pantallaMicrofono(bool grabando);

// =====================================================================
//  Teclado
// =====================================================================

// Lee toda la cadena de 74HC165. Bit N del resultado = entrada N (1 = switch activado).
// El chip conectado directo al ESP32 da las entradas 0-7, el siguiente 8-15, etc.
// La tecla T usa los bits 2T (arriba) y 2T+1 (abajo).
uint32_t leerTeclas() {
  digitalWrite(pinPL, LOW);      // "foto" de todas las entradas
  delayMicroseconds(5);
  digitalWrite(pinPL, HIGH);     // pasa a modo desplazamiento
  uint32_t estado = 0;
  for (int chip = 0; chip < NUM_CHIPS; chip++) {
    // Los switches dan 0 al presionarse, así que se invierte: 1 = activado.
    estado |= (uint32_t)(byte)~shiftIn(pinData, pinCLK, MSBFIRST) << (8 * chip);
  }
  return estado;
}

// velocity = 127 - (tiempo_medido / k), limitado al rango MIDI de 1 a 127.
int calcularVelocity(unsigned long tiempo) {
  long velocity = 127 - (long)(tiempo / K_VELOCITY);
  return constrain(velocity, 1, 127);
}

// Se llama en cada vuelta del loop, sin espera: cuanto más seguido se lea, más preciso el velocity.
// Por tecla: SUELTA -> (switch de arriba) BAJANDO -> (switch de abajo) APRETADA -> (se suelta) SUELTA
void actualizarTeclado() {
  uint32_t lectura = leerTeclas();
  unsigned long ahora = micros();

  for (int i = 0; i < NUM_TECLAS; i++) {
    bool arriba = lectura & (1UL << (2 * i));
    bool abajo  = lectura & (1UL << (2 * i + 1));
    Tecla &t = teclas[i];

    if (arriba) t.ultimoCerrado = ahora;
    // Recién se considera soltada si el switch de arriba lleva ANTIRREBOTE_US abierto (filtra el rebote).
    bool soltada = !arriba && (ahora - t.ultimoCerrado) >= ANTIRREBOTE_US;

    switch (t.estado) {
      case SUELTA:
        if (arriba) {
          t.estado = BAJANDO;
          t.t1 = ahora;
        }
        break;
      case BAJANDO:
        if (abajo) {
          unsigned long t2 = ahora;
          unsigned long tiempo = t2 - t.t1;   // lo que tardó en recorrer los 2 mm entre los switches
          if (DEPURAR_VELOCITY) Serial.printf("Tecla %d: %lu us\n", i, tiempo);
          Serial.printf("ON:%d:%d\n", i, calcularVelocity(tiempo));   // tecla presionada y su velocity
          t.estado = APRETADA;
        } else if (soltada) {
          t.estado = SUELTA;   // se soltó sin llegar al fondo: no suena
        }
        break;
      case APRETADA:
        if (soltada) {
          Serial.printf("OFF:%d\n", i);   // tecla soltada
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
//  Potenciómetro
// =====================================================================

// Cada mensaje lleva el control y el valor, por ejemplo POT:VOL:90.
// Si la perilla controla el efecto de una sola pista, se agrega su canal: POT:REV:90:2.
void leerPotenciometro() {
  if (millis() - ultimaLecturaPot < PERIODO_POT) return;   // cada 20 ms
  ultimaLecturaPot = millis();

  long suma = 0;
  for (int i = 0; i < 8; i++) suma += analogRead(POT_PIN);
  int valor = map(suma / 8, 0, 4095, 0, 127);

  if (abs(valor - potAnterior) >= 2) {   // solo si cambió de verdad
    potAnterior = valor;
    if (canalPerilla < 0) {
      Serial.printf("POT:%s:%d\n", CODIGO_PERILLA[modoPerilla], valor);
    } else {
      Serial.printf("POT:%s:%d:%d\n", CODIGO_PERILLA[modoPerilla], valor, canalPerilla);
    }
  }
}

// =====================================================================
//  Pantalla: dibujos que se usan en varias pantallas
// =====================================================================

// Botón con bordes redondeados. Marcado = fondo cian (es el que tiene el foco).
void dibujarBoton(int x, int y, int ancho, int alto, const String& texto, bool marcado, int fuente) {
  uint16_t fondo = marcado ? TFT_CYAN : TFT_BLACK;
  tft.fillRoundRect(x, y, ancho, alto, 10, fondo);
  tft.drawRoundRect(x, y, ancho, alto, 10, TFT_CYAN);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(marcado ? TFT_BLACK : TFT_WHITE, fondo);
  tft.drawString(texto, x + ancho / 2, y + alto / 2, fuente);
}

void textoCentrado(const String& texto, int y, uint16_t color, int fuente) {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(texto, tft.width() / 2, y, fuente);
}

// Usa la fuente grande (4) si el texto entra en ese ancho; si no, la chica (2).
int fuenteQueEntra(const String& texto, int ancho) {
  return (tft.textWidth(texto, 4) <= ancho) ? 4 : 2;
}

// Pantalla con un solo mensaje, mientras Python trabaja (abrir un proyecto, pensar una sugerencia...).
void mostrarMensaje(const char* texto) {
  pantalla = ESPERANDO;
  tft.fillScreen(TFT_BLACK);
  textoCentrado(texto, tft.height() / 2, TFT_CYAN, 4);
}

// Nombre de la pista de ese canal: el que mandó Python o, si no mandó ninguno, el del canal.
String nombrePista(int canal) {
  for (int i = 0; i < cantidadPistas; i++) {
    if (pistas[i] == canal && nombresPistas[i].length() > 0) return nombresPistas[i];
  }
  if (canal == 0) return "Melodia";
  if (canal == 1) return "Teclado";
  if (canal == 2) return "Sugerencia IA";
  return String("Pista ") + canal;
}

// =====================================================================
//  Pantalla: listas (voces, proyectos, instrumentos, pistas, ajustes, efectos)
// =====================================================================
// Todas las listas se dibujan y se recorren igual; solo cambian estas cuatro funciones.

bool esLista(Pantalla p) {
  return p == ELEGIR_VOZ || p == LISTA_PROYECTOS || p == OPCIONES_PROYECTO ||
         p == MENU_INSTRUMENTOS || p == LISTA_INSTRUMENTOS || p == PISTAS ||
         p == OPCIONES_PISTA || p == EFECTOS_PISTA || p == AJUSTES || p == EFECTOS;
}

int cantidadOpciones() {
  switch (pantalla) {
    case ELEGIR_VOZ:         return 4;
    case LISTA_PROYECTOS:    return cantidadProyectos + 1;   // + "Nuevo proyecto"
    case OPCIONES_PROYECTO:  return 2;
    case MENU_INSTRUMENTOS:  return 2;
    case LISTA_INSTRUMENTOS: return NUM_INSTRUMENTOS;
    case PISTAS:             return cantidadPistas;
    case OPCIONES_PISTA:     return 6;
    case EFECTOS_PISTA:      return 2;
    case AJUSTES:            return 2;
    case EFECTOS:            return 3;
    default:                 return 0;
  }
}

String textoOpcion(int i) {
  switch (pantalla) {
    case ELEGIR_VOZ:         return VOCES[i];
    case LISTA_PROYECTOS:    return (i == 0) ? String("Nuevo proyecto") : proyectos[i - 1];
    case OPCIONES_PROYECTO:  return (i == 0) ? "Abrir" : "Renombrar";
    case MENU_INSTRUMENTOS:  return String(i == MODO_MIC ? "Microfono: " : "Teclado: ") + INSTRUMENTOS[seleccionado[i]];
    case LISTA_INSTRUMENTOS: return String(i) + ": " + INSTRUMENTOS[i];
    case PISTAS:             return nombrePista(pistas[i]);
    case OPCIONES_PISTA:     return OPCIONES_DE_PISTA[i];
    case EFECTOS_PISTA:      return NOMBRE_PERILLA[i + 1];   // Reverb y Chorus
    case AJUSTES:            return (i == 0) ? "Voz de la IA" : "Volver al inicio";
    case EFECTOS:            return NOMBRE_PERILLA[i];
    default:                 return "";
  }
}

String tituloLista() {
  switch (pantalla) {
    case ELEGIR_VOZ:         return "Elegir voz de la IA";
    case LISTA_PROYECTOS:    return "Proyectos";
    case OPCIONES_PROYECTO:  return proyectos[proyectoMarcado];
    case MENU_INSTRUMENTOS:  return "Instrumentos";
    case LISTA_INSTRUMENTOS:
      if (modo == MODO_PISTA) return String("Instrumento - ") + nombrePista(pistaMarcada);
      return String("Instrumento - ") + (modo == MODO_MIC ? "Microfono" : "Teclado");
    case PISTAS:             return "Pistas";
    case OPCIONES_PISTA:     return nombrePista(pistaMarcada);
    case EFECTOS_PISTA:      return String("Efectos - ") + nombrePista(pistaMarcada);
    case AJUSTES:            return "Ajustes";
    case EFECTOS:            return "Que controla la perilla";
    default:                 return "";
  }
}

// Opción que se marca con un punto verde (la que está elegida ahora), o -1 si no hay.
int opcionElegida() {
  switch (pantalla) {
    case ELEGIR_VOZ:         return vozElegida;
    case LISTA_INSTRUMENTOS: return (modo == MODO_PISTA) ? -1 : seleccionado[modo];
    case EFECTOS_PISTA:      return (canalPerilla == pistaMarcada && modoPerilla != VOLUMEN) ? modoPerilla - 1 : -1;
    case EFECTOS:            return (canalPerilla < 0) ? (int)modoPerilla : -1;
    default:                 return -1;
  }
}

// Dibuja una fila de la lista. Si la opción no está en pantalla, no hace nada.
void dibujarFila(int i) {
  int fila = i - primeraVisible;
  if (i < 0 || i >= cantidadOpciones() || fila < 0 || fila >= FILAS_VISIBLES) return;

  int y = LISTA_Y + fila * FILA_ALTO;
  bool marcado = (i == indice);
  bool elegido = (i == opcionElegida());

  uint16_t fondo = TFT_BLACK;
  if (marcado) fondo = (foco == FOCO_LISTA) ? TFT_CYAN : TFT_DARKGREY;
  uint16_t color = marcado ? TFT_BLACK : (elegido ? TFT_GREEN : TFT_WHITE);

  tft.fillRect(0, y, LISTA_ANCHO, FILA_ALTO, fondo);

  // Si el texto no entra con la fuente grande, se usa una más chica.
  String texto = textoOpcion(i);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(color, fondo);
  tft.drawString(texto, 8, y + FILA_ALTO / 2, fuenteQueEntra(texto, LISTA_ANCHO - 8 - 26));

  // Punto verde = opción elegida actualmente.
  if (elegido) tft.fillCircle(LISTA_ANCHO - 14, y + FILA_ALTO / 2, 6, TFT_GREEN);
}

// Barra de desplazamiento: solo hace falta si hay más opciones que filas.
void dibujarBarra() {
  int n = cantidadOpciones();
  if (n <= FILAS_VISIBLES) return;
  const int alto = FILAS_VISIBLES * FILA_ALTO;
  int altoCursor = alto * FILAS_VISIBLES / n;
  int yCursor = LISTA_Y + (long)(alto - altoCursor) * primeraVisible / (n - FILAS_VISIBLES);
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

// En la lista de voces, avisa a Python qué voz quedó marcada para que reproduzca su muestra.
// Se llama solo cuando el cursor llega a una voz (no en cada vuelta del loop),
// así la muestra suena una vez y no se repite mientras el cursor se queda ahí.
void avisarVozMarcada() {
  if (pantalla == ELEGIR_VOZ && foco == FOCO_LISTA) {
    Serial.printf("MARCA:VOZ:%d\n", indice);
  }
}

// Abre una lista con la opción "marcado" ya marcada.
void abrirLista(Pantalla p, int marcado) {
  pantalla = p;
  int n = cantidadOpciones();
  foco = (n == 0) ? FOCO_VOLVER : FOCO_LISTA;   // lista vacía: solo se puede volver
  indice = constrain(marcado, 0, max(n - 1, 0));
  primeraVisible = constrain(indice - FILAS_VISIBLES / 2, 0, max(n - FILAS_VISIBLES, 0));

  tft.fillScreen(TFT_BLACK);
  String titulo = tituloLista();
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(titulo, 10, 12, fuenteQueEntra(titulo, 460));
  tft.drawFastHLine(0, LISTA_Y - 6, tft.width(), TFT_CYAN);
  if (n == 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Todavia no hay pistas", LISTA_ANCHO / 2, 160, 4);
  }
  dibujarLista();
  dibujarVolver();
  avisarVozMarcada();   // al abrir la lista de voces, suena la que está marcada
}

void cambiarFoco(Foco nuevo) {
  if (nuevo == foco) return;
  foco = nuevo;
  dibujarFila(indice);   // la fila marcada cambia de color según tenga el foco o no
  dibujarVolver();
  avisarVozMarcada();    // volvió del botón de volver a la lista: el cursor llega a una voz
}

// Suma o resta al índice sin salirse de la lista, desplazándola si hace falta.
void moverIndice(int paso) {
  int nuevo = constrain(indice + paso, 0, cantidadOpciones() - 1);
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
  avisarVozMarcada();   // el cursor pasó a otra voz
}

// =====================================================================
//  Pantalla: bienvenida, inicio, preguntas Si / No y micrófono
// =====================================================================

void abrirBienvenida() {
  pantalla = BIENVENIDA;
  tft.fillScreen(TFT_BLACK);
  textoCentrado("Te damos la bienvenida", 80, TFT_CYAN, 4);
  textoCentrado("Instrumento Compositor Musical con IA", 125, TFT_WHITE, 2);
  dibujarBoton(140, 200, 200, 64, "Comenzar", true, 4);
}

void dibujarOpcionInicio(int i) {
  dibujarBoton(70, 100 + i * 100, 340, 70, OPCIONES_INICIO[i], i == opcionInicio, 4);
}

void abrirInicio() {
  pantalla = INICIO;
  opcionInicio = 0;
  tft.fillScreen(TFT_BLACK);
  textoCentrado(String("Hola, ") + usuario, 40, TFT_CYAN, 4);
  dibujarOpcionInicio(0);
  dibujarOpcionInicio(1);
}

void dibujarBotonesSiNo() {
  dibujarBoton(90, 220, 130, 64, "Si", opcionSiNo == 0, 4);
  dibujarBoton(260, 220, 130, 64, "No", opcionSiNo == 1, 4);
}

// La misma pantalla de dos opciones sirve para las tres preguntas.
void abrirSiNo(Pantalla p) {
  pantalla = p;
  opcionSiNo = 0;
  tft.fillScreen(TFT_BLACK);
  if (p == PREGUNTA_NOMBRE) {
    textoCentrado("Deseas ponerle un nombre", 80, TFT_CYAN, 4);
    textoCentrado("al proyecto?", 115, TFT_CYAN, 4);
  } else if (p == CONFIRMAR_NOMBRE) {
    textoCentrado("Es correcto?", 70, TFT_CYAN, 4);
    textoCentrado(nombreTranscripto, 135, TFT_GREEN, fuenteQueEntra(nombreTranscripto, 460));
  } else {   // SUGERENCIA
    textoCentrado("Aceptar sugerencia?", 80, TFT_CYAN, 4);
    textoCentrado("Escucha la sugerencia de la IA", 130, TFT_WHITE, 2);
  }
  dibujarBotonesSiNo();
}

// Dibuja un micrófono centrado en (cx, cy)
void dibujarMicrofono(int cx, int cy, uint16_t color) {
  tft.fillRoundRect(cx - 20, cy - 55, 40, 75, 20, color);  // cápsula
  for (int r = 30; r <= 33; r++)                            // soporte en U, 4 px de grosor
    tft.drawCircleHelper(cx, cy, r, 0x4 | 0x8, color);      // mitad de abajo del círculo
  tft.fillRect(cx - 33, cy - 15, 4, 15, color);            // lado izquierdo de la U
  tft.fillRect(cx + 30, cy - 15, 4, 15, color);            // lado derecho de la U
  tft.fillRect(cx - 3, cy + 33, 6, 20, color);             // pie
  tft.fillRect(cx - 25, cy + 53, 50, 6, color);            // base
}

void pantallaMicrofono(bool grabando) {
  uint16_t color = grabando ? TFT_RED : TFT_WHITE;
  tft.fillScreen(TFT_BLACK);
  dibujarMicrofono(240, 140, color);
  tft.setTextDatum(MC_DATUM);  // texto centrado en el punto indicado
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(grabando ? "Grabando..." : "Presiona el boton de chat para grabar",
                 240, 270, 2);
}

void abrirMicrofono() {
  pantalla = MICROFONO;
  pantallaMicrofono(grabandoChat);
}

// =====================================================================
//  Pantalla principal del proyecto
// =====================================================================

// Recuadro con título. Marcado = borde cian doble; si no, gris.
void dibujarCaja(int x, const char* titulo, bool marcado) {
  uint16_t color = marcado ? TFT_CYAN : TFT_DARKGREY;
  tft.fillRect(x, CAJA_Y, CAJA_ANCHO, CAJA_ALTO, TFT_BLACK);
  tft.drawRoundRect(x, CAJA_Y, CAJA_ANCHO, CAJA_ALTO, 8, color);
  if (marcado) tft.drawRoundRect(x + 1, CAJA_Y + 1, CAJA_ANCHO - 2, CAJA_ALTO - 2, 7, color);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(titulo, x + CAJA_ANCHO / 2, CAJA_Y + 8, 4);
}

// Muestra el instrumento elegido para el micrófono y para el teclado.
void dibujarCajaInstrumentos(bool marcado) {
  dibujarCaja(CAJA_INSTRUMENTOS_X, "Instrumentos", marcado);
  int x = CAJA_INSTRUMENTOS_X + 12;
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Microfono:", x, CAJA_Y + 50, 2);
  tft.drawString("Teclado:", x, CAJA_Y + 110, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(INSTRUMENTOS[seleccionado[MODO_MIC]], x, CAJA_Y + 70, 2);
  tft.drawString(INSTRUMENTOS[seleccionado[MODO_TEC]], x, CAJA_Y + 130, 2);
}

// Muestra las pistas del proyecto (hasta 5; el resto se ve entrando al recuadro).
void dibujarCajaPistas(bool marcado) {
  dibujarCaja(CAJA_PISTAS_X, "Pistas", marcado);
  tft.setTextDatum(TL_DATUM);
  if (cantidadPistas == 0) {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Sin pistas todavia", CAJA_PISTAS_X + 12, CAJA_Y + 50, 2);
    return;
  }
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  for (int i = 0; i < cantidadPistas && i < 5; i++) {
    tft.drawString(nombrePista(pistas[i]), CAJA_PISTAS_X + 12, CAJA_Y + 45 + i * 25, 2);
  }
}

// Botón de reproducir: un triángulo hacia la derecha.
void dibujarReproducir(bool marcado) {
  uint16_t fondo = marcado ? TFT_CYAN : TFT_BLACK;
  tft.fillRoundRect(REPRODUCIR_X, ABAJO_Y, REPRODUCIR_ANCHO, ABAJO_ALTO, 10, fondo);
  tft.drawRoundRect(REPRODUCIR_X, ABAJO_Y, REPRODUCIR_ANCHO, ABAJO_ALTO, 10, TFT_CYAN);
  int cx = REPRODUCIR_X + REPRODUCIR_ANCHO / 2;
  int cy = ABAJO_Y + ABAJO_ALTO / 2;
  tft.fillTriangle(cx - 12, cy - 16, cx - 12, cy + 16, cx + 16, cy, marcado ? TFT_BLACK : TFT_CYAN);
}

// Botón Efectos: abajo muestra qué controla la perilla en este momento.
void dibujarEfectos(bool marcado) {
  uint16_t fondo = marcado ? TFT_CYAN : TFT_BLACK;
  tft.fillRoundRect(EFECTOS_X, ABAJO_Y, EFECTOS_ANCHO, ABAJO_ALTO, 10, fondo);
  tft.drawRoundRect(EFECTOS_X, ABAJO_Y, EFECTOS_ANCHO, ABAJO_ALTO, 10, TFT_CYAN);
  String perilla = CORTO_PERILLA[modoPerilla];
  if (canalPerilla >= 0) perilla += " pista";
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(marcado ? TFT_BLACK : TFT_WHITE, fondo);
  tft.drawString("Efectos", EFECTOS_X + EFECTOS_ANCHO / 2, ABAJO_Y + 20, 2);
  tft.drawString(perilla, EFECTOS_X + EFECTOS_ANCHO / 2, ABAJO_Y + 42, 2);
}

// Abajo a la izquierda: aviso en rojo mientras se graba algo.
void dibujarGrabacion() {
  tft.fillRect(0, ABAJO_Y, REPRODUCIR_X - 5, ABAJO_ALTO, TFT_BLACK);
  const char* que = grabandoMelodia ? "melodia" : grabandoChat ? "chat" : grabandoTeclado ? "teclado" : nullptr;
  if (que == nullptr) return;
  tft.fillCircle(24, ABAJO_Y + ABAJO_ALTO / 2, 9, TFT_RED);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Grabando", 42, ABAJO_Y + 20, 2);
  tft.drawString(que, 42, ABAJO_Y + 42, 2);
}

void dibujarElementoPrincipal(FocoPrincipal f) {
  bool marcado = (f == focoPrincipal);
  switch (f) {
    case F_AJUSTES:      dibujarBoton(AJUSTES_X, AJUSTES_Y, AJUSTES_ANCHO, AJUSTES_ALTO, "Ajustes", marcado, 2); break;
    case F_INSTRUMENTOS: dibujarCajaInstrumentos(marcado); break;
    case F_PISTAS:       dibujarCajaPistas(marcado); break;
    case F_REPRODUCIR:   dibujarReproducir(marcado); break;
    case F_EFECTOS:      dibujarEfectos(marcado); break;
  }
}

// reiniciarFoco = true al entrar a un proyecto; false al volver de un submenú (queda donde estaba).
void abrirPrincipal(bool reiniciarFoco) {
  pantalla = PRINCIPAL;
  if (reiniciarFoco) focoPrincipal = F_INSTRUMENTOS;
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(proyectoActual, 10, AJUSTES_Y + AJUSTES_ALTO / 2, fuenteQueEntra(proyectoActual, AJUSTES_X - 20));
  for (int f = F_AJUSTES; f <= F_EFECTOS; f++) dibujarElementoPrincipal((FocoPrincipal)f);
  dibujarGrabacion();
}

// Izquierda y derecha pasan de un recuadro al otro, arriba va a Ajustes y abajo a la barra de abajo,
// donde izquierda y derecha pasan de reproducir a Efectos.
void moverPrincipal(Direccion dir) {
  FocoPrincipal nuevo = focoPrincipal;
  switch (focoPrincipal) {
    case F_AJUSTES:
      if (dir == ABAJO) nuevo = F_PISTAS;
      break;
    case F_INSTRUMENTOS:
      if (dir == DERECHA) nuevo = F_PISTAS;
      else if (dir == ARRIBA) nuevo = F_AJUSTES;
      else if (dir == ABAJO) nuevo = F_REPRODUCIR;
      break;
    case F_PISTAS:
      if (dir == IZQUIERDA) nuevo = F_INSTRUMENTOS;
      else if (dir == ARRIBA) nuevo = F_AJUSTES;
      else if (dir == ABAJO) nuevo = F_REPRODUCIR;
      break;
    case F_REPRODUCIR:
      if (dir == DERECHA) nuevo = F_EFECTOS;
      else if (dir == ARRIBA) nuevo = F_INSTRUMENTOS;
      break;
    case F_EFECTOS:
      if (dir == IZQUIERDA) nuevo = F_REPRODUCIR;
      else if (dir == ARRIBA) nuevo = F_PISTAS;
      break;
  }
  if (nuevo == focoPrincipal) return;
  FocoPrincipal anterior = focoPrincipal;
  focoPrincipal = nuevo;
  dibujarElementoPrincipal(anterior);   // solo se redibujan los dos elementos que cambian
  dibujarElementoPrincipal(nuevo);
}

void entrarPrincipal() {
  switch (focoPrincipal) {
    case F_AJUSTES:      abrirLista(AJUSTES, 0); break;
    case F_INSTRUMENTOS: abrirLista(MENU_INSTRUMENTOS, 0); break;
    case F_PISTAS:       abrirLista(PISTAS, 0); break;
    case F_REPRODUCIR:   Serial.println("PROY:PLAY"); break;
    case F_EFECTOS:      abrirLista(EFECTOS, (canalPerilla < 0) ? modoPerilla : 0); break;
  }
}

// =====================================================================
//  Pantalla: mover una pista en el tiempo
// =====================================================================

void dibujarDesplazamiento() {
  tft.fillRect(0, 100, tft.width(), 60, TFT_BLACK);
  String texto = String(desplazamiento > 0 ? "+" : "") + desplazamiento + " tiempos";
  textoCentrado(texto, 130, TFT_WHITE, 4);
}

void abrirMover() {
  pantalla = MOVER_PISTA;
  desplazamiento = 0;
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(String("Mover - ") + nombrePista(pistaMarcada), 10, 12, 4);
  tft.drawFastHLine(0, LISTA_Y - 6, tft.width(), TFT_CYAN);
  dibujarDesplazamiento();
  textoCentrado("Izquierda: antes     Derecha: despues", 230, TFT_DARKGREY, 2);
  textoCentrado("Boton del joystick: listo", 260, TFT_DARKGREY, 2);
}

// MOVER:2:1 corre la pista del canal 2 un tiempo más tarde, y MOVER:2:-1 un tiempo antes.
void moverPista(int tiempos) {
  desplazamiento += tiempos;
  Serial.printf("MOVER:%d:%d\n", pistaMarcada, tiempos);
  dibujarDesplazamiento();
}

// =====================================================================
//  Navegación
// =====================================================================

int posicionPista(int canal) {
  for (int i = 0; i < cantidadPistas; i++) {
    if (pistas[i] == canal) return i;
  }
  return 0;
}

void quitarPista(int canal) {
  int i = posicionPista(canal);
  if (cantidadPistas == 0 || pistas[i] != canal) return;
  for (; i < cantidadPistas - 1; i++) {
    pistas[i] = pistas[i + 1];
    nombresPistas[i] = nombresPistas[i + 1];
  }
  cantidadPistas--;
}

void crearProyecto() {
  Serial.println("PROY:NUEVO");
  abrirSiNo(PREGUNTA_NOMBRE);   // mientras tanto la IA pregunta en voz alta
}

void pedirProyectos() {
  Serial.println("PROY:PEDIR");
  mostrarMensaje("Cargando proyectos...");   // se cambia cuando llega LISTA:
}

// canal = -1 para todo el proyecto, o el canal de una pista.
void elegirPerilla(ModoPerilla nuevo, int canal) {
  modoPerilla = nuevo;
  canalPerilla = canal;
  dibujarLista();   // mueve el punto verde
}

void elegirOpcionPista(int i) {
  switch (i) {
    case 0:   // Reproducir sola
      Serial.printf("PISTA:%d:SOLA\n", pistaMarcada);
      break;
    case 1:   // Instrumento
      modo = MODO_PISTA;
      abrirLista(LISTA_INSTRUMENTOS, 0);
      break;
    case 2:   // Mover en el tiempo
      abrirMover();
      break;
    case 3:   // Efectos
      abrirLista(EFECTOS_PISTA, 0);
      break;
    case 4:   // Generar con IA
      Serial.printf("PISTA:%d:IA\n", pistaMarcada);
      mostrarMensaje("La IA esta pensando...");   // Python manda PANTALLA:SUGERENCIA cuando la tiene
      break;
    case 5:   // Borrar
      Serial.printf("PISTA:%d:BORRAR\n", pistaMarcada);
      quitarPista(pistaMarcada);
      abrirLista(PISTAS, 0);
      break;
  }
}

// Se presionó el joystick sobre la opción i de una lista.
void elegirOpcion(int i) {
  int anterior;
  switch (pantalla) {
    case ELEGIR_VOZ:
      anterior = vozElegida;
      vozElegida = i;
      Serial.printf("VOZ:%d\n", i);
      if (primerInicio) {
        abrirMicrofono();   // siguiente paso del primer inicio: decir el nombre
      } else {
        dibujarFila(anterior);
        dibujarFila(i);
      }
      break;
    case LISTA_PROYECTOS:
      if (i == 0) {
        crearProyecto();
      } else {
        proyectoMarcado = i - 1;
        abrirLista(OPCIONES_PROYECTO, 0);
      }
      break;
    case OPCIONES_PROYECTO:
      if (i == 0) {
        Serial.printf("PROY:ABRIR:%d\n", proyectoMarcado);
        mostrarMensaje("Abriendo proyecto...");   // Python manda PANTALLA:PRINCIPAL
      } else {
        Serial.printf("PROY:RENOMBRAR:%d\n", proyectoMarcado);
        abrirMicrofono();
      }
      break;
    case MENU_INSTRUMENTOS:
      modo = i;
      abrirLista(LISTA_INSTRUMENTOS, seleccionado[modo]);
      break;
    case LISTA_INSTRUMENTOS:
      if (modo == MODO_PISTA) {
        Serial.printf("PISTA:%d:INST:%d\n", pistaMarcada, i);
        abrirLista(OPCIONES_PISTA, 1);
      } else {
        anterior = seleccionado[modo];
        seleccionado[modo] = i;
        dibujarFila(anterior);
        dibujarFila(i);
        Serial.printf("SEL:%s:%d\n", CODIGO_MODO[modo], i);   // SEL:MIC:n o SEL:TEC:n
      }
      break;
    case PISTAS:
      pistaMarcada = pistas[i];
      abrirLista(OPCIONES_PISTA, 0);
      break;
    case OPCIONES_PISTA:
      elegirOpcionPista(i);
      break;
    case EFECTOS_PISTA:
      elegirPerilla((ModoPerilla)(i + 1), pistaMarcada);   // Reverb o Chorus de esta pista
      break;
    case AJUSTES:
      if (i == 0) {
        primerInicio = false;
        abrirLista(ELEGIR_VOZ, max(vozElegida, 0));
      } else {
        Serial.println("PROY:CERRAR");   // Python guarda y cierra el proyecto
        abrirInicio();
      }
      break;
    case EFECTOS:
      elegirPerilla((ModoPerilla)i, -1);   // volumen, reverb o chorus de todo el proyecto
      break;
    default:
      break;
  }
}

// Se presionó el botón de volver de una lista.
void volverAtras() {
  switch (pantalla) {
    case ELEGIR_VOZ:         if (primerInicio) abrirBienvenida(); else abrirLista(AJUSTES, 0); break;
    case LISTA_PROYECTOS:    abrirInicio(); break;
    case OPCIONES_PROYECTO:  abrirLista(LISTA_PROYECTOS, proyectoMarcado + 1); break;
    case MENU_INSTRUMENTOS:  abrirPrincipal(false); break;
    case LISTA_INSTRUMENTOS:
      if (modo == MODO_PISTA) abrirLista(OPCIONES_PISTA, 1);
      else abrirLista(MENU_INSTRUMENTOS, modo);
      break;
    case PISTAS:             abrirPrincipal(false); break;
    case OPCIONES_PISTA:     abrirLista(PISTAS, posicionPista(pistaMarcada)); break;
    case EFECTOS_PISTA:      abrirLista(OPCIONES_PISTA, 3); break;
    case AJUSTES:            abrirPrincipal(false); break;
    case EFECTOS:            abrirPrincipal(false); break;
    default:                 break;
  }
}

void responderSiNo(bool si) {
  switch (pantalla) {
    case PREGUNTA_NOMBRE:     // "Deseas ponerle un nombre al proyecto?"
      Serial.println(si ? "NOMBRE:SI" : "NOMBRE:NO");
      if (si) abrirMicrofono();
      else mostrarMensaje("Creando proyecto...");   // Python lo guarda con la fecha y la hora
      break;
    case CONFIRMAR_NOMBRE:    // "Es correcto?"
      Serial.println(si ? "NOMBRE:OK" : "NOMBRE:REPETIR");
      if (si) mostrarMensaje("Guardando...");
      else abrirMicrofono();   // vuelve a grabar
      break;
    case SUGERENCIA:          // "Aceptar sugerencia?"
      Serial.println(si ? "SUG:OK" : "SUG:NO");
      abrirPrincipal(false);
      break;
    default:
      break;
  }
}

bool esSiNo(Pantalla p) {
  return p == PREGUNTA_NOMBRE || p == CONFIRMAR_NOMBRE || p == SUGERENCIA;
}

// Movimiento del joystick: cada pantalla lo usa a su manera.
void moverJoystick(Direccion dir) {
  if (esLista(pantalla)) {
    if (dir == DERECHA) {
      cambiarFoco(FOCO_VOLVER);
    } else if (cantidadOpciones() == 0) {
      return;   // lista vacía: el foco se queda en volver
    } else if (dir == IZQUIERDA) {
      cambiarFoco(FOCO_LISTA);
    } else if (foco == FOCO_VOLVER) {
      cambiarFoco(FOCO_LISTA);   // desde el botón de volver, arriba/abajo regresa a la lista
    } else {
      moverIndice(dir == ABAJO ? +1 : -1);
    }
  } else if (esSiNo(pantalla)) {
    int nueva = (dir == IZQUIERDA) ? 0 : (dir == DERECHA) ? 1 : opcionSiNo;
    if (nueva != opcionSiNo) {
      opcionSiNo = nueva;
      dibujarBotonesSiNo();
    }
  } else if (pantalla == INICIO) {
    int nueva = (dir == ARRIBA) ? 0 : (dir == ABAJO) ? 1 : opcionInicio;
    if (nueva != opcionInicio) {
      opcionInicio = nueva;
      dibujarOpcionInicio(0);
      dibujarOpcionInicio(1);
    }
  } else if (pantalla == PRINCIPAL) {
    moverPrincipal(dir);
  } else if (pantalla == MOVER_PISTA) {
    if (dir == IZQUIERDA) moverPista(-1);
    else if (dir == DERECHA) moverPista(+1);
  }
  // En ESPERANDO, BIENVENIDA y MICROFONO el movimiento no hace nada.
}

// Botón del joystick: entra a la opción elegida, vuelve atrás o confirma.
void presionar() {
  if (esLista(pantalla)) {
    if (foco == FOCO_VOLVER) volverAtras();
    else elegirOpcion(indice);
  } else if (esSiNo(pantalla)) {
    responderSiNo(opcionSiNo == 0);
  } else if (pantalla == BIENVENIDA) {
    primerInicio = true;
    abrirLista(ELEGIR_VOZ, 0);
  } else if (pantalla == INICIO) {
    if (opcionInicio == 0) crearProyecto();
    else pedirProyectos();
  } else if (pantalla == PRINCIPAL) {
    entrarPrincipal();
  } else if (pantalla == MOVER_PISTA) {
    abrirLista(OPCIONES_PISTA, 2);
  }
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
  bool vertical = (dir == ARRIBA || dir == ABAJO);

  if (dir != dirAnterior) {
    // Flanco: se acaba de mover (o cambió de dirección) -> un paso inmediato
    if (dir != NINGUNA) moverJoystick(dir);
    proximaRepeticion = ahora + ESPERA_REPETICION;
  } else if (vertical && esLista(pantalla) && (long)(ahora - proximaRepeticion) >= 0) {
    // Sigue en la misma dirección y ya pasó el tiempo -> repetición (solo arriba/abajo en las listas)
    moverJoystick(dir);
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
//  Mensajes que llegan de Python
// =====================================================================

// LISTA:proyecto_04-11_1830,proyecto_05-11_1530
void separarProyectos(String resto) {
  cantidadProyectos = 0;
  while (resto.length() > 0 && cantidadProyectos < MAX_PROYECTOS) {
    int coma = resto.indexOf(',');
    if (coma == -1) coma = resto.length();
    proyectos[cantidadProyectos++] = resto.substring(0, coma);
    resto = resto.substring(min(coma + 1, (int)resto.length()));
  }
}

// PISTAS:0=Melodia,3=Melodia 2,2=Sugerencia IA (canal y nombre de cada pista del proyecto)
void separarPistas(String resto) {
  cantidadPistas = 0;
  while (resto.length() > 0 && cantidadPistas < MAX_PISTAS) {
    int coma = resto.indexOf(',');
    if (coma == -1) coma = resto.length();
    String parte = resto.substring(0, coma);   // "3=Melodia 2"
    int igual = parte.indexOf('=');
    pistas[cantidadPistas] = parte.toInt();    // toInt() lee solo el número del principio
    nombresPistas[cantidadPistas] = (igual == -1) ? String("") : parte.substring(igual + 1);
    cantidadPistas++;
    resto = resto.substring(min(coma + 1, (int)resto.length()));
  }
}

void procesarLinea(String linea) {
  linea.trim();
  if (linea.startsWith("PANTALLA:")) {
    // PANTALLA:BIENVENIDA / PANTALLA:INICIO:usuario / PANTALLA:PRINCIPAL:proyecto / PANTALLA:SUGERENCIA
    String resto = linea.substring(9);
    int dosPuntos = resto.indexOf(':');
    String cual = (dosPuntos == -1) ? resto : resto.substring(0, dosPuntos);
    String dato = (dosPuntos == -1) ? String("") : resto.substring(dosPuntos + 1);

    if (cual == "BIENVENIDA") {
      abrirBienvenida();
    } else if (cual == "INICIO") {
      if (dato.length() > 0) usuario = dato;
      abrirInicio();
    } else if (cual == "PRINCIPAL") {
      bool proyectoNuevo = dato.length() > 0;   // con nombre = se acaba de abrir un proyecto
      if (proyectoNuevo) proyectoActual = dato;
      abrirPrincipal(proyectoNuevo);
    } else if (cual == "SUGERENCIA") {
      abrirSiNo(SUGERENCIA);
    }
  } else if (linea.startsWith("LISTA:")) {
    separarProyectos(linea.substring(6));
    abrirLista(LISTA_PROYECTOS, 0);
  } else if (linea.startsWith("NOMBRE:")) {
    nombreTranscripto = linea.substring(7);   // nombre transcripto, sin tildes
    abrirSiNo(CONFIRMAR_NOMBRE);
  } else if (linea.startsWith("PISTAS:")) {
    separarPistas(linea.substring(7));
    if (pantalla == PRINCIPAL) dibujarElementoPrincipal(F_PISTAS);
    else if (pantalla == PISTAS) abrirLista(PISTAS, 0);
  }
}

// Lee lo que llega de Python carácter por carácter, sin quedarse esperando la línea completa
// (si esperara, frenaría el barrido de las teclas y se mediría mal la velocity).
void leerDesdePython() {
  while (Serial.available()) {          // solo lee lo que ya llegó
    char c = Serial.read();
    if (c == '\n') {                     // terminó la línea
      procesarLinea(entrada);
      entrada = "";
    } else {
      entrada += c;
    }
  }
}

// =====================================================================
//  Botones de control (grabar melodía, hablar con la IA, grabar el teclado)
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

// Actualiza la pantalla que muestra el estado de la grabación.
void avisarGrabacion() {
  if (pantalla == MICROFONO) pantallaMicrofono(grabandoChat);
  else if (pantalla == PRINCIPAL) dibujarGrabacion();
}

// Cada toque enciende o apaga su grabación y lo avisa por serial:
// G:M1 / G:M0 = melodía, G:C1 / G:C0 = chat, G:T1 / G:T0 = teclado.
// Se graba una sola cosa a la vez. La melodía y el teclado se graban dentro de un proyecto
// (ahí se crean las pistas); el chat también sirve en la pantalla del micrófono, para decir un nombre.
// El botón que está grabando siempre se puede tocar para terminar.
void actualizarBotones() {
  static bool melodiaAnterior = false, chatAnterior = false, tecladoAnterior = false;
  static unsigned long melodiaCambio = 0, chatCambio = 0, tecladoCambio = 0;

  bool libre = !grabandoMelodia && !grabandoChat && !grabandoTeclado;
  bool enProyecto = pantalla >= PRINCIPAL;   // las pantallas del proyecto están al final del enum

  if (seToco(BOTON_MELODIA_PIN, melodiaAnterior, melodiaCambio) && (grabandoMelodia || (libre && enProyecto))) {
    grabandoMelodia = !grabandoMelodia;
    Serial.printf("G:M%d\n", grabandoMelodia ? 1 : 0);
    avisarGrabacion();
  }
  if (seToco(BOTON_CHAT_PIN, chatAnterior, chatCambio) && (grabandoChat || (libre && (enProyecto || pantalla == MICROFONO)))) {
    grabandoChat = !grabandoChat;
    Serial.printf("G:C%d\n", grabandoChat ? 1 : 0);
    avisarGrabacion();
  }
  if (seToco(BOTON_TECLADO_PIN, tecladoAnterior, tecladoCambio) && (grabandoTeclado || (libre && enProyecto))) {
    grabandoTeclado = !grabandoTeclado;
    Serial.printf("G:T%d\n", grabandoTeclado ? 1 : 0);
    avisarGrabacion();
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
  // Con pull-up: si el teclado no está conectado, el pin lee siempre 1 (= ninguna tecla
  // apretada) en vez de quedar al aire y mandar notas falsas. Con los 74HC165 conectados
  // no cambia nada, porque el chip maneja el pin.
  pinMode(pinData, INPUT_PULLUP);
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

  // Botones de control
  pinMode(BOTON_MELODIA_PIN, INPUT_PULLUP);
  pinMode(BOTON_CHAT_PIN, INPUT_PULLUP);
  pinMode(BOTON_TECLADO_PIN, INPUT_PULLUP);

  // Pantalla. Python decide qué mostrar (bienvenida o inicio) porque los datos guardados están en la computadora.
  tft.init();
  tft.setRotation(1);            // horizontal (480x320)
  mostrarMensaje("Esperando a la computadora...");
}

void loop() {
  actualizarTeclado();
  actualizarJoystick();
  actualizarBotones();
  if (HAY_POTENCIOMETRO) leerPotenciometro();
  leerDesdePython();
}
