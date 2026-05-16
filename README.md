# Portão Social Inteligente 🚪

Circuito usando **ESP8266 (NodeMCU)** para automatizar um portão social residencial. O portão pode ser aberto de duas formas: pelo **celular via página HTML** (na rede local) ou aproximando uma **tag RFID** autorizada no módulo MFRC522.

---

## Como funciona ⚙️

**Modo Wi-Fi (Página HTML)**
- O ESP8266 se conecta à sua rede Wi-Fi e sobe um servidor web
- Qualquer dispositivo na mesma rede acessa o IP do ESP e aperta o botão na página
- O ESP aciona o relé por alguns segundos, abrindo o portão

**Modo RFID**
- O módulo MFRC522 lê continuamente por tags
- Se a tag aproximada tiver o UID cadastrado no código → aciona o relé
- Se não estiver cadastrada → acesso negado (nada acontece)

**Relé**
- O relé simula o botão físico do portão (ligado em paralelo com o botão da campainha)
- Fecha o circuito por ~3 segundos e depois abre novamente

---

## Lista de materiais 🛒


<img width="647" height="471" alt="imagem intens_portão" src="https://github.com/user-attachments/assets/3e517b4f-2940-4e66-b2f7-2817d8849146" />


| # | Componente | Qtd |
|---|-----------|-----|
| 1 | NodeMCU ESP8266 | 1x |
| 2 | Módulo RFID MFRC522 | 1x |
| 3 | Relé 5V 10A | 1x |
| 4 | Tag/Cartão RFID 13,56 MHz | 1+ |
| 5 | Cabos jumper | vários |
| 6 | Fonte 5V (ou USB) | 1x |

> ⚠️ O MFRC522 opera em **3,3V** — nunca ligue o VCC dele no 5V, vai queimar o módulo.

---

## Conexões ⚡

### RFID (MFRC522) → ESP8266

| MFRC522 | NodeMCU (ESP8266) |
|---------|-------------------|
| VCC     | 3V3               |
| GND     | GND               |
| RST     | D3 (GPIO0)        |
| SDA(SS) | D4 (GPIO2)        |
| SCK     | D5 (GPIO14)       |
| MISO    | D6 (GPIO12)       |
| MOSI    | D7 (GPIO13)       |

### Relé → ESP8266

| Relé | NodeMCU (ESP8266) |
|------|-------------------|
| VCC  | Vin (5V via USB)  |
| GND  | GND               |
| IN   | D1 (GPIO5)        |

### Relé → Portão

Ligue os fios **COM** e **NO** do relé em paralelo com o botão interno do portão/campainha.  
Quando o relé fechar, vai simular um toque no botão → portão abre.

---

## Configuração inicial 🔧

### 1. Instalar as bibliotecas na Arduino IDE

Vá em **Ferramentas → Gerenciar Bibliotecas** e instale:
- `MFRC522` (by GithubCommunity)
- `ESP8266WiFi` (já vem com o pacote ESP8266)

Para instalar o suporte ao ESP8266, vá em **Arquivo → Preferências** e adicione na URL de gerenciadores de placa:
```
http://arduino.esp8266.com/stable/package_esp8266com_index.json
```

### 2. Descobrir o UID da sua tag

Carregue o código uma vez, abra o **Monitor Serial** (115200 baud) e aproxime a tag.  
O UID vai aparecer no formato: `DE AD BE EF`  
Copie esse valor e substitua em `authorizedUIDs` no código.

---

## Código 💻

