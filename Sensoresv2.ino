#include <DHT.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define         MQ2                       (15)     //define la entrada analogica para el sensor
#define         MQ7                   (2)
#define         MQ8                   (4)
#define         RL_VALOR             (3.3)     //define el valor de la resistencia mde carga en kilo ohms
#define         RAL       (9.83)  // resistencia del sensor en el aire limpio / RO, que se deriva de la                                             tabla de la hoja de datos
#define         GAS                      (0)
#define          DHTPIN (23)
#define          DHTTYPE DHT11

const char* ssid = "EntraYDiQueEresPobre";
const char* password = "SoyPobre135";
const char* serverIP = "192.168.156.159";
const int serverPort = 8080;
WiFiUDP udp;

String inputstring = "";                                                        //Cadena recibida desde el PC
float           LPCurve[3]  =  {2.3, 0.21, -0.47}; 
float           COCurve[3]  =  {1.8, 0.09, -0.43};
float           H2Curve[3]  =  {9.0, 0.04, -0.0091};
float           RoMQ2           =  0.46;
float           RoMQ7           =  1.12;
float           RoMQ8           =  0.11;

DHT dht(DHTPIN, DHTTYPE);

void setup(){
Serial.begin(9600);                                                                  //Inicializa Serial a 9600 baudios
 Serial.println("Iniciando ...");
 WiFi.begin(ssid, password);
  Serial.print("Conectando a la red Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");  
  }
  Serial.print("\n");
  Serial.println("Conexión Wi-Fi establecida!");
   //configuracion del sensor
  Serial.print("Calibrando...\n");
  RoMQ2 = Calibracion(MQ2);                        //Calibrando el sensor. Por favor de asegurarse que el sensor se encuentre en una zona de aire limpio mientras se calibra
  RoMQ7 = Calibracion(MQ7);
  RoMQ8 = Calibracion(MQ8);
  Serial.print("Calibracion finalizada...\n");
  Serial.print(analogRead(MQ2));
  Serial.print("\n");
  Serial.print("R0 MQ2=");
  Serial.print(RoMQ2);
  Serial.print("kohm");
  Serial.print("\n");
  Serial.print(analogRead(MQ7));
  Serial.print("\n");
  Serial.print("R0 MQ7=");
  Serial.print(RoMQ7);
  Serial.print("kohm");
  Serial.print("\n");
  Serial.print(analogRead(MQ8));
  Serial.print("\n");
  Serial.print("R0 MQ8=");
  Serial.print(RoMQ8);
  Serial.print("kohm");
  Serial.print("\n");
  dht.begin();
}
 
void loop(){
 //Obtener el número de serie
 uint64_t chipId = ESP.getEfuseMac();

 float glp = 0, co = 0, h2 = 0;
 float humidity = 0, temperature = 0;
 for( int i = 0; i < 7; i++){
    glp = glp + porcentaje_gasLP(lecturaMQ(MQ2)/RoMQ2,GAS);
    co = co + porcentaje_gasCO(lecturaMQ(MQ7)/RoMQ7,GAS);
    h2 = h2 + porcentaje_gasH2(lecturaMQ(MQ8)/RoMQ8,GAS);
    humidity = humidity + dht.readHumidity();
    temperature = temperature + dht.readTemperature();
    delay(10000);
  }
  glp = glp / 7;
  co = co / 7;
  h2 = h2 / 7;
  humidity = humidity / 7;
  temperature = temperature / 7;
  
   /*Serial.print("LP:");
   Serial.print(glp );
   Serial.print( "ppm" );
   Serial.print("\n");
   Serial.print("CO:");
   Serial.print(porcentaje_gasCO(lecturaMQ(MQ7)/RoMQ7,GAS) );
   Serial.print( "ppm" );
   Serial.print("\n");
   Serial.print("H2:");
   Serial.print(porcentaje_gasH2(lecturaMQ(MQ8)/RoMQ8,GAS) );
   Serial.print( "ppm" );
   Serial.print("\n");
  Serial.print("Humedad: ");
  Serial.print(humidity);
  Serial.print("%\tTemperatura: ");
  Serial.print(temperature);
  Serial.println("°C");*/
  
  //Crear un objeto JSON con los datos recopilados y el número de serie
  StaticJsonDocument<200> jsonDocument;
  jsonDocument["SerialNumber"] = chipId;
  jsonDocument["LP"] = glp;
  jsonDocument["CO"] = co;
  jsonDocument["H2"] = h2;
  jsonDocument["humidity"] = humidity;
  jsonDocument["temperature"] = temperature;

  // Serializar el objeto JSON
  String jsonString;
  serializeJson(jsonDocument, jsonString);

  //Enviar el JSon mediante UDP
  udp.beginPacket(serverIP, serverPort);
  udp.print(jsonString);
  udp.endPacket();

  // Enviar el JSON a través del puerto serie
  Serial.println(jsonString);
}
 
float calc_res(int raw_adc)
{
  return ( ((float)RL_VALOR*(4095-raw_adc)/raw_adc)); //4095 3.3
}
 
float Calibracion(float mq_pin){
  int i;
  float val=0;
    for (i=0;i<50;i++) {                                                                               //tomar múltiples muestras
    val += calc_res(analogRead(mq_pin));
    delay(500);
  }
  val = val/50;                                                                                         //calcular el valor medio
  val = val/RAL;
  return val;
}
 
float lecturaMQ(int mq_pin){
  int i;0
  float rs=0;
  for (i=0;i<5;i++) {
    rs += calc_res(analogRead(mq_pin));
    delay(50);
  }
rs = rs/5;
return rs;
}
 
float porcentaje_gasLP(float rs_ro_ratio, int gas_id){
   if ( gas_id == GAS ) {
     return porcentaje_gas(rs_ro_ratio,LPCurve);
   }
  return 0;
}

float porcentaje_gasCO(float rs_ro_ratio, int gas_id){
   if ( gas_id == GAS ) {
     return porcentaje_gas(rs_ro_ratio,COCurve);
   }
  return 0;
}

float porcentaje_gasH2(float rs_ro_ratio, int gas_id){
   if ( gas_id == GAS ) {
     return porcentaje_gas(rs_ro_ratio,H2Curve);
   }
  return 0;
}
 
float porcentaje_gas(float rs_ro_ratio, float *pcurve){
  return (pow(10, (((log(rs_ro_ratio)-pcurve[1])/pcurve[2]) + pcurve[0])));
}
