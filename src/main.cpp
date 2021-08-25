#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <SPIFFS.h>



#define WIFI_NETWORK "SUZANA 40 OI FIBRA 2G"
#define  WIFI_PASSWORD "10081958"
#define WIFI_TIMEOUT_MS 2000


/*
HARDWARE
*/
// DHT22 SENSOR
DHT my_sensor(5, DHT22);
//LED PINOUT
int LED = 2;
//pwm
int pwm_pin = 27; // pino a ser usado para pwm 
int pwm_channel = 0; // Canal de pwm

/*
 * Prototipos das tarefas
 */
void tarefa_1(void * parameters); //Ler temperatura
void tarefa_2(void * parameters); //Conectar e Manter WIFI
void tarefa_3(void * parameters); // Web Server
void tarefa_4(void * parameters); // Salvar as medias, semaforo
void tarefa_5(void * parameters); // Atualiza PWM pela temperatura
void tarefa_6(void * parameters); // Salva a configuracao de fan
void tarefa_7(void * parameters); //
/*
Task Handlers
*/
xSemaphoreHandle sem_media = NULL;
xTaskHandle save_cfg = NULL;

/*
Globals
*/
volatile float temperature;
volatile float temp_med = 0;
volatile float peak_buffer = 0;
  //curvas
volatile int curva_ind = 2;
int get_curva(void);
void load_cfg();
String translate(int x);


/*
DEFAULT FUNCTIONS
*/
void setup() {
  pinMode(LED, OUTPUT);
  Serial.begin(9600);
  if(!SPIFFS.begin()){
    Serial.println("[SPIFFS] Erro na Montagem"); 
  }
  Serial.println("[SPIFFS] Montado");
  load_cfg();


  xTaskCreatePinnedToCore(
    tarefa_1,  // nome pro os
    "get_temperature", // nome humano
    1000, // tamanho da pilha
    NULL, // parameters
    6,  //prioridade
    NULL, // handle
    1
  );

xTaskCreatePinnedToCore(
  tarefa_2,
  "Keep Alive",
  5000,
  NULL,
  2,
  NULL,
  CONFIG_ARDUINO_RUNNING_CORE
);
xTaskCreate(
  tarefa_3,
  "Web_Server",
  5000,
  NULL,
  1,
  NULL
);
xTaskCreate(
  tarefa_4,
  "Salvar Valores",
  8000,
  NULL,
  4,
  NULL
);
xTaskCreatePinnedToCore(
  tarefa_5,
  "Atualiza PWM",
  1000,
  NULL,
  5,
  NULL,
  1
);
xTaskCreate(
  tarefa_6,
  "Salva Cfg",
  5000,
  NULL,
  3,
  &save_cfg
);

}

void loop() {
  // put your main code here, to run repeatedly:
  vTaskDelay(15);

}





/* Tarefas de exemplo que usam funcoes para suspender/continuar as tarefas */
void tarefa_1(void * parameters)
{
  my_sensor.begin();
  volatile float temp_sum = 0;
  volatile float temp_peak = 0;
  volatile int temp_ind = 0;
  sem_media = xSemaphoreCreateBinary();

	for(;;)
	{
		temperature = my_sensor.readTemperature();        // Leitura do Sensor
		temp_ind ++;
		temp_sum = temp_sum + temperature;                // Soma 631 temps (20min)
    if(temperature > temp_peak){                      // Temperatura de Pico (20m)
      temp_peak = temperature;
    }
    Serial.print("Temperatura: ");
    Serial.println(temperature);
		if(temp_ind == 6){
			temp_med = temp_sum/temp_ind;                   // Media de temperatura a cada 20m (480*2.5s)
      peak_buffer = temp_peak;                       // Carrega o buffer da temperatura de pico
      temp_peak = 0;
			temp_ind = 0;
			temp_sum = 0;
      Serial.print("Temperatura Media: ");
      Serial.println(temp_med);                         // So pra manter informado no Serial
      xSemaphoreGive(sem_media);                      // Libera para a funcao que grava
		} 
		vTaskDelay(2500);
	}
}


void tarefa_2(void * parameters){
  if(!WiFi.begin(WIFI_NETWORK, WIFI_PASSWORD)){
    Serial.println("[WIFI] Erro na Inicializacao");
  }
  Serial.println("[WIFI] Modulo Inicializado");

  for(;;){
		if(WiFi.status() == WL_CONNECTED){              // Verifica se o Wifi Ainda esta conectado
      Serial.println("[WIFI] Conectado");
			vTaskDelay(31000 / portTICK_PERIOD_MS);       // Periodo de repeticao da verificacao
			continue;                                     // Ignora o restante da funcao
		}
		Serial.println("[WIFI] Tentando Conectar");
		WiFi.begin(WIFI_NETWORK, WIFI_PASSWORD);        // Caso tenha perdido a conexao, rotina de reconeccao

		unsigned long startAttemptTime = millis();      // contagem p/ timeout
		
    while (WiFi.status() != WL_CONNECTED && (millis() - startAttemptTime < WIFI_TIMEOUT_MS) ){}
		if(WiFi.status() != WL_CONNECTED){
            Serial.println("[WIFI] Falha na Conexao");        // TimedOut ao tentar reconectar
            vTaskDelay(21000 / portTICK_PERIOD_MS);
			  continue;
        }
    
    Serial.println("[WIFI] Connected: " + WiFi.localIP());   // Reconectou
	}
}

