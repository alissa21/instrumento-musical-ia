// Programa de prueba: muestra por serial, en tiempo real, los bits de la cadena de 74HC165
// y el velocity de cada tecla, calculado igual que en main.cpp.
// Imprime los bits cada vez que cambia alguno, sin antirrebote (así se ven los rebotes).
//
// Subir:  pio run -e leer_165 -t upload
// Ver:    pio device monitor -e leer_165
#include <Arduino.h>

// Mismos pines y cantidad de chips que main.cpp
const int pinPL = 27;
const int pinCLK = 26;
const int pinData = 25;
const int NUM_CHIPS = 2;

const bool INVERTIR = true;   // true = 1 es contacto cerrado (como main.cpp); false = bits tal cual salen del chip
const bool MOSTRAR_BITS = true;   // false = solo muestra el velocity (imprimir menos hace la medición más precisa)

// Mismos ajustes de velocity que main.cpp
// Cada tecla usa 2 pines: D(2n) = contacto de arriba, D(2n+1) = contacto de abajo.
const int NUM_TECLAS = NUM_CHIPS * 4;
const unsigned long ANTIRREBOTE_US = 10000;
const unsigned long T_RAPIDO_US = 2000;
const unsigned long T_LENTO_US = 80000;

enum EstadoTecla { SUELTA, BAJANDO, APRETADA };
struct Tecla {
  EstadoTecla estado = SUELTA;
  unsigned long inicio = 0;
  unsigned long ultimoCerrado = 0;
};
Tecla teclas[NUM_TECLAS];

uint32_t anterior = 0;

uint32_t leerChips() {
  digitalWrite(pinPL, LOW);
  delayMicroseconds(5);
  digitalWrite(pinPL, HIGH);
  uint32_t estado = 0;
  for (int chip = 0; chip < NUM_CHIPS; chip++) {
    byte valor = shiftIn(pinData, pinCLK, MSBFIRST);
    if (INVERTIR) valor = ~valor;
    estado |= (uint32_t)valor << (8 * chip);
  }
  return estado;
}

// Ejemplo con 2 chips:  1234 ms | chip0: 00000011 | chip1: 00000000
// Dentro de cada chip el bit de la izquierda es D7 y el de la derecha es D0.
void imprimir(uint32_t estado) {
  Serial.printf("%lu ms", millis());
  for (int chip = 0; chip < NUM_CHIPS; chip++) {
    Serial.printf(" | chip%d: ", chip);
    byte valor = estado >> (8 * chip);
    for (int bit = 7; bit >= 0; bit--) {
      Serial.print((valor >> bit) & 1);
    }
  }
  Serial.println();
}

int calcularVelocity(unsigned long dt) {
  if (dt <= T_RAPIDO_US) return 127;
  if (dt >= T_LENTO_US) return 1;
  float x = log((float)dt / T_RAPIDO_US) / log((float)T_LENTO_US / T_RAPIDO_US);   // 0 = rápido, 1 = lento
  return 127 - (int)(x * 126 + 0.5f);
}

// Por tecla: SUELTA -> (cierra arriba) BAJANDO -> (cierra abajo) APRETADA -> (abre arriba) SUELTA
void actualizarVelocity(uint32_t lectura, unsigned long ahora) {
  for (int i = 0; i < NUM_TECLAS; i++) {
    bool arriba = lectura & (1UL << (2 * i));
    bool abajo  = lectura & (1UL << (2 * i + 1));
    Tecla &t = teclas[i];

    if (arriba) t.ultimoCerrado = ahora;
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
          Serial.printf("Tecla %d: %lu us -> velocity %d\n", i, dt, calcularVelocity(dt));
          t.estado = APRETADA;
        } else if (soltada) {
          t.estado = SUELTA;   // se soltó sin llegar al fondo: no hay velocity
        }
        break;
      case APRETADA:
        if (soltada) {
          Serial.printf("Tecla %d: soltada\n", i);
          t.estado = SUELTA;
        }
        break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  pinMode(pinPL, OUTPUT);
  pinMode(pinCLK, OUTPUT);
  pinMode(pinData, INPUT);
  digitalWrite(pinPL, HIGH);
  digitalWrite(pinCLK, LOW);

  Serial.println("Bits de izquierda a derecha: D7 ... D0");
  anterior = leerChips();
  imprimir(anterior);

  // Una tecla ya apretada al arrancar no cuenta como toque.
  for (int i = 0; i < NUM_TECLAS; i++) {
    if (anterior & (1UL << (2 * i))) teclas[i].estado = APRETADA;
  }
}

void loop() {
  uint32_t lectura = leerChips();
  unsigned long ahora = micros();   // se toma antes de imprimir para no medir el tiempo del serial
  if (lectura != anterior) {
    if (MOSTRAR_BITS) imprimir(lectura);
    anterior = lectura;
  }
  actualizarVelocity(lectura, ahora);
}