```cpp
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <MFRC522.h>

// ─── Configurações Wi-Fi ───────────────────────────────────────────────────
const char* ssid     = "SEU_WIFI";       // Nome da sua rede
const char* password = "SUA_SENHA";      // Senha da sua rede

// ─── Pinos ─────────────────────────────────────────────────────────────────
#define RELAY_PIN D1   // Relé
#define RST_PIN   D3   // Reset do MFRC522
#define SS_PIN    D4   // SDA/CS do MFRC522
// SCK → D5 | MISO → D6 | MOSI → D7 (SPI padrão do NodeMCU)

// ─── Objetos ───────────────────────────────────────────────────────────────
MFRC522 rfid(SS_PIN, RST_PIN);
ESP8266WebServer server(80);

// ─── UIDs autorizados ──────────────────────────────────────────────────────
// Substitua pelos UIDs das suas tags (leia no Monitor Serial)
byte authorizedUIDs[][4] = {
  {0xDE, 0xAD, 0xBE, 0xEF},  // Tag 1 — exemplo, troque pelo seu UID
  // {0xAA, 0xBB, 0xCC, 0xDD}, // Tag 2 — descomente para adicionar mais
};
const int numAuthorized = sizeof(authorizedUIDs) / sizeof(authorizedUIDs[0]);

// ─── Temporizador do relé ──────────────────────────────────────────────────
const unsigned long RELAY_DURATION = 3000; // ms que o portão fica acionado
unsigned long relayTimer = 0;
bool relayActive = false;

// ──────────────────────────────────────────────────────────────────────────
// Funções auxiliares
// ──────────────────────────────────────────────────────────────────────────

void abrirPortao() {
  if (!relayActive) {
    digitalWrite(RELAY_PIN, LOW);  // Ativa relé (lógica invertida: LOW = ON)
    relayActive = true;
    relayTimer  = millis();
    Serial.println("✅ Portão aberto!");
  }
}

bool verificarUID(byte* uid, byte size) {
  if (size != 4) return false;
  for (int i = 0; i < numAuthorized; i++) {
    bool match = true;
    for (int j = 0; j < 4; j++) {
      if (uid[j] != authorizedUIDs[i][j]) { match = false; break; }
    }
    if (match) return true;
  }
  return false;
}

// ─── Página HTML ───────────────────────────────────────────────────────────
String buildHTML() {
  return R"rawhtml(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Portão Social</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: Arial, sans-serif;
      display: flex; flex-direction: column;
      align-items: center; justify-content: center;
      min-height: 100vh;
      background: #f4f4f4;
    }
    .card {
      background: white;
      border-radius: 16px;
      padding: 40px 32px;
      box-shadow: 0 4px 20px rgba(0,0,0,.1);
      text-align: center;
      max-width: 340px;
      width: 90%;
    }
    h1 { font-size: 1.6rem; color: #222; margin-bottom: 8px; }
    p  { color: #666; margin-bottom: 28px; font-size: .95rem; }
    button {
      background: #2ecc71;
      color: white;
      border: none;
      border-radius: 12px;
      padding: 18px 36px;
      font-size: 1.1rem;
      cursor: pointer;
      width: 100%;
      transition: background .2s;
    }
    button:active { background: #27ae60; }
    #status {
      margin-top: 18px;
      font-size: .9rem;
      min-height: 22px;
      color: #2ecc71;
      font-weight: bold;
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>🏠 Portão Social</h1>
    <p>Toque no botão para abrir o portão</p>
    <button onclick="abrir()">🔓 Abrir Portão</button>
    <div id="status"></div>
  </div>
  <script>
    function abrir() {
      fetch('/abrir')
        .then(r => r.text())
        .then(t => {
          document.getElementById('status').innerText = t;
          setTimeout(() => document.getElementById('status').innerText = '', 4000);
        })
        .catch(() => document.getElementById('status').innerText = '❌ Erro de conexão');
    }
  </script>
</body>
</html>
)rawhtml";
}

// ─── Rotas do servidor ─────────────────────────────────────────────────────
void handleRoot()  { server.send(200, "text/html", buildHTML()); }
void handleAbrir() { abrirPortao(); server.send(200, "text/plain", "✅ Portão aberto!"); }
void handleNotFound() { server.send(404, "text/plain", "Não encontrado"); }

// ──────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Portão Social Inteligente ---");

  // Relé
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // HIGH = relé desligado (lógica invertida)

  // SPI + RFID
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("RFID pronto.");

  // Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado!");
  Serial.print("Acesse: http://");
  Serial.println(WiFi.localIP());

  // Servidor
  server.on("/",      handleRoot);
  server.on("/abrir", handleAbrir);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Servidor HTTP iniciado.");
}

// ──────────────────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();

  // Desliga relé após RELAY_DURATION ms
  if (relayActive && millis() - relayTimer >= RELAY_DURATION) {
    digitalWrite(RELAY_PIN, HIGH);
    relayActive = false;
    Serial.println("Relé desligado.");
  }

  // Leitura RFID
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  Serial.print("UID lido: ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.printf("%02X ", rfid.uid.uidByte[i]);
  }
  Serial.println();

  if (verificarUID(rfid.uid.uidByte, rfid.uid.size)) {
    Serial.println("Acesso autorizado!");
    abrirPortao();
  } else {
    Serial.println("Acesso negado!");
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}
```

---

## Como descobrir o UID da sua tag 🔍

1. Grave o código com os campos `ssid` e `password` preenchidos
2. Abra o **Monitor Serial** (115200 baud)
3. Aproxime a tag do sensor
4. Anote o UID exibido (ex: `DE AD BE EF`)
5. Substitua em `authorizedUIDs`:

```cpp
byte authorizedUIDs[][4] = {
  {0xDE, 0xAD, 0xBE, 0xEF},  // sua tag
};
```

6. Regrave e teste

---

## Acessando pelo celular 📱

1. Conecte o celular na **mesma rede Wi-Fi** do ESP
2. Abra o Monitor Serial e veja o IP (ex: `192.168.1.105`)
3. No navegador do celular, acesse: `http://192.168.1.105`
4. Toque em **Abrir Portão**

> 💡 Dica: no roteador, configure um **IP fixo** para o MAC do ESP8266 para o endereço nunca mudar.

---

## Ligação no portão 🔌

O relé deve ser ligado **em paralelo** com o botão interno do interfone/portão:

```
[Botão do portão] ──── COM ──┐
                              ├── Relé NO
[Fio da campainha] ─────────┘
```

Quando o relé fechar (NO → COM), simula um toque no botão → o portão abre normalmente.

> ⚠️ Se o portão usar tensão CA (110/220V) nos fios do botão, use um relé adequado e tome muito cuidado com isolamento. Nesse projeto o relé 5V 10A suporta até 250V CA.

---

## Considerações 💡

- O ESP8266 e o relé **não ficam acessíveis fora da rede local** — isso é intencional e mais seguro. Para acesso externo, explore o uso de VPN ou MQTT com broker na nuvem.
- Se o relé ligar sozinho ao resetar, é sinal de inversão de lógica — troque `HIGH`/`LOW` na inicialização.
- O MFRC522 é sensível a interferência. Se tiver falhas de leitura, tente um fio mais curto entre o módulo e o ESP.
- Você pode cadastrar quantas tags quiser adicionando linhas em `authorizedUIDs`.

---

## Resultado❗



https://github.com/user-attachments/assets/910cd696-a44d-4dba-87e2-24fd8e2bdc5a


https://github.com/user-attachments/assets/d89ea576-6a1f-497a-8a80-198087d4dd68


### Referências 📚

- [Documentação MFRC522 - GitHub](https://github.com/miguelbalboa/rfid)
- [ESP8266WebServer - Documentação](https://arduino-esp8266.readthedocs.io/en/latest/)
- [Eletrogate - Tutoriais ESP8266](https://blog.eletrogate.com/)
