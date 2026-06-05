#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <MFRC522.h>

/* ========= WIFI ========= */
const char* ssid = "Sua_Rede-wifi";
const char* password = "Sua_Senha";

/* ========= SERVER ========= */
ESP8266WebServer server(80);

/* ========= PINAGEM (IGUAL AO CÓDIGO BOM) ========= */
#define SS_PIN     D8
#define RST_PIN    D0
#define RELAY_PIN  D1   // relé ativo em LOW

MFRC522 mfrc522(SS_PIN, RST_PIN);

/* ========= TEMPO ========= */
const unsigned long GATE_TIME = 800;

/* ========= CONTROLE ========= */
bool gateOpen = false;
unsigned long gateTimer = 0;

/* Anti-releitura */
unsigned long lastReadTime = 0;
const unsigned long RFID_COOLDOWN = 1000;

/* UID AUTORIZADO */
byte authorizedUID[] = {0xB3, 0x48, 0xCA, 0x13};
const byte UID_LENGTH = 4;

/* ========= FUNÇÕES ========= */
bool isAuthorized(byte *uid) {
  for (byte i = 0; i < UID_LENGTH; i++) {
    if (uid[i] != authorizedUID[i]) return false;
  }
  return true;
}

void openGate() {
  if (!gateOpen) {
    digitalWrite(RELAY_PIN, LOW);   // ATIVA RELÉ
    gateOpen = true;
    gateTimer = millis();
    Serial.println(">>> PORTÃO ABERTO");
  }
}

/* ========= HUD (INALTERADO) ========= */
String pagina() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Portão Social</title>
<style>
body {
  background: radial-gradient(circle, #0f2027, #203a43, #2c5364);
  height: 100vh;
  margin: 0;
  display: flex;
  justify-content: center;
  align-items: center;
  font-family: Arial, sans-serif;
  color: white;
}
.card {
  background: #111;
  padding: 30px;
  border-radius: 16px;
  width: 300px;
  box-shadow: 0 0 25px rgba(0,255,255,0.25);
  text-align: center;
}
button {
  width: 100%;
  padding: 15px;
  margin-top: 15px;
  font-size: 16px;
  font-weight: bold;
  border: none;
  border-radius: 10px;
  background: linear-gradient(135deg, #00e5ff, #00bcd4);
}
.status {
  margin-top: 15px;
  font-size: 14px;
}
</style>
</head>
<body>
<div class="card">
  <h2>Portão Social</h2>
  <button onclick="fetch('/start')">ABRIR</button>
  <div class="status">Controle Web Ativo</div>
</div>
</body>
</html>
)rawliteral";
}

/* ========= SETUP ========= */
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // relé DESLIGADO

  SPI.begin();
  mfrc522.PCD_Init();

  Serial.println("\nSistema iniciado.");
  Serial.println("Aproxime a tag RFID...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(300);

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", []() {
    server.send(200, "text/html", pagina());
  });

  server.on("/start", []() {
    openGate();
    server.send(200, "text/plain", "OK");
  });

  server.begin();
}

/* ========= LOOP ========= */
void loop() {
  server.handleClient();

  if (gateOpen && millis() - gateTimer >= GATE_TIME) {
    digitalWrite(RELAY_PIN, HIGH); // DESLIGA RELÉ
    gateOpen = false;
    Serial.println(">>> PORTÃO FECHADO");
  }

  if (millis() - lastReadTime < RFID_COOLDOWN) return;

  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  lastReadTime = millis();

  Serial.print("UID lido: ");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print("0x");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    if (i < mfrc522.uid.size - 1) Serial.print(", ");
  }
  Serial.println();

  if (isAuthorized(mfrc522.uid.uidByte)) {
    Serial.println("Acesso autorizado.");
    openGate();
  } else {
    Serial.println("Acesso negado.");
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
