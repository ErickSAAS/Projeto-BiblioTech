// ============================================================
// SISTEMA BIBLIOTECH - ESP32 + RFID + DASHBOARD COMPLETO
// ============================================================

#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WebServer.h>

// Sensores ultrassônicos
#define TRIG_A 13
#define ECHO_A 12
#define TRIG_B 14
#define ECHO_B 27

// RFID
#define RFID_SS 17
#define RFID_RST 22

// LEDs e buzzer
#define LED_VERDE 25
#define LED_VERMELHO 26
#define BUZZER 33

// Configurações
#define DISTANCIA_LIMITE 50
#define LIMITE_PESSOAS 6
#define TIMEOUT_SEQUENCIA 4000

int pessoasNaSala = 0;
int estadoSequencia = 0;
int totalAcessos = 0;

unsigned long tempoInicio = 0;

bool ultimoA = false;
bool ultimoB = false;

String ultimoRfidNome = "Nenhum registro";
String ultimoRfidRA = "---";
String ultimoRfidStatus = "---";

String historico[5] = {
  "Aguardando...",
  "---",
  "---",
  "---",
  "---"
};

// Wi-Fi
const char* ssid = "Max";
const char* password = "18041976g";

WebServer server(80);

// Cartão autorizado
byte cartaoAutorizado[][4] = {
  {0x5C, 0x00, 0xCA, 0x22}
};

String nomesAlunos[] = {
  "Jose"
};

String rasAlunos[] = {
  "26000938"
};

const int TOTAL_AUTORIZADOS = 1;

MFRC522 rfid(RFID_SS, RFID_RST);

// ============================================================
// HTML
// ============================================================

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Bibliotech</title>

<style>
body{
  background:#0f0f0f;
  color:white;
  font-family:Arial;
  text-align:center;
  padding:20px;
}

h1{
  color:#ff8c42;
  letter-spacing:3px;
}

.grid{
  display:grid;
  grid-template-columns:repeat(auto-fit,minmax(250px,1fr));
  gap:20px;
  max-width:1000px;
  margin:auto;
}

.card{
  background:#1f1f1f;
  padding:20px;
  border-radius:15px;
  box-shadow:0 0 15px #000;
}

.numero{
  font-size:60px;
  color:#ff8c42;
  font-weight:bold;
}

.status{
  padding:10px;
  border-radius:20px;
  background:#34c759;
  color:black;
  font-weight:bold;
}

.lotada{
  background:#ff3b30;
  color:white;
}

.info{
  text-align:left;
}

button{
  padding:12px 20px;
  border:none;
  border-radius:10px;
  background:#ff3b30;
  color:white;
  font-size:16px;
  cursor:pointer;
}

button:hover{
  opacity:0.8;
}

ul{
  text-align:left;
}
</style>
</head>

<body>

<h1>BIBLIOTECH</h1>
<p>Dashboard em Tempo Real</p>

<div class="grid">

  <div class="card">
    <h2>Pessoas na Sala</h2>
    <div class="numero" id="pessoas">0</div>
    <div class="status" id="status">SALA VAZIA</div>
  </div>

  <div class="card">
    <h2>Total de Acessos</h2>
    <div class="numero" id="total">0</div>
  </div>

  <div class="card">
    <h2>Último RFID</h2>
    <div class="info">
      <p><b>Nome:</b> <span id="nome">---</span></p>
      <p><b>RA:</b> <span id="ra">---</span></p>
      <p><b>Status:</b> <span id="rfidStatus">---</span></p>
    </div>
  </div>

  <div class="card">
    <h2>Controle</h2>
    <button onclick="resetar()">Resetar contador</button>
  </div>

</div>

<div class="card" style="max-width:1000px;margin:20px auto;">
  <h2>Histórico de Acessos</h2>
  <ul id="historico"></ul>
</div>

