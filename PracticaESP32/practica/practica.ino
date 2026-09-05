/*
 * ============================================================
 *  Panel Web básico para ESP32
 *  - Métricas internas del propio ESP32 (RAM, Flash, latencia, temp. del chip)
 *  - Controla un servo mediante un slider en tiempo real
 * ============================================================
 *
 * Librerías necesarias (Arduino IDE > Herramientas > Administrar bibliotecas):
 *   - ESP32Servo   (por Kevin Harrington / John K. Bennett)
 *   - (WebServer.h y WiFi.h ya vienen incluidas en el core de ESP32)
 *
 * Ajusta:
 *   1. SSID / PASSWORD de tu red WiFi
 *   2. Pin del servo
 * ============================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ---------------------- CONFIGURACIÓN ----------------------
const char* ssid     = "El Pan";
const char* password = "12345678";

const int PIN_SERVO = 18;   // Pin PWM para el servo

// -------------------------------------------------------------

WebServer server(80);
Servo miServo;

int anguloServoActual = 90; // Posición inicial del servo

// ---------------------- LECTURA DE MÉTRICAS INTERNAS ----------------------
struct DatosSensores {
  unsigned long latenciaMs;   // tiempo que tomó calcular la respuesta
  float ramUsadaPct;          // % de heap (RAM) en uso
  float flashUsadaPct;        // % de flash del sketch en uso
  float temperaturaChip;      // temperatura interna del chip (°C)
  int ramLibreKB;             // heap libre en KB (dato extra)
};

DatosSensores leerSensores() {
  unsigned long t0 = millis();

  DatosSensores d;

  // ---- RAM (heap) ----
  uint32_t heapTotal = ESP.getHeapSize();
  uint32_t heapLibre  = ESP.getFreeHeap();
  d.ramLibreKB   = heapLibre / 1024;
  d.ramUsadaPct  = 100.0 * (heapTotal - heapLibre) / (float)heapTotal;

  // ---- Flash (espacio del sketch) ----
  uint32_t sketchUsado = ESP.getSketchSize();
  uint32_t sketchLibre = ESP.getFreeSketchSpace();
  d.flashUsadaPct = 100.0 * sketchUsado / (float)(sketchUsado + sketchLibre);

  // ---- Temperatura interna del chip ----
  d.temperaturaChip = temperatureRead(); // °C, sensor interno del ESP32

  d.latenciaMs = millis() - t0; // tiempo de cómputo de esta respuesta
  return d;
}

// ---------------------- PÁGINA HTML ----------------------
const char PAGINA_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Panel ESP32</title>
<style>
  :root {
    --bg: #0f172a;
    --card: #1e293b;
    --accent: #38bdf8;
    --text: #e2e8f0;
    --muted: #94a3b8;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0;
    font-family: -apple-system, Segoe UI, Roboto, sans-serif;
    background: var(--bg);
    color: var(--text);
    padding: 24px;
  }
  h1 { font-size: 22px; margin-bottom: 4px; }
  p.sub { color: var(--muted); margin-top: 0; margin-bottom: 24px; font-size: 14px; }
  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
    gap: 14px;
    margin-bottom: 32px;
  }
  .card {
    background: var(--card);
    border-radius: 12px;
    padding: 16px 18px;
    border: 1px solid #334155;
  }
  .card .label {
    font-size: 12px;
    color: var(--muted);
    text-transform: uppercase;
    letter-spacing: 0.04em;
  }
  .card .value {
    font-size: 26px;
    font-weight: 600;
    margin-top: 6px;
    color: var(--accent);
  }
  .card .sub-value {
    font-size: 12px;
    color: var(--muted);
    margin-top: 2px;
  }
  .servo-box {
    background: var(--card);
    border-radius: 12px;
    padding: 20px 22px;
    border: 1px solid #334155;
    max-width: 420px;
  }
  .servo-box h2 { font-size: 16px; margin-top: 0; }
  input[type=range] { width: 100%; accent-color: var(--accent); }
  .servo-value {
    font-size: 20px;
    font-weight: 600;
    color: var(--accent);
    margin-top: 8px;
  }
  .status-dot {
    display: inline-block;
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: #22c55e;
    margin-right: 6px;
  }
</style>
</head>
<body>

  <h1>Panel de control ESP32</h1>
  <p class="sub"><span class="status-dot" id="dot"></span><span id="conn">Conectado</span> · actualiza cada 1 s</p>

  <div class="grid">
    <div class="card">
      <div class="label">Latencia</div>
      <div class="value" id="v-lat">-- ms</div>
    </div>
    <div class="card">
      <div class="label">RAM en uso</div>
      <div class="value" id="v-ram">-- %</div>
      <div class="sub-value" id="v-ram-kb">-- KB libres</div>
    </div>
    <div class="card">
      <div class="label">Flash en uso</div>
      <div class="value" id="v-flash">-- %</div>
    </div>
    <div class="card">
      <div class="label">Temp. del chip</div>
      <div class="value" id="v-temp">-- °C</div>
    </div>
  </div>

  <div class="servo-box">
    <h2>Control de servo</h2>
    <input type="range" id="slider" min="0" max="180" value="90">
    <div class="servo-value"><span id="v-servo">90</span>°</div>
  </div>

<script>
  const slider = document.getElementById('slider');
  const vServo = document.getElementById('v-servo');
  const dot = document.getElementById('dot');
  const conn = document.getElementById('conn');

  slider.addEventListener('input', () => {
    vServo.textContent = slider.value;
  });

  slider.addEventListener('change', () => {
    fetch('/servo?angulo=' + slider.value)
      .catch(() => marcarDesconectado());
  });

  function marcarDesconectado() {
    dot.style.background = '#ef4444';
    conn.textContent = 'Sin conexión';
  }

  function marcarConectado() {
    dot.style.background = '#22c55e';
    conn.textContent = 'Conectado';
  }

  async function actualizarSensores() {
    try {
      const res = await fetch('/datos');
      const d = await res.json();
      document.getElementById('v-lat').textContent     = d.latencia + ' ms';
      document.getElementById('v-ram').textContent     = d.ramPct.toFixed(1) + ' %';
      document.getElementById('v-ram-kb').textContent  = d.ramLibreKB + ' KB libres';
      document.getElementById('v-flash').textContent   = d.flashPct.toFixed(1) + ' %';
      document.getElementById('v-temp').textContent    = d.tempChip.toFixed(1) + ' °C';
      marcarConectado();
    } catch (e) {
      marcarDesconectado();
    }
  }

  setInterval(actualizarSensores, 1000);
  actualizarSensores();
</script>
</body>
</html>
)rawliteral";

// ---------------------- HANDLERS DEL SERVIDOR ----------------------

void manejarRaiz() {
  server.send_P(200, "text/html", PAGINA_HTML);
}

void manejarDatos() {
  DatosSensores d = leerSensores();

  String json = "{";
  json += "\"latencia\":" + String(d.latenciaMs) + ",";
  json += "\"ramPct\":" + String(d.ramUsadaPct, 1) + ",";
  json += "\"ramLibreKB\":" + String(d.ramLibreKB) + ",";
  json += "\"flashPct\":" + String(d.flashUsadaPct, 1) + ",";
  json += "\"tempChip\":" + String(d.temperaturaChip, 1);
  json += "}";

  server.send(200, "application/json", json);
}

void manejarServo() {
  if (server.hasArg("angulo")) {
    int angulo = server.arg("angulo").toInt();
    angulo = constrain(angulo, 0, 180);
    anguloServoActual = angulo;
    miServo.write(anguloServoActual);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Falta parámetro 'angulo'");
  }
}

void manejarNoEncontrado() {
  server.send(404, "text/plain", "Ruta no encontrada");
}

// ---------------------- SETUP / LOOP ----------------------

void setup() {
  Serial.begin(115200);

  miServo.setPeriodHertz(50);
  miServo.attach(PIN_SERVO, 500, 2400);
  miServo.write(anguloServoActual);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Conectado. IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", manejarRaiz);
  server.on("/datos", manejarDatos);
  server.on("/servo", manejarServo);
  server.onNotFound(manejarNoEncontrado);

  server.begin();
  Serial.println("Servidor web iniciado.");
}

void loop() {
  server.handleClient();
}