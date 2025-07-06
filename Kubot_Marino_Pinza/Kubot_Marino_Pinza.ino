#include <WiFi.h>
#include <WebSocketsServer_Generic.h>   // Se debe descargar esta librería
#include <WebServer.h>
#include <ESP32Servo.h> // ¡NUEVA LIBRERÍA PARA EL SERVO!

#define IZQ_PWM     22 // D0 (PWMB)          
#define IZQ_AVZ     32 // D1 (BIN2)          
#define IZQ_RET     33 // D2 (BIN1)
#define STBY        02 // D4 (STBY)
#define DER_RET     16 // D5 (AIN1)
#define DER_AVZ     17 // D6 (AIN2)
#define DER_PWM     21 // D7 (PWMA)

// Define el pin para el servo
#define SERVO_PIN   13 // ¡Pin para el microservo SG90!

const char* ssid = "Dragon_09";   // Cambiar por el nombre de sus robots ej Messi, Mbappe, etc. 
const char* password = "mante2025"; // cambiar la clave para que solo los integrandes del equipo puedan accesar a sus robots.

const int freq = 2000;
const int resolution = 8;

// Variable global para la velocidad de los motores
uint8_t currentSpeed = 0; 

// Objeto Servo
Servo gripperServo; // ¡Declaración del objeto servo!

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

    .slider-container {
      width: 100%;
      margin-top: 20px;
    }
    .slider {
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
    .slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 40px;
      height: 40px;
      border-radius: 50%;
      background: #00FF7F; /* Verde brillante para el pulgar */
      cursor: grab;
      box-shadow: 0 2px 5px rgba(0,0,0,0.3);
    }
    .slider::-moz-range-thumb {
      width: 40px;
      height: 40px;
      border-radius: 50%;
      background: #00FF7F;
      cursor: grab;
      box-shadow: 0 2px 5px rgba(0,0,0,0.3);
    }
    .slider-value {
      margin-top: 10px;
      font-size: 4vmin;
      font-weight: bold;
    }
  </style>
  </head>
  <body>
  <div class="control-section">
    <h2>Control Kubot Marino</h2>
    <h2>Investigador</h2>
    <div class="buttons-grid">
      <button class="button up" ontouchstart="sendCmd('FORWARD');" ontouchend="sendCmd('STOP');" onmousedown="sendCmd('FORWARD');" onmouseup="sendCmd('STOP');">▲</button>
      <button class="button left" ontouchstart="sendCmd('TURN_LEFT');" ontouchend="sendCmd('STOP');" onmousedown="sendCmd('TURN_LEFT');" onmouseup="sendCmd('STOP');">◀</button>
      <button class="button stop" ontouchstart="sendCmd('STOP');" ontouchend="sendCmd('STOP');" onmousedown="sendCmd('STOP');" onmouseup="sendCmd('STOP');">STOP</button>
      <button class="button right" ontouchstart="sendCmd('TURN_RIGHT');" ontouchend="sendCmd('STOP');" onmousedown="sendCmd('TURN_RIGHT');" onmouseup="sendCmd('STOP');">▶</button>
      <button class="button down" ontouchstart="sendCmd('BACKWARD');" ontouchend="sendCmd('STOP');" onmousedown="sendCmd('BACKWARD');" onmouseup="sendCmd('STOP');">▼</button>
    </div>

    <div class="slider-container">
      <label for="speedSlider" class="slider-value">Velocidad</label>
      <input type="range" min="0" max="255" value="150" class="slider" id="speedSlider">
      <p class="slider-value">Velocidad: <span id="currentSpeedValue">150</span></p>
    </div>

    <div class="slider-container">
      <label for="servoSlider" class="slider-value">Pinza (Ángulo)</label>
      <input type="range" min="0" max="180" value="90" class="slider" id="servoSlider">
      <p class="slider-value">Ángulo Pinza: <span id="currentServoAngle">90</span>°</p>
    </div>
  </div>

  <script>
    var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']);
    var currentSpeed = 150; // Velocidad inicial
    var currentServoAngle = 90; // Ángulo inicial del servo

    connection.onopen = function () {
      console.log('WebSocket Connected');
      connection.send('Connect ' + new Date());
      sendSpeedUpdate(); // Enviar velocidad inicial al conectar
      sendServoAngleUpdate(); // Enviar ángulo inicial del servo
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

    function sendServoAngleUpdate() {
      connection.send('SERVO:' + currentServoAngle);
      console.log('Enviado: SERVO:' + currentServoAngle);
    }

    // Slider de velocidad
    var speedSlider = document.getElementById('speedSlider');
    var currentSpeedValueSpan = document.getElementById('currentSpeedValue');

    speedSlider.oninput = function() {
      currentSpeed = this.value;
      currentSpeedValueSpan.innerHTML = currentSpeed;
      sendSpeedUpdate(); // Enviar velocidad cada vez que el slider cambia
    };

    // Slider del servo
    var servoSlider = document.getElementById('servoSlider');
    var currentServoAngleSpan = document.getElementById('currentServoAngle');

    servoSlider.oninput = function() {
      currentServoAngle = this.value;
      currentServoAngleSpan.innerHTML = currentServoAngle;
      sendServoAngleUpdate(); // Enviar ángulo del servo cada vez que el slider cambia
    };
  </script>
  </body>
  </html>
)";

