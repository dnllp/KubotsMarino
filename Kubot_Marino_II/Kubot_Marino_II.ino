#include <WiFi.h>
#include <WebSocketsServer_Generic.h> // Se debe descargar esta librería
#include <WebServer.h>

// Pines de control de motores para el driver L298N (o similar)
#define IZQ_PWM     22 // D0 (PWMB) para el motor izquierdo
#define IZQ_AVZ     32 // D1 (BIN2) para dirección de avance del motor izquierdo
#define IZQ_RET     33 // D2 (BIN1) para dirección de retroceso del motor izquierdo
#define STBY        02 // D4 (STBY) para habilitar/deshabilitar el driver de motor
#define DER_RET     16 // D5 (AIN1) para dirección de retroceso del motor derecho
#define DER_AVZ     17 // D6 (AIN2) para dirección de avance del motor derecho
#define DER_PWM     21 // D7 (PWMA) para el motor derecho

const char* ssid = "Dragon_05";   // Cambiar por el nombre de sus robots ej Messi, Mbappe, etc. 
const char* password = "mante2025"; // Cambiar la clave para que solo los integrantes del equipo puedan accesar a sus robots.

const int freq = 2000;
const int resolution = 8; // 8 Bit resolution for duty cycle so value is between 0 - 255 (2^8 - 1)

// Variable global para la velocidad del slider
uint8_t currentSpeed = 100; // Inicializamos con un valor por defecto razonable

// Servidor Web y WebSocket
WebServer server (80);
WebSocketsServer webSocket = WebSocketsServer(81);

// --- Funciones de Control de Motores ---
void stopMotors() {
  digitalWrite(STBY, LOW); // Apaga el driver para un stop total
  // Asegura que las salidas de dirección estén en LOW
  digitalWrite(IZQ_AVZ, LOW); 
  digitalWrite(IZQ_RET, LOW); 
  digitalWrite(DER_AVZ, LOW); 
  digitalWrite(DER_RET, LOW);
  // Detener el PWM (velocidad 0)
  ledcWrite(IZQ_PWM, 0);
  ledcWrite(DER_PWM, 0);
  Serial.println("Motores detenidos.");
}

void moveMotors(int leftSpeed, int rightSpeed, bool leftDirForward, bool rightDirForward) {
  digitalWrite(STBY, HIGH); // Enciende el driver

  // Motor Izquierdo
  digitalWrite(IZQ_AVZ, leftDirForward ? HIGH : LOW);
  digitalWrite(IZQ_RET, leftDirForward ? LOW : HIGH);
  ledcWrite(IZQ_PWM, leftSpeed);

  // Motor Derecho
  digitalWrite(DER_AVZ, rightDirForward ? HIGH : LOW);
  digitalWrite(DER_RET, rightDirForward ? LOW : HIGH);
  ledcWrite(DER_PWM, rightSpeed);

  Serial.printf("Movimiento: Izq:%d (%s) Der:%d (%s)\n", 
                leftSpeed, leftDirForward ? "AVZ" : "RET", 
                rightSpeed, rightDirForward ? "AVZ" : "RET");
}

// --- Manejo de Eventos WebSocket ---
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Desconectado!\n", num);
      stopMotors(); // Detener motores al desconectarse
      break;
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Conectado desde %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      webSocket.sendTXT(num, "Conectado al Kubot!"); // Mensaje de bienvenida
    }
    break;
    case WStype_TEXT:
      Serial.printf("Número de conexión: %u - Texto recibido: %s\n", num, payload);
      // Solo procesamos comandos de la primera conexión (del navegador de control)
      if(num == 0) { 
        String str = (char*)payload; 

        if (str.startsWith("SPEED:")) {
          currentSpeed = str.substring(6).toInt();
          currentSpeed = constrain(currentSpeed, 0, 255); // Asegura que esté en rango
          Serial.printf("Velocidad actualizada a: %d\n", currentSpeed);
        } else if (str.startsWith("CMD:")) {
          String command = str.substring(4);

          if (command == "FORWARD") {
            moveMotors(currentSpeed, currentSpeed, true, true); 
          } else if (command == "BACKWARD") {
            moveMotors(currentSpeed, currentSpeed, false, false); 
          } else if (command == "TURN_LEFT") {
            moveMotors(currentSpeed*0.5, currentSpeed*0.5, false, true); 
          } else if (command == "TURN_RIGHT") {
            moveMotors(currentSpeed*0.5, currentSpeed*0.5, true, false); 
          } else if (command == "STOP") {
            stopMotors();
          }
        }
      }
      break;
  }
}

