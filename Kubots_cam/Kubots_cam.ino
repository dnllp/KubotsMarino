#include <WiFi.h>
#include <WebSocketsServer_Generic.h>   // Se debe descargar esta librería
#include <WebServer.h>
#include <ESP32Servo.h> // Librería para el servo
#include <esp_camera.h> // ¡NUEVA LIBRERÍA PARA LA CÁMARA!
#include "camera_pins.h" // Incluye el archivo con las definiciones de pines de la cámara

// Pines de control de motores
#define IZQ_PWM     22 // D0 (PWMB)          
#define IZQ_AVZ     32 // D1 (BIN2)          
#define IZQ_RET     33 // D2 (BIN1)
#define STBY        02 // D4 (STBY)
#define DER_RET     16 // D5 (AIN1)
#define DER_AVZ     17 // D6 (AIN2)
#define DER_PWM     21 // D7 (PWMA)

// Define el pin para el servo
#define SERVO_PIN   14 // ¡Pin para el microservo SG90! (¡CAMBIADO PARA EVITAR CONFLICTO CON LA CÁMARA!)

const char* ssid = "KubotCam_23";   // Cambiado para diferenciar
const char* password = "mante2024";

const int freq = 2000;
const int resolucion = 8;

// Variable global para la velocidad de los motores
uint8_t currentSpeed = 0; 

// Objeto Servo
Servo gripperServo; 

// Servidor Web y WebSocket
WebServer server (80);
WebSocketsServer webSocket = WebSocketsServer(81);

// --- Funciones de Control de Motores ---
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

void moveMotors(int leftSpeed, int rightSpeed, bool leftDirForward, bool rightDirForward) {
  digitalWrite(STBY, HIGH); // Enciende el driver

  digitalWrite(IZQ_AVZ, leftDirForward ? HIGH : LOW);
  digitalWrite(IZQ_RET, leftDirForward ? LOW : HIGH);
  ledcWrite(IZQ_PWM, leftSpeed);

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
      webSocket.sendTXT(num, "Conectado");
    }
    break;
    case WStype_TEXT:
      Serial.printf("Número de conexión: %u - Texto recibido: %s\n", num, payload);
      if(num == 0) { 
        String str = (char*)payload; 

        if (str.startsWith("SPEED:")) {
          currentSpeed = str.substring(6).toInt();
          currentSpeed = constrain(currentSpeed, 0, 255);
          Serial.printf("Velocidad actualizada a: %d\n", currentSpeed);
        } else if (str.startsWith("CMD:")) {
          String command = str.substring(4);

          if (command == "FORWARD") {
            moveMotors(currentSpeed, currentSpeed, true, true); 
          } else if (command == "BACKWARD") {
            moveMotors(currentSpeed, currentSpeed, false, false); 
          } else if (command == "TURN_LEFT") {
            moveMotors(currentSpeed, currentSpeed, false, true); 
          } else if (command == "TURN_RIGHT") {
            moveMotors(currentSpeed, currentSpeed, true, false); 
          } else if (command == "STOP") {
            stopMotors();
          }
        } else if (str.startsWith("SERVO:")) { 
          int servoAngle = str.substring(6).toInt();
          servoAngle = constrain(servoAngle, 0, 180);
          gripperServo.write(servoAngle); 
          Serial.printf("Servo movido a: %d grados\n", servoAngle);
        }
      }
      break;
  }
}

// --- Configuración de la Cámara ---
bool setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000; // Frecuencia del reloj externo para la cámara

  config.pixel_format = PIXFORMAT_JPEG; // Usar JPEG para stream de video
  config.frame_size = FRAMESIZE_QQVGA;  // QVGA (320x240) es un buen balance para stream fluido
  // config.frame_size = FRAMESIZE_VGA; // Puedes probar VGA (640x480) si necesitas más resolución, pero puede ser lento
  config.jpeg_quality = 60; // Calidad de JPEG (0-63, 0 es alta calidad, 63 es baja). Ajusta este valor si la calidad es mala o el stream lento.
  config.fb_count = 2;      // Número de framebuffers. 2 puede mejorar el rendimiento pero usa más RAM.

  // Inicializar cámara
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Fallo en la inicialización de la cámara con error 0x%x", err);
    return false;
  }
  Serial.println("Cámara inicializada con éxito.");

  // Configurar propiedades de la cámara después de la inicialización
  sensor_t * s = esp_camera_sensor_get();
  // s->set_vflip(s, 1); // Descomenta para voltear verticalmente la imagen si es necesario
  // s->set_hmirror(s, 1); // Descomenta para reflejar horizontalmente la imagen si es necesario

  return true;
}