WebServer server (80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Función para detener los motores
void stopMotors() {
  digitalWrite(STBY, LOW); // Apaga el driver
  digitalWrite(IZQ_AVZ, 0); 
  digitalWrite(IZQ_RET, 0); 
  digitalWrite(DER_AVZ, 0); 
  digitalWrite(DER_RET, 0);
  ledcWrite(IZQ_PWM, 0);
  ledcWrite(DER_PWM, 0);
  Serial.println("Motores detenidos.");
}

// Función para mover los motores
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


void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Desconectado!\n", num);
      stopMotors(); // Detener motores al desconectarse
      break;
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Conectado desde %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      webSocket.sendTXT(num, "Conectado");        // send message to client
    }
    break;
    case WStype_TEXT:
      Serial.printf("Número de conexión: %u - Texto recibido: %s\n", num, payload);
      if(num == 0) { // Únicamente va a reconocer la primera conexión (softAP). 
        String str = (char*)payload; 

        if (str.startsWith("SPEED:")) {
          currentSpeed = str.substring(6).toInt();
          // Asegurarse de que la velocidad esté dentro del rango PWM (0-255)
          currentSpeed = constrain(currentSpeed, 0, 255);
          Serial.printf("Velocidad actualizada a: %d\n", currentSpeed);
        } else if (str.startsWith("CMD:")) {
          String command = str.substring(4); // Extraer el comando (ej. "FORWARD", "STOP")

          if (command == "FORWARD") {
            moveMotors(currentSpeed, currentSpeed, true, true); // Ambos hacia adelante
          } else if (command == "BACKWARD") {
            moveMotors(currentSpeed, currentSpeed, false, false); // Ambos hacia atrás
          } else if (command == "TURN_LEFT") {
            // Para girar a la izquierda, el motor izquierdo retrocede y el derecho avanza
            moveMotors(currentSpeed, currentSpeed, false, true); 
          } else if (command == "TURN_RIGHT") {
            // Para girar a la derecha, el motor derecho retrocede y el izquierdo avanza
            moveMotors(currentSpeed, currentSpeed, true, false); 
          } else if (command == "STOP") {
            stopMotors();
          }
        } else if (str.startsWith("SERVO:")) { // ¡NUEVO: Manejar comandos del servo!
          int servoAngle = str.substring(6).toInt();
          // Asegurarse de que el ángulo esté dentro del rango típico de un servo (0-180)
          servoAngle = constrain(servoAngle, 0, 180);
          gripperServo.write(servoAngle); // Mover el servo al ángulo recibido
          Serial.printf("Servo movido a: %d grados\n", servoAngle);
        }
      }
      break;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP(); 
  Serial.print("IP del access point: ");
  Serial.println(myIP);
  Serial.println("WebServer iniciado...");
    // start webSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  // handle index
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

  // Inicializar todos los pines de control a LOW y STBY a HIGH por defecto (puede ser LOW también)
  digitalWrite(IZQ_PWM, LOW); 
  digitalWrite(DER_PWM, LOW);
  digitalWrite(IZQ_AVZ, LOW); 
  digitalWrite(DER_AVZ, LOW);
  digitalWrite(IZQ_RET, LOW); 
  digitalWrite(DER_RET, LOW);
  digitalWrite(STBY, HIGH); // Activar el driver de motor por defecto, o LOW si prefieres controlarlo con STOP

  // Configurar canales PWM para los pines de velocidad
  ledcAttach(IZQ_PWM, freq, resolution); // Asigna IZQ_PWM al canal 0
  ledcAttach(DER_PWM, freq, resolution); // Asigna DER_PWM al canal 1
  //ledcSetup(0, freq, resolution); // Configura el canal 0
  //ledcSetup(1, freq, resolution); // Configura el canal 1

  // Configuración del servo
  gripperServo.attach(SERVO_PIN); // Asocia el objeto servo al pin 13
  gripperServo.write(90); // Mueve el servo a una posición central inicial (90 grados)
}

void loop() {
    webSocket.loop();
    server.handleClient();
}