// HTML actualizado (copiar el nuevo HTML aquí)
static const char PROGMEM INDEX_HTML[] = R"( 
  <!doctype html>
  <html>
  <head>
  <meta charset=utf-8>
  <meta name='viewport' content='width=device-width, height=device-height, initial-scale=1.0, maximum-scale=1.0'/>
  <title>Control Kubot Marino</title>
  <style>
    body {
      margin: 0px;
      font-family: Arial, sans-serif;
      text-align: center;
      background-color: #331BF5; /* Color de fondo marino */
      color: white;
      display: flex;
      flex-direction: column;
      justify-content: space-around;
      align-items: center;
      min-height: 100vh;
      
      /* --- PROPIEDADES PARA DESACTIVAR SELECCIÓN/MENÚ CONTEXTUAL --- */
      -webkit-touch-callout: none; /* iOS Safari */
      -webkit-user-select: none;   /* Safari */
      -khtml-user-select: none;    /* Konqueror HTML */
      -moz-user-select: none;      /* Old versions of Firefox */
      -ms-user-select: none;       /* Internet Explorer/Edge */
      user-select: none;           /* Non-prefixed version, currently supported by Chrome, Edge, Opera and Firefox */
      -webkit-tap-highlight-color: transparent; /* Evita el resalte azul al tocar en algunos navegadores */
      touch-action: manipulation; /* Optimiza el manejo de eventos táctiles, previene zoom y doble toque */
      /* --- FIN PROPIEDADES --- */
    }
    .control-section {
      width: 90%;
      max-width: 400px;
      margin-bottom: 20px;
    }
    .buttons-grid {
      display: grid;
      grid-template-columns: repeat(3, 1fr); /* 3 columnas */
      grid-template-rows: repeat(3, 1fr); /* 3 filas */
      gap: 10px;
      margin-bottom: 20px;
    }
    .button {
      background-color: #1E90FF; /* Azul botón */
      color: white;
      border: none;
      padding: 20px 0;
      font-size: 5vmin; /* Tamaño de fuente relativo al viewport */
      border-radius: 8px;
      cursor: pointer;
      box-shadow: 0 4px #0056b3;
      transition: background-color 0.2s, box-shadow 0.2s;
      
      /* Asegura que los botones también hereden o tengan estas propiedades */
      -webkit-user-select: none;
      user-select: none;
      -webkit-touch-callout: none;
      -webkit-tap-highlight-color: transparent;
      /* touch-action: manipulation se hereda de body, no es necesario aquí */
    }
    .button:active {
      background-color: #0056b3;
      box-shadow: 0 2px #003f80;
      transform: translateY(2px);
    }
    /* Posicionamiento específico para los botones de dirección */
    .button.up { grid-column: 2; grid-row: 1; }
    .button.left { grid-column: 1; grid-row: 2; }
    .button.stop { grid-column: 2; grid-row: 2; background-color: #FF4500; box-shadow: 0 4px #cc3700; } /* Naranja para Stop */
    .button.stop:active { background-color: #cc3700; box-shadow: 0 2px #992a00; }
    .button.right { grid-column: 3; grid-row: 2; }
    .button.down { grid-column: 2; grid-row: 3; }

    .speed-slider-container {
      width: 100%;
      margin-top: 20px;
    }
    .speed-slider {
      width: 90%;
      height: 30px;
      -webkit-appearance: none;
      appearance: none;
      background: #f1f1f1;
      outline: none;
      opacity: 0.7;
      -webkit-transition: .2s;
      transition: opacity .2s;
      border-radius: 15px;
    }
    .speed-slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 40px;
      height: 40px;
      border-radius: 50%;
      background: #00FF7F; /* Verde brillante para el pulgar */
      cursor: grab;
      box-shadow: 0 2px 5px rgba(0,0,0,0.3);
    }
    .speed-slider::-moz-range-thumb {
      width: 40px;
      height: 40px;
      border-radius: 50%;
      background: #00FF7F;
      cursor: grab;
      box-shadow: 0 2px 5px rgba(0,0,0,0.3);
    }
    .speed-value {
      margin-top: 10px;
      font-size: 4vmin;
      font-weight: bold;
    }
  </style>
  </head>
  <body>
  <div class="control-section">
    <h2>Control Kubot Marino</h2>
    <div class="buttons-grid">
      <button class="button up" ontouchstart="sendCmd('FORWARD');" ontouchend="sendCmd('STOP');" onmousedown="sendCmd('FORWARD');" onmouseup="sendCmd('STOP');">▲</button>
      <button class="button left" ontouchstart="sendCmd('TURN_LEFT');" ontouchend="sendCmd('STOP');" onmousedown="sendCmd('TURN_LEFT');" onmouseup="sendCmd('STOP');">◀</button>
      <button class="button stop" ontouchstart="sendCmd('STOP');" ontouchend="sendCmd('STOP');" onmousedown="sendCmd('STOP');" onmouseup="sendCmd('STOP');">STOP</button>
      <button class="button right" ontouchstart="sendCmd('TURN_RIGHT');" ontouchend="sendCmd('STOP');" onmousedown="sendCmd('TURN_RIGHT');" onmouseup="sendCmd('STOP');">▶</button>
      <button class="button down" ontouchstart="sendCmd('BACKWARD');" ontouchend="sendCmd('STOP');" onmousedown="sendCmd('BACKWARD');" onmouseup="sendCmd('STOP');">▼</button>
    </div>

    <div class="speed-slider-container">
      <input type="range" min="0" max="255" value="100" class="speed-slider" id="speedSlider">
      <p class="speed-value">Velocidad: <span id="currentSpeedValue">150</span></p>
    </div>
  </div>

  <script>
    var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']);
    var currentSpeed = 100; // Velocidad inicial

    // ---  DESACTIVAR MENÚ CONTEXTUAL Y SELECCIÓN ---
    document.addEventListener('contextmenu', event => event.preventDefault());
    

    connection.onopen = function () {
      console.log('WebSocket Connected');
      connection.send('Connect ' + new Date());
      sendSpeedUpdate(); // Enviar velocidad inicial al conectar
    };
    connection.onerror = function (error) {
      console.log('WebSocket Error ', error);
    };
    connection.onmessage = function (event) {
      console.log('Server: ', event.data);
    };

    function sendCmd(cmd) {
      connection.send('CMD:' + cmd);
      console.log('Enviado: CMD:' + cmd);
    }

    function sendSpeedUpdate() {
      connection.send('SPEED:' + currentSpeed);
      console.log('Enviado: SPEED:' + currentSpeed);
    }

    // Slider de velocidad
    var speedSlider = document.getElementById('speedSlider');
    var currentSpeedValueSpan = document.getElementById('currentSpeedValue');

    speedSlider.oninput = function() {
      currentSpeed = this.value;
      currentSpeedValueSpan.innerHTML = currentSpeed;
      sendSpeedUpdate(); // Enviar velocidad cada vez que el slider cambia
    };

    // Manejo de eventos táctiles y de ratón para los botones
    // Los eventos onTouchStart, onTouchEnd, onMouseDown, onMouseUp ya están en línea en el HTML
  </script>
  </body>
  </html>
)";

