#include "Seeed_BME280.h"
#include <Wire.h>
#include <TFT_eSPI.h>
#include <SdFat.h>

#define BUTTON_A_PIN WIO_KEY_A  // Cambiar umbral
#define BUTTON_B_PIN WIO_KEY_B  // Disminuir
#define BUTTON_C_PIN WIO_KEY_C  // Aumentar

SdFat SD;
File archivo;

BME280 bme280;
TFT_eSPI tft;

float presionBase;
float umbralDerecho = 300.0;
float umbralIzquierdo = 25.0;
float umbralCentro = 800.0;

bool izquierdo = false;
bool derecho = false;
bool centro = false;

unsigned long tiempoUltimoClickIzquierdo = 0;
unsigned long tiempoUltimoClickDerecho = 0;
unsigned long tiempoUltimoClickCentro = 0;
unsigned long tiempoUltimoEventoGlobal = 0;

const unsigned long debounceClick = 100;
const unsigned long cambioEntreBotones = 500;

int modoCalibracion = 0;
float ultimoUI = -1, ultimoUD = -1, ultimoUC = -1;
int ultimoModo = -1;

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_A_PIN, INPUT);
  pinMode(BUTTON_B_PIN, INPUT);
  pinMode(BUTTON_C_PIN, INPUT);

  if (!bme280.init()) {
    Serial.println("Error BME280");
    while (1);
  }

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.drawString("3A", 150, 10);

  dibujarMouse(TFT_LIGHTGREY, TFT_LIGHTGREY);
  actualizarMiniConfig();

  delay(2000);

if (!SD.begin(SDCARD_SS_PIN)) {
  Serial.println("No se pudo inicializar SD");
} 
else {
  archivo = SD.open("umbrales.txt", FILE_READ);
  if (archivo) {
    umbralIzquierdo = archivo.parseFloat();
    umbralDerecho = archivo.parseFloat();
    umbralCentro = archivo.parseFloat();
    archivo.close();
    Serial.println("Umbrales cargados de SD.");
  }
  else {
    Serial.println("Archivo de umbrales no encontrado. Usando valores por defecto.");
  }
}

  presionBase = bme280.getPressure();
}

void loop() {
  float presionActual = bme280.getPressure();
  float delta = presionActual - presionBase;
  unsigned long ahora = millis();

  bool cambiar = digitalRead(BUTTON_A_PIN) == LOW;
  bool disminuir = digitalRead(BUTTON_B_PIN) == LOW;
  bool aumentar = digitalRead(BUTTON_C_PIN) == LOW;

  if (cambiar) {
    modoCalibracion = (modoCalibracion + 1) % 3;  // 0 -> 1 -> 2 -> 0
    guardarUmbrales();
    delay(250);
  }
  if (disminuir) {
    if (modoCalibracion == 0) umbralIzquierdo = max(0.0, umbralIzquierdo - 5.0);
    else if (modoCalibracion == 1) umbralDerecho = max(5.0, umbralDerecho - 5.0);
    else umbralCentro = max(10.0, umbralCentro - 5.0);
    guardarUmbrales();
    delay(150);
  }
  if (aumentar) {
    if (modoCalibracion == 0) umbralIzquierdo += 5.0;
    else if (modoCalibracion == 1) umbralDerecho += 5.0;
    else umbralCentro += 5.0;
    guardarUmbrales();
    delay(150);
  }

  if (umbralIzquierdo != ultimoUI || umbralDerecho != ultimoUD || umbralCentro != ultimoUC || modoCalibracion != ultimoModo) {
    actualizarMiniConfig();
    ultimoUI = umbralIzquierdo;
    ultimoUD = umbralDerecho;
    ultimoUC = umbralCentro;
    ultimoModo = modoCalibracion;
  }
  // CENTRO
  if (delta > umbralCentro) {
    if (!centro && (ahora - tiempoUltimoEventoGlobal >= cambioEntreBotones)) {
      Serial.println("CENTRO");
      mostrarEstado("CENTRO", TFT_BLUE);
      dibujarMouse(TFT_BLUE, TFT_BLUE);
      centro = true;
      izquierdo = false;
      derecho = false;
      tiempoUltimoClickCentro = ahora;
      tiempoUltimoEventoGlobal = ahora;
    }
  } else if (centro && (ahora - tiempoUltimoClickCentro >= debounceClick)) {
    Serial.println("CENTRO SOLTADO");
    mostrarEstado("SOLTADO", TFT_WHITE);
    dibujarMouse(TFT_LIGHTGREY, TFT_LIGHTGREY);
    centro = false;
    tiempoUltimoClickCentro = ahora;
  }
  // DERECHO
  else if (delta > umbralDerecho && delta <= umbralCentro) {
    if (!derecho && (ahora - tiempoUltimoEventoGlobal >= cambioEntreBotones)) {
      Serial.println("CLICK DERECHO");
      mostrarEstado("CLICK DERECHO", TFT_RED);
      dibujarMouse(TFT_LIGHTGREY, TFT_RED);
      derecho = true;
      izquierdo = false;
      tiempoUltimoClickDerecho = ahora;
      tiempoUltimoEventoGlobal = ahora;
    }
  } else if (derecho && (ahora - tiempoUltimoClickDerecho >= debounceClick)) {
    Serial.println("CLICK DERECHO SOLTADO");
    mostrarEstado("SOLTADO", TFT_WHITE);
    dibujarMouse(TFT_LIGHTGREY, TFT_LIGHTGREY);
    derecho = false;
    tiempoUltimoClickDerecho = ahora;
  }
  // IZQUIERDO
  else if (delta > umbralIzquierdo && delta <= umbralDerecho) {
    if (!izquierdo && !derecho && (ahora - tiempoUltimoEventoGlobal >= cambioEntreBotones)) {
      Serial.println("CLICK IZQUIERDO PRESIONADO");
      mostrarEstado("CLICK IZQUIERDO", TFT_GREEN);
      dibujarMouse(TFT_GREEN, TFT_LIGHTGREY);
      izquierdo = true;
      tiempoUltimoClickIzquierdo = ahora;
      tiempoUltimoEventoGlobal = ahora;
    }
  } else if (izquierdo && (ahora - tiempoUltimoClickIzquierdo >= debounceClick)) {
    Serial.println("CLICK IZQUIERDO SOLTADO");
    mostrarEstado("SOLTADO", TFT_WHITE);
    dibujarMouse(TFT_LIGHTGREY, TFT_LIGHTGREY);
    izquierdo = false;
    tiempoUltimoClickIzquierdo = ahora;
  }

  delay(50);
}

