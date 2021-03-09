#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "esp_camera.h"
#include <Preferences.h>

#include "BluetoothSerial.h"
#include <String.h>

#ifdef NVINIT
#include "nvs_flash.h"
#include "nvs.h"
#endif
//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//

// cleaning :       pio run --target erase

// C:\Users\la\.platformio\packages\framework-arduinoespressif32\tools\partitions\huge_app2.csv:
//
// # Name     Type  SubType     Offset   Size   Flags
// nvs,       data,  nvs,       0x9000,   0x5000,
// otadata,   data,  ota,       0xe000 ,  0x2000,
// app0,      app,   ota_0,     0x10000,  0x3c0000,
// spiffs,    data,  spiffs,    0x3D0000, 0x30000,

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//
// AiTinker specific
//
#define RED_BACKSIDE_LED 33
#define FLASHLIGHT_LED 4
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM

#include "camera_pins.h"

#define MYNAME "ESPCAM00"
#define MYVERSION "FW:V3-202103091800"

Preferences pref;
BluetoothSerial SerialBT;

String ssids_array[50];
String network_string;

String BTinp();
int wifiScanNetworks();

void startCameraServer();
bool wifiConnect(long timeout);
void WIFIPrintStatus(int status);
void cameraInit();
void digitalToggle(uint8_t PIN);

void setup()
{
  // RED LED on backside off
  pinMode(RED_BACKSIDE_LED, OUTPUT);
  digitalWrite(RED_BACKSIDE_LED, HIGH);

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println(MYVERSION);

  cameraInit();

  String ssid;
  String pass;
  pref.begin(MYNAME, false);

  // Serial.print("freeEntries()_:");
  // Serial.println( pref.freeEntries());;

  //Try WiFi connect
  // if not successful, ask for ssid and passwword via Bluetooth
  while (!wifiConnect(40000))
  {
    if (!SerialBT.begin(MYNAME))
    {
      Serial.println("Starting bluetooth failed.");
      ESP.restart();
    }

    digitalToggle(RED_BACKSIDE_LED);
    Serial.println("Bluetooth waiting for connect. My name_:");
    while (!SerialBT.connected(5000))
    {
      digitalToggle(RED_BACKSIDE_LED);
      Serial.print("#");
      Serial.println(MYNAME);
    }

    digitalWrite(RED_BACKSIDE_LED, HIGH);

    int client_wifi_ssid_id = 0;
    int n = wifiScanNetworks();

    if (n == 0)
    {
      SerialBT.println("No networks found. Givig up.");
      return;
    }

    // Ask for SSID until a valid index has been choosen
    while (client_wifi_ssid_id < 1 || client_wifi_ssid_id > n)
    {
      SerialBT.println();
      SerialBT.print("Enter SSID number:");
      client_wifi_ssid_id = BTinp().toInt();
      SerialBT.print("Client_wifi_ssid_id:");
      SerialBT.println(client_wifi_ssid_id);
    }

    ssid = ssids_array[client_wifi_ssid_id];
    SerialBT.printf("SSID_:%s\n", ssid.c_str());

    //ask for password
    SerialBT.println();
    SerialBT.println("Enter pass:");
    pass = BTinp();
    SerialBT.printf("PASS_:%s\n", pass.c_str());

    // store SSID and passwd to NVRAM
    pref.putString("ssid", ssid.c_str());
    pref.putString("pass", pass.c_str());

    // reread from NVRAM to be shure everything is in place
    Serial.println("Stored values:");
    Serial.print("ssid_:");
    Serial.println(pref.getString("ssid", "DEFAULT"));
    Serial.print("pass_:");
    Serial.println(pref.getString("pass", "DEFAULT"));

    SerialBT.println("Bluetooth no longer needed. Disconnecting");
    SerialBT.disconnect();
    Serial.println("Closing bluetooth connection");
    //
    //    SerialBT.end(); will cause the ESP to hang
    //    so restart will work as workaround
    ESP.restart();
  }
  // at this point the network should be connected
  Serial.print("Connected to_:");
  Serial.println(WiFi.SSID());
  Serial.print("IP_:");
  Serial.println(WiFi.localIP());

  if (!MDNS.begin(MYNAME))
    Serial.println("Error setting up MDNS responder!");

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.println(WiFi.localIP());
  Serial.printf("\nRecording:\n\tffmpeg -re -f mjpeg -t 300 -i http://%s:81/stream   -an -c:v libx265 -crf 29 -preset fast <OUTPUTFILE>", MYNAME);
  Serial.printf("\nCapture frame:\n\tcurl http://%s/capture  --output <OUTPUTFILE>\n\n", MYNAME);
  digitalWrite(RED_BACKSIDE_LED, HIGH);
}

