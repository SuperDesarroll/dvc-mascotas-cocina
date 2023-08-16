/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-cam-post-image-photo-server/

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

#include <Arduino.h>
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char *ssid = "LosMuMus";
const char *password = "Best042022@";
const char *targetTags[] = {"cat", "cats", "dog","dogs","person","man"};

String serverName = "20.9.14.70"; // REPLACE WITH YOUR Raspberry Pi IP ADDRESS

String serverPath = "/upload.php"; // The default serverPath should be upload.php

const int serverPort = 80;

WiFiClient client;
String response;

String sendPhoto();
String classifyImage(camera_fb_t *fb);

// CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

#define BUZZER_PIN 15 // ESP32 GIOP15 pin connected to Buzzer's pin

const int timerInterval = 30000;  // time between each HTTP POST image
unsigned long previousMillis = 0; // last time image was sent

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP());

  pinMode(BUZZER_PIN, OUTPUT);   

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

  // init with high specs to pre-allocate larger buffers
  if (psramFound())
  {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10; // 0-63 lower number means higher quality
    config.fb_count = 2;
  }
  else
  {
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 12; // 0-63 lower number means higher quality
    config.fb_count = 1;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  sendPhoto();
}

void loop()
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= timerInterval)
  {
    sendPhoto();
    previousMillis = currentMillis;
  }
}

String sendPhoto()
{
  String getAll;
  String getBody;

  camera_fb_t *fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
  }
  String name=classifyImage(fb);

  Serial.println("Connecting to server: " + serverName);

  if (client.connect(serverName.c_str(), serverPort))
  {
    Serial.println("Connection successful!");
    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"" + name + ".jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--RandomNerdTutorials--\r\n";

    uint32_t imageLen = fb->len;
    uint32_t extraLen = head.length() + tail.length();
    uint32_t totalLen = imageLen + extraLen;

    client.println("POST " + serverPath + " HTTP/1.1");
    client.println("Host: " + serverName);
    client.println("Content-Length: " + String(totalLen));
    client.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    client.println();
    client.print(head);

    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n = 0; n < fbLen; n = n + 1024)
    {
      if (n + 1024 < fbLen)
      {
        client.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen % 1024 > 0)
      {
        size_t remainder = fbLen % 1024;
        client.write(fbBuf, remainder);
      }
    }
    client.print(tail);

    esp_camera_fb_return(fb);

    int timoutTimer = 10000;
    long startTimer = millis();
    boolean state = false;

    while ((startTimer + timoutTimer) > millis())
    {
      Serial.print(".");
      delay(100);
      while (client.available())
      {
        char c = client.read();
        if (c == '\n')
        {
          if (getAll.length() == 0)
          {
            state = true;
          }
          getAll = "";
        }
        else if (c != '\r')
        {
          getAll += String(c);
        }
        if (state == true)
        {
          getBody += String(c);
        }
        startTimer = millis();
      }
      if (getBody.length() > 0)
      {
        break;
      }
    }
    Serial.println();
    client.stop();
    Serial.println(getBody);
  }
  else
  {
    getBody = "Connection to " + serverName + " failed.";
    Serial.println(getBody);
  }
  return getBody;
}

void buz(){
  digitalWrite(BUZZER_PIN, HIGH); // turn on
  delay(3000);
  digitalWrite(BUZZER_PIN, LOW);  // turn off
  //delay(3000);
}

String classifyImage(camera_fb_t *fb)
{
  if (!fb)
  {
    Serial.println(F("Camera capture failed"));
    return "";
  }
  else
  {
    Serial.println(F("Camera capture OK"));
  }

  size_t size = fb->len;
  int len = (int)fb->len;

  String buffer = ""; // base64::encode((uint8_t *)fb->buf, fb->len);
                      //    buffer.resize(len);
  for (int i = 0; i < len; i++)
  {
    buffer += (char)fb->buf[i];
  }

  String endpoint = "https://myeyes.cognitiveservices.azure.com//";
  String subscriptionKey = "070effdc845e4542b99ec2fe31c0c065";
  String uri = endpoint + "vision/v3.2/describe"; // detect";
  Serial.println(uri);

  HTTPClient http;
  http.begin(uri);
  http.addHeader("Content-Length", (String(len)).c_str());
  http.addHeader("Content-Type", "application/octet-stream"); // multipart/form-data
  //    http.addHeader("Content-Type", "multipart/form-data");
  http.addHeader("Ocp-Apim-Subscription-Key", subscriptionKey);
  Serial.println(String(len));

  int httpResponseCode = http.POST((buffer));
  //    esp_camera_fb_return(fb);
  if (httpResponseCode > 0)
  {
    Serial.print(httpResponseCode);
    Serial.print(F("Returned String: "));
    response = http.getString();
    Serial.println(response);
  }
  else
  {
    Serial.print(F("POST Error: "));
    Serial.print(httpResponseCode);
    return "";
  }
  StaticJsonBuffer<4000> jsonBuffer;
  // Parse the json response: Arduino assistant
  JsonObject &doc = jsonBuffer.parseObject(response);

  if (!doc.success())
  {
    Serial.println("JSON parsing failed!");
  }
  int i = 0;
  String tags_name="";
  bool detected=false;
  while (true)
  {
    const char *tags = doc["description"]["tags"][i++];
    if (tags == nullptr)
      break;        
    char *tag;
    tag = strtok((char *)tags, " "); // Split the tags by space
    while (tag != NULL)
    {
      String tagString = String(tag);
      Serial.println(tagString);
      for (int i = 0; i < sizeof(targetTags) / sizeof(targetTags[0]); i++)
      {
        if (strcmp(tagString.c_str(), targetTags[i]) == 0)
        {
          Serial.println(targetTags[i]);
          Serial.println(" ****** detected");  
          detected=true;    
        }
      }      
      tags_name.concat(tagString);
      tags_name.concat("_");
      tag = strtok(NULL, " ");      
    }
  }
  i=0;
  
  tags_name.concat("@");
  while (true)
  {
    const char *tags = doc["description"]["captions"][i++]["text"];
    if (tags == nullptr)
      break;    
    char *tag;
    tag = strtok((char *)tags, " "); // Split the tags by space
    while (tag != NULL)
    {
      String tagString = String(tag);
      Serial.println(tagString);
      for (int i = 0; i < sizeof(targetTags) / sizeof(targetTags[0]); i++)
      {
        if (strcmp(tagString.c_str(), targetTags[i]) == 0)
        {
          Serial.println(targetTags[i]);
          Serial.println(" >>>>>> detected");    
          detected=true;  
        }
      }
      tags_name.concat(tagString);
      tags_name.concat("_");
      tag = strtok(NULL, " ");
    }
  }
  Serial.println("End of tags");
  Serial.println(tags_name);
  if (detected)
  {
    Serial.println("Buz");
    buz();
  }
  
  return tags_name;
}