void mostrarEstado(String texto, uint16_t color) {
  tft.fillRect(0, 40, 320, 40, TFT_BLACK);
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextSize(2);
  int textX = (320 - texto.length() * 12) / 2;
  tft.setCursor(textX, 50);
  tft.print(texto);
}

void dibujarMouse(uint16_t colorIzq, uint16_t colorDer) {
  int w = 100, h = 120;
  int x = 180;
  int y = (240 - h) / 2 + 20;

  tft.fillRoundRect(x, y, w, h, 24, TFT_WHITE);
  tft.drawRoundRect(x, y, w, h, 24, TFT_BLACK);

  tft.fillRoundRect(x + 6, y + 6, w / 2 - 12, h / 2 - 8, 10, colorIzq);
  tft.drawRoundRect(x + 6, y + 6, w / 2 - 12, h / 2 - 8, 10, TFT_BLACK);

  tft.fillRoundRect(x + w / 2 + 6, y + 6, w / 2 - 12, h / 2 - 8, 10, colorDer);
  tft.drawRoundRect(x + w / 2 + 6, y + 6, w / 2 - 12, h / 2 - 8, 10, TFT_BLACK);

  tft.fillRoundRect(x + w / 2 - 4, y + h / 2 + 4, 8, 24, 3, TFT_DARKGREY);
}

void actualizarMiniConfig() {
  tft.fillRect(0, 80, 140, 100, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 90);

  tft.print("Umbral I: ");
  tft.setTextColor(modoCalibracion == 0 ? TFT_GREEN : TFT_WHITE);
  tft.print(umbralIzquierdo, 0);
  tft.println(" Pa");

  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 100);
  tft.print("Umbral D: ");
  tft.setTextColor(modoCalibracion == 1 ? TFT_RED : TFT_WHITE);
  tft.print(umbralDerecho, 0);
  tft.println(" Pa");
  
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 110);
  tft.print("Umbral C: ");
  tft.setTextColor(modoCalibracion == 2 ? TFT_BLUE : TFT_WHITE);
  tft.print(umbralCentro, 0);
  tft.println(" Pa");

  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(10, 150);
  tft.println("A: Cambiar");
  tft.setCursor(10, 160);
  tft.println("B: -5");
  tft.setCursor(10, 170);
  tft.println("C: +5");
}

void guardarUmbrales() {
  archivo = SD.open("umbrales.txt", FILE_WRITE);
  if (archivo) {
    archivo.println(umbralIzquierdo);
    archivo.println(umbralDerecho);
    archivo.println(umbralCentro);
    archivo.close();
    Serial.println("Umbrales guardados.");
  } else {
    Serial.println("Error al guardar umbrales.");
  }
}