void tarefa_3(void * parameters){
  WiFiServer server(80);
  server.begin();
  for(;;){
    WiFiClient client = server.available();
    if (client) { 
      Serial.println("[WEB-SV] New Client.");
      String currentLine = "";
      while (client.connected()) {
        if (client.available()) {
          char c = client.read();
          Serial.write(c);
          if (c == '\n') {
            if (currentLine.length() == 0) {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println();
              client.println("<!DOCTYPE HTML>");
              client.println("<html>");
              client.println("<meta http-equiv=\"refresh\" content=\"2\" >");
              client.print("Temperatura Atual: ");
              client.println();
              client.println(temperature);
              client.println();
              client.println("<br />");
              client.print("Click <a href=\"/O\">OFF</a> para desligar os Ventiladores.<br>");
              client.print("Click <a href=\"/L\">MINIMUM</a> para ligar os Ventiladores com baixa rotacao.<br>");
              client.print("Click <a href=\"/M\">GRADUAL</a> para os Ventiladores acelerarem conforme a temperatura.<br>");
              client.print("Click <a href=\"/H\">MAX</a> para ligar os Ventiladores com rotacao maxima.<br>");
              client.print("Click <a href=\"/S\">SAVE Cfg</a> para salvar a configuracao.<br>");
              client.println();
              client.println("</html>");
              break;
            }
            else {
              currentLine = "";
            }
          } else if (c != '\r') {
            currentLine += c;
          }
          if (currentLine.endsWith("GET /O")) {
            curva_ind = 0;
            //digitalWrite(LED, HIGH);
          }
          if (currentLine.endsWith("GET /L")) {
            curva_ind = 1;
          }
          if (currentLine.endsWith("GET /M")) {
            curva_ind = 2;
          }
          if (currentLine.endsWith("GET /H")) {
            curva_ind = 3;
          }
          if (currentLine.endsWith("GET /S")) {
            vTaskResume(save_cfg);
          }
        }
      }
      client.stop();
      Serial.println("[WEB-SV] Client Disconnected.");
    }

  }
}

void tarefa_4(void * parameters){

  volatile int tempo = 0;         // tempo de execucao que foi obtido o dado (min)
  File file;
  
  for(;;){
      if(xSemaphoreTake(sem_media, portMAX_DELAY)){       // Verifica disponibilidade do semaforo
        Serial.println("[SPIFFS] Gravando...");
        if(file = SPIFFS.open("/media.txt", "a")){        //Grava [0]24.45 ...
          file.print("[");
          file.print(tempo);
          file.print("]");
          file.print(temp_med);
          file.print("-");
          file.println(peak_buffer);
          file.close();
          Serial.println("[SPIFFS] Gravacao Concluida");
        }
        else{Serial.println("[SPIFFS] Erro na abertura do arquivo");}        
        tempo += 20;

      }
    }
  
}

void tarefa_5(void * parameters){

  int pwm_freq = 5000;
  volatile int intensity = 0;

  ledcSetup(pwm_channel, pwm_freq, 8);
  ledcAttachPin(pwm_pin, pwm_channel);

  for(;;){

    intensity = get_curva();
    if(intensity > 255 || intensity < 0){intensity = 255;}
    ledcWrite(pwm_channel, intensity);
    Serial.print("[PWM]");
    Serial.println(intensity);
    vTaskDelay(5000);
  }
}


void tarefa_6(void * parameters){
  File file;
  String s;
  for(;;){
    vTaskSuspend(NULL);
    s = translate(curva_ind);
    Serial.println("Conferindo o q sera salvo");
    Serial.println(s);
    Serial.println("[SPIFFS] Gravando Cfg...");
    if(file = SPIFFS.open("/cfg.txt", "w")){        //Grava [0]24.45 ... 
          file.println(s);
          file.close();
          Serial.println("[SPIFFS] Gravacao Cgf Concluida");
        }
        else{Serial.println("[SPIFFS] Erro na abertura do arquivo Cfg");}
  }
}



int get_curva(void){
  switch (curva_ind)
  {
  case 0:
    return 0;
    break;
  case 1:
    return 56;
  case 2:
    if(temperature < 40){return 0;}
    else{
      if(temperature > 75){return 255;}
      else{
      return ((7.22*temperature) - 286.5);
      }    
    }
    break;
  case 3:
    return 255;
  default:
    return 255;
    break;
  }
}

void load_cfg(){
  File file;
  String s;
  Serial.println("[SPIFFS] Carregando Cfg...");
    if(file = SPIFFS.open("/cfg.txt", "r")){      
          file.seek(0, SeekSet);
          s = file.readStringUntil('\n');
          curva_ind = s.toInt();
          file.close();
          Serial.println("[SPIFFS] Carregamento Cgf Concluido");
          Serial.println(curva_ind);
        }
        else{Serial.println("[SPIFFS] Erro na abertura do arquivo Cfg");}
}

String translate(int x){
  switch (x){
    case 0:
      return "0";
    break;
    case 1:
      return "1";
    break;
    case 2:
      return "2";
    break;
    case 3:
      return "3";
    break;
    default:
      Serial.println("[TRANSLATE]Erro na conversao");
      return "2";
    break;
  }
}