void setup() {
  Serial.begin(115200);
  Serial.println();

  // Configurar el Wi-Fi como Access Point
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP(); 
  Serial.print("IP del access point: ");
  Serial.println(myIP);
  Serial.println("WebServer iniciado...");
    
  // Iniciar servidor WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  // Manejar la página principal (HTML)
  server.on("/", []() {
      server.send_P(200, "text/html", INDEX_HTML);
  });
  server.begin();

  // Configuración de pines para el driver de motor L298N o similar
  pinMode(IZQ_PWM, OUTPUT); 
  pinMode(DER_PWM, OUTPUT); 
  pinMode(IZQ_AVZ, OUTPUT); 
  pinMode(DER_AVZ, OUTPUT);
  pinMode(IZQ_RET, OUTPUT); 
  pinMode(DER_RET, OUTPUT);
  pinMode(STBY, OUTPUT);

  // Inicializar todos los pines de dirección a LOW y STBY a HIGH al inicio
  digitalWrite(IZQ_AVZ, LOW); 
  digitalWrite(IZQ_RET, LOW); 
  digitalWrite(DER_AVZ, LOW); 
  digitalWrite(DER_RET, LOW);
  digitalWrite(STBY, HIGH); // Activa el driver al inicio. Si prefieres que esté apagado hasta un comando, cámbialo a LOW.
  // Configurar canales PWM para los pines de velocidad
  ledcAttach(IZQ_PWM, freq, resolution);     // Asigna IZQ_PWM al canal 0
  ledcAttach(DER_PWM, freq, resolution);     // Asigna DER_PWM al canal 1
  // Detener motores al iniciar para asegurar un estado conocido
  stopMotors(); 
}

void loop() {
    webSocket.loop();
    server.handleClient();
}