<script>
async function atualizar(){

  const resposta = await fetch('/api/status');
  const dados = await resposta.json();

  document.getElementById("pessoas").innerText = dados.pessoas;
  document.getElementById("total").innerText = dados.total_acessos;

  const status = document.getElementById("status");
  status.innerText = dados.status_texto;

  if(dados.pessoas > dados.limite){
    status.className = "status lotada";
  }else{
    status.className = "status";
  }

  document.getElementById("nome").innerText = dados.ultimo_rfid_nome;
  document.getElementById("ra").innerText = dados.ultimo_rfid_ra;
  document.getElementById("rfidStatus").innerText = dados.ultimo_rfid_status;

  let lista = document.getElementById("historico");
  lista.innerHTML = "";

  dados.historico.forEach(item => {
    let li = document.createElement("li");
    li.innerText = item;
    lista.appendChild(li);
  });
}

async function resetar(){
  await fetch('/api/reset');
  atualizar();
}

setInterval(atualizar,1000);
atualizar();
</script>

</body>
</html>
)rawliteral";

// ============================================================
// PROTÓTIPOS
// ============================================================

void alarmeCapacidade();
void alarmAcessoNegado();
void somAcessoLiberado();
void verificarRFID();
void processarSequencia(bool disparouA, bool disparouB);

// ============================================================
// SERVIDOR
// ============================================================

void handleRoot(){
  server.send(200, "text/html", index_html);
}

void handleApiStatus(){

  String statusStr = "SALA VAZIA";

  if(pessoasNaSala > 0 && pessoasNaSala <= LIMITE_PESSOAS){
    statusStr = "SALA OCUPADA";
  }
  else if(pessoasNaSala > LIMITE_PESSOAS){
    statusStr = "SALA LOTADA";
  }

  String json = "{";

  json += "\"pessoas\":" + String(pessoasNaSala) + ",";
  json += "\"limite\":" + String(LIMITE_PESSOAS) + ",";
  json += "\"total_acessos\":" + String(totalAcessos) + ",";
  json += "\"status_texto\":\"" + statusStr + "\",";
  json += "\"ultimo_rfid_nome\":\"" + ultimoRfidNome + "\",";
  json += "\"ultimo_rfid_ra\":\"" + ultimoRfidRA + "\",";
  json += "\"ultimo_rfid_status\":\"" + ultimoRfidStatus + "\",";

  json += "\"historico\":[";

  for(int i = 0; i < 5; i++){
    json += "\"" + historico[i] + "\"";
    if(i < 4){
      json += ",";
    }
  }

  json += "]";
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleReset(){

  pessoasNaSala = 0;
  totalAcessos = 0;

  ultimoRfidNome = "Nenhum registro";
  ultimoRfidRA = "---";
  ultimoRfidStatus = "Sistema resetado";

  for(int i = 0; i < 5; i++){
    historico[i] = "---";
  }

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Resetado");
}

// ============================================================
// SETUP
// ============================================================

void setup(){

  Serial.begin(115200);

  pinMode(TRIG_A, OUTPUT);
  pinMode(ECHO_A, INPUT);

  pinMode(TRIG_B, OUTPUT);
  pinMode(ECHO_B, INPUT);

  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_VERMELHO, LOW);
  digitalWrite(BUZZER, LOW);

  SPI.begin(18, 19, 23, RFID_SS);
  rfid.PCD_Init();

  Serial.println("RFID iniciado");

  Serial.println("Conectando WiFi...");
  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/api/status", handleApiStatus);
  server.on("/api/reset", handleReset);

  server.begin();

  Serial.println("Servidor iniciado");
}

// ============================================================
// ULTRASSÔNICO
// ============================================================

float medirDistancia(int trig, int echo){

  digitalWrite(trig, LOW);
  delayMicroseconds(2);

  digitalWrite(trig, HIGH);
  delayMicroseconds(10);

  digitalWrite(trig, LOW);

  long duracao = pulseIn(echo, HIGH, 30000);

  float distancia = duracao * 0.034 / 2;

  return distancia;
}

bool detectarPresenca(int trig, int echo){

  float distancia = medirDistancia(trig, echo);

  if(distancia > 0 && distancia < DISTANCIA_LIMITE){
    return true;
  }

  return false;
}

bool detectarDisparo(bool presencaAtual, bool &ultimaPresenca){

  bool disparou = false;

  if(presencaAtual == true && ultimaPresenca == false){
    disparou = true;
  }

  ultimaPresenca = presencaAtual;

  return disparou;
}