void loop()
{
  // put your main code here, to run repeatedly:

  int i = 0;

  if (WiFi.status() != WL_CONNECTED)
  {
    wifiConnect(45000);
    i++;
    if (i > 5)
      ESP.restart();
  }
  else
  {
    i = 0;
  }
  delay(10000);
}

void cameraInit()
{
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
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (psramFound())
  {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  }
  else
  {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID)
  {
    s->set_vflip(s, 1);       // flip it back
    s->set_brightness(s, 1);  // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif
}

bool wifiConnect(long timeout)
{
  long start_wifi_millis;

  String tmp_pref_ssid = pref.getString("ssid");
  String tmp_pref_pass = pref.getString("pass");

  const char *pref_ssid = tmp_pref_ssid.c_str();
  const char *pref_pass = tmp_pref_pass.c_str();

  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);

  start_wifi_millis = millis();
  WiFi.begin(pref_ssid, pref_pass);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(125);
    digitalToggle(RED_BACKSIDE_LED);
    Serial.print(".");
    delay(125);
    digitalToggle(RED_BACKSIDE_LED);

    if (millis() - start_wifi_millis > timeout)
    {
      WiFi.disconnect(true, true);
      Serial.println();
      return false;
    }
  }
  Serial.println();
  return true;
}

int wifiScanNetworks()
{
  WiFi.mode(WIFI_STA);
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  SerialBT.println();
  if (n == 0)
  {
    SerialBT.println("no WiFi networks found");
  }
  else
  {
    SerialBT.println();
    SerialBT.print(n);
    SerialBT.println(" WiFi networks found");
    delay(1000);
    for (int i = 0; i < n; ++i)
    {
      ssids_array[i + 1] = WiFi.SSID(i);
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.println(ssids_array[i + 1]);
      network_string = i + 1;
      network_string = network_string + ": " + WiFi.SSID(i) + " (Strength:" + WiFi.RSSI(i) + ")";
      SerialBT.println(network_string);
    }
  }

  return n;
}

void WIFIPrintStatus(int status)
{
  Serial.print(F("WIFI status_: "));
  switch (WiFi.status())
  {
  case WL_IDLE_STATUS:
    Serial.println(F("WL_IDLE_STATUS    "));
    break;
  case WL_NO_SSID_AVAIL:
    Serial.println(F("WL_NO_SSID_AVAIL  "));
    break;
  case WL_SCAN_COMPLETED:
    Serial.println(F("WL_SCAN_COMPLETED "));
    break;
  case WL_CONNECTED:
    Serial.println(F("WL_CONNECTED      "));
    break;
  case WL_CONNECT_FAILED:
    Serial.println(F("WL_CONNECT_FAILED "));
    break;
  case WL_CONNECTION_LOST:
    Serial.println(F("WL_CONNECTION_LOST"));
    break;
  case WL_DISCONNECTED:
    Serial.println(F("WL_DISCONNECTED   "));
    break;
  case WL_NO_SHIELD:
    Serial.println(F("WL_NO_SHIELD      "));
    break;
  }
}

void digitalToggle(uint8_t PIN)
{
  digitalWrite(PIN, (digitalRead(PIN) == LOW ? HIGH : LOW));
}

String BTinp()
{
  String inp = "";

  while (true)
  {
    if (SerialBT.available())
    {
      inp = (SerialBT.readStringUntil('\n'));
      break;
    }
  }
  inp.replace("\n", "");
  inp.replace("\r", "");
  inp.replace("\t", "");
  inp.trim();
  return inp;
}

#ifdef NVINIT
void nvinit()
{

  Serial.println("flash init");
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    NVS partition was truncated and needs to be erased
        Retry nvs_flash_init
            ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  Serial.printf("nvs_flash_init()_:%i", err);
}
#endif
