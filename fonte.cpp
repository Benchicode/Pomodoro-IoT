#include <WiFi.h>
#include <HTTPClient.h>

//  Pinos
const int LED_PIN = 2;
const int BUTTON_PIN = 4;
const int BUZZER_PIN = 5;

// Tempos  
const unsigned long WORK_TIME  = 5 * 1000UL; // 25s foco
const unsigned long BREAK_TIME = 5  * 1000UL; // 5s pausa
const unsigned long LONG_BREAK = 25 * 1000UL; // 25s pausa longa

//  Wi-Fi / ThingSpeak 
const char* ssid     = "Wokwi-GUEST";
const char* password = "";           
String apiKey        = "Y3UDYP3S1WZ01RENM"; 
const char* server   = "http://api.thingspeak.com/update";

// Controle do Pomodoro 
bool running = false;
bool lastButton = HIGH;
unsigned long stateStart = 0;
int cycle = 0;

enum Mode { IDLE, WORK, BREAK_, LONG_BREAK_ };
Mode mode = IDLE;

// Função para enviar dados ao ThingSpeak 
void sendToThingSpeak(int stateCode, int cycleNumber) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado, nao enviou para ThingSpeak");
    return;
  }

  HTTPClient http;

  // Monta URL: field1 = estado, field2 = ciclo
  String url = String(server) +
               "?api_key=" + apiKey +
               "&field1=" + String(stateCode) +
               "&field2=" + String(cycleNumber);

  Serial.print("Enviando para ThingSpeak: ");
  Serial.println(url);

  http.begin(url);
  int httpCode = http.GET();   // faz o GET
  if (httpCode > 0) {
    Serial.print("Resposta HTTP: ");
    Serial.println(httpCode);
  } else {
    Serial.print("Erro HTTP: ");
    Serial.println(http.errorToString(httpCode));
  }
  http.end();
}

// Buzzer 
void beep(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(120);
    digitalWrite(BUZZER_PIN, LOW);
    delay(120);
  }
}

// Troca de modo 
void changeMode(Mode newMode) {
  mode = newMode;
  stateStart = millis();

  int stateCode = 0; // 0=IDLE, 1=FOCO, 2=PAUSA CURTA, 3=PAUSA LONGA

  switch (newMode) {
    case WORK:
      digitalWrite(LED_PIN, HIGH);  // LED aceso = foco
      beep(1);
      stateCode = 1;
      break;

    case BREAK_:
      beep(2);
      stateCode = 2;
      break;

    case LONG_BREAK_:
      beep(3);
      stateCode = 3;
      break;

    case IDLE:
      digitalWrite(LED_PIN, LOW);
      stateCode = 0;
      break;
  }

  // Envia o novo estado para o ThingSpeak
  sendToThingSpeak(stateCode, cycle);
}

// Setup 
void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Conecta ao Wi-Fi do Wokwi
  Serial.print("Conectando ao WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// Loop 
void loop() {
  // Leitura do botão (borda de descida)
  bool btn = digitalRead(BUTTON_PIN);

  if (lastButton == HIGH && btn == LOW) {
    running = !running;    // liga/desliga o Pomodoro

    if (!running) {
      changeMode(IDLE);
      cycle = 0;
    } else {
      cycle = 1;
      changeMode(WORK);
    }

    delay(250); // debounce
  }
  lastButton = btn;

  if (!running) return;

  unsigned long now = millis();

  switch (mode) {
    case WORK:
      if (now - stateStart >= WORK_TIME) {
        if (cycle < 4) {
          changeMode(BREAK_);
        } else {
          changeMode(LONG_BREAK_);
        }
      }
      break;

    case BREAK_:
      // LED pisca rápido na pausa curta
      if ((now / 300) % 2 == 0) digitalWrite(LED_PIN, LOW);
      else digitalWrite(LED_PIN, HIGH);

      if (now - stateStart >= BREAK_TIME) {
        cycle++;
        changeMode(WORK);
      }
      break;

    case LONG_BREAK_:
      // LED pisca mais devagar na pausa longa
      if ((now / 600) % 2 == 0) digitalWrite(LED_PIN, LOW);
      else digitalWrite(LED_PIN, HIGH);

      if (now - stateStart >= LONG_BREAK) {
        cycle = 1;
        changeMode(WORK); // recomeça os ciclos
      }
      break;
  }
}