// --- Manejo del Stream de Video ---
void handleVideoStream() {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];
  
  // Set response headers
  server.sendHeader("Access-Control-Allow-Origin", "*"); // Permite la conexión desde cualquier origen
  server.setContentLength(0); // El content-length es variable para MJPEG stream
  server.send(200, "multipart/x-mixed-replace;boundary=123456789000000000000987654321"); // Encabezado para MJPEG

  while(true){ // Loop infinito para enviar frames
    fb = esp_camera_fb_get(); // Obtener un frame de la cámara
    if (!fb) {
      Serial.println("Fallo en la captura del frame de la cámara");
      res = ESP_FAIL;
    } else {
      if(fb->format != PIXFORMAT_JPEG){ // Si el formato no es JPEG, convertirlo
        bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
        if(!jpeg_converted){
          Serial.println("Fallo en la compresión JPG");
          res = ESP_FAIL;
        }
      } else { // Si ya es JPEG, usarlo directamente
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
      }
    }

    if(res == ESP_OK){ // Si la captura y conversión fueron exitosas, enviar el frame
      size_t hlen = snprintf((char *)part_buf, 64, "--123456789000000000000987654321\nContent-Type: image/jpeg\nContent-Length: %u\n\n", _jpg_buf_len);
      server.client().write((const char *)part_buf, hlen); // Enviar encabezado del frame
      server.client().write((const char *)_jpg_buf, _jpg_buf_len); // Enviar datos de la imagen
    }
    
    esp_camera_fb_return(fb); // Liberar el framebuffer de la cámara
    if(_jpg_buf && _jpg_buf != fb->buf){ // Liberar buffer JPEG si se creó uno nuevo
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    
    if(res != ESP_OK){ // Si hubo un error, salir del loop
      Serial.println("Error en el stream, deteniendo.");
      break;
    }
    // Puedes agregar un pequeño delay aquí si el stream es demasiado rápido o consume muchos recursos
    // delay(10); 
  }
}

// HTML completo (incluyendo los sliders y el video stream)
static const char PROGMEM INDEX_HTML[] = R"( 
  <!doctype html>
  <html>
  <head>
  <meta charset=utf-8>
  <meta name='viewport' content='width=device-width, height=device-height, initial-scale=1.0, maximum-scale=1.0'/>
  <title>Kubot Cam Marino</title>
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

    /* Estilos para el video stream */
    #videoStream {
      width: 100%; /* Ocupa el 100% del ancho del contenedor */
      max-width: 400px; /* Ancho máximo para evitar que se vea demasiado grande en pantallas grandes */
      height: auto;
      border: 2px solid #00FF7F;
      border-radius: 8px;
      margin-bottom: 20px;
    }
  </style>
  </head>
  <body>
  <div class="control-section">
    <h2>Kubot Cam Marino</h2>
    
    <img id="videoStream" src="" alt="Cargando video...">

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

    // Inicializar el stream de video
    window.onload = function() {
      var videoStream = document.getElementById('videoStream');
      videoStream.src = 'http://' + location.hostname + ':80/stream'; 
    };

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

// --- Setup y Loop principal ---
void setup() {
  Serial.begin(115200);
  Serial.println();

  // 1. Configurar y iniciar la cámara
  Serial.println("Intentando inicializar la cámara...");
 /* if(!setupCamera()){
    Serial.println("¡ERROR: Fallo al iniciar la cámara! Asegúrate que los pines en camera_pins.h son correctos.");
    // No reiniciar automáticamente para que puedas ver el error en el Serial Monitor
    while(true) { delay(1000); } // Quedarse en un loop infinito para depuración
  }
  */
  Serial.println("Cámara inicializada.");

  // 2. Configurar el Wi-Fi como Access Point
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP(); 
  Serial.print("IP del access point: ");
  Serial.println(myIP);
  Serial.println("WebServer iniciado...");
    
  // 3. Iniciar servidores WebSocket y Web
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  // Manejar la página principal (HTML)
  server.on("/", HTTP_GET, []() {
      server.send_P(200, "text/html", INDEX_HTML);
  });
  
  // Manejar el stream de video
//  server.on("/stream", HTTP_GET, handleVideoStream);

  server.begin();

  // 4. Configuración de pines para el driver de motor L298N o similar
  pinMode(IZQ_PWM, OUTPUT); 
  pinMode(DER_PWM, OUTPUT); 
  pinMode(IZQ_AVZ, OUTPUT); 
  pinMode(DER_AVZ, OUTPUT);
  pinMode(IZQ_RET, OUTPUT); 
  pinMode(DER_RET, OUTPUT);
  pinMode(STBY, OUTPUT);

  digitalWrite(IZQ_PWM, LOW); 
  digitalWrite(DER_PWM, LOW);
  digitalWrite(IZQ_AVZ, LOW); 
  digitalWrite(DER_AVZ, LOW);
  digitalWrite(IZQ_RET, LOW); 
  digitalWrite(DER_RET, LOW);
  digitalWrite(STBY, HIGH); 

  ledcAttach(IZQ_PWM, freq, resolucion); 
  ledcAttach(DER_PWM, freq, resolucion); 
  

  // 5. Configuración del servo
  gripperServo.attach(SERVO_PIN); 
  gripperServo.write(90); 
  Serial.printf("Servo conectado al pin: %d y movido a 90 grados.\n", SERVO_PIN);
}

void loop() {
  webSocket.loop();
  server.handleClient();
}