// ============================================================
// CONTADOR
// ============================================================

void processarSequencia(bool disparouA, bool disparouB){

  unsigned long agora = millis();

  if(estadoSequencia == 0){

    if(disparouA){
      estadoSequencia = 1;
      tempoInicio = agora;
    }

    else if(disparouB){
      estadoSequencia = 2;
      tempoInicio = agora;
    }
  }

  else if(estadoSequencia == 1){

    if(disparouB){

      pessoasNaSala++;

      estadoSequencia = 0;

      if(pessoasNaSala > LIMITE_PESSOAS){
        alarmeCapacidade();
      }
    }

    else if(agora - tempoInicio > TIMEOUT_SEQUENCIA){
      estadoSequencia = 0;
    }
  }

  else if(estadoSequencia == 2){

    if(disparouA){

      if(pessoasNaSala > 0){
        pessoasNaSala--;
      }

      estadoSequencia = 0;
    }

    else if(agora - tempoInicio > TIMEOUT_SEQUENCIA){
      estadoSequencia = 0;
    }
  }
}

// ============================================================
// SONS
// ============================================================

void somAcessoLiberado(){

  tone(BUZZER, 523);
  delay(120);

  tone(BUZZER, 659);
  delay(120);

  tone(BUZZER, 784);
  delay(120);

  tone(BUZZER, 1047);
  delay(200);

  noTone(BUZZER);
}

void alarmAcessoNegado(){

  tone(BUZZER, 400);
  delay(300);

  tone(BUZZER, 250);
  delay(400);

  noTone(BUZZER);
}

void alarmeCapacidade(){

  for(int i = 0; i < 3; i++){

    tone(BUZZER, 1500);
    delay(120);

    tone(BUZZER, 700);
    delay(120);
  }

  noTone(BUZZER);
}

// ============================================================
// RFID
// ============================================================

int buscarCartao(){

  for(int i = 0; i < TOTAL_AUTORIZADOS; i++){

    bool uidCorreto = true;

    for(byte j = 0; j < 4; j++){

      if(rfid.uid.uidByte[j] != cartaoAutorizado[i][j]){
        uidCorreto = false;
        break;
      }
    }

    if(uidCorreto){
      return i;
    }
  }

  return -1;
}

void adicionarHistorico(String texto){

  for(int i = 4; i > 0; i--){
    historico[i] = historico[i - 1];
  }

  historico[0] = texto;
}

void verificarRFID(){

  if(!rfid.PICC_IsNewCardPresent()){
    return;
  }

  if(!rfid.PICC_ReadCardSerial()){
    return;
  }

  int indiceAluno = buscarCartao();

  if(indiceAluno >= 0){

    ultimoRfidNome = nomesAlunos[indiceAluno];
    ultimoRfidRA = rasAlunos[indiceAluno];
    ultimoRfidStatus = "Acesso Liberado";

    totalAcessos++;

    adicionarHistorico("Acesso liberado: " + ultimoRfidNome + " - RA " + ultimoRfidRA);

    digitalWrite(LED_VERDE, HIGH);
    somAcessoLiberado();
    delay(1000);
    digitalWrite(LED_VERDE, LOW);

    Serial.println("Acesso liberado");
  }

  else{

    ultimoRfidNome = "Acesso Negado";
    ultimoRfidRA = "---";
    ultimoRfidStatus = "Cartao nao autorizado";

    adicionarHistorico("Acesso negado");

    digitalWrite(LED_VERMELHO, HIGH);
    alarmAcessoNegado();
    delay(1000);
    digitalWrite(LED_VERMELHO, LOW);

    Serial.println("Acesso negado");
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// ============================================================
// LOOP
// ============================================================

void loop(){

  server.handleClient();

  bool presencaA = detectarPresenca(TRIG_A, ECHO_A);
  bool presencaB = detectarPresenca(TRIG_B, ECHO_B);

  bool disparouA = detectarDisparo(presencaA, ultimoA);
  bool disparouB = detectarDisparo(presencaB, ultimoB);

  processarSequencia(disparouA, disparouB);

  verificarRFID();

  delay(50);
}
