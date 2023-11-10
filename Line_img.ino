#include "VideoStream.h"
#include "AmebaFatFS.h"
#include "WiFi.h"


#define CHANNEL 0
#define FILENAME "image.jpg"

// Enter your WiFi ssid and password
char ssid[] = "Yamato";       // your network SSID (name)
char pass[] = "15926535";  // your network password
int status = WL_IDLE_STATUS;
String Linetoken = "MIKU_88888888888888888888888888888888888888";  //Line Notify Token. You can set the value of xxxxxxxxxx empty if you don't want to send picture to Linenotify.


int PicID = 0;

uint32_t img_addr = 0;
uint32_t img_len = 0;

AmebaFatFS fs;
VideoSetting config(640,480, 10, VIDEO_JPEG, 1);

void setup() {
  Serial.begin(115200);

  // WiFi init
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    // wait 2 seconds for connection:
    delay(2000);
  }
  // camera init
  Camera.configVideoChannel(CHANNEL, config);
  Camera.videoInit();
  Camera.channelBegin(CHANNEL);
  Serial.println("");
  Serial.println("===================================");

  // SD card init
  if (!fs.begin()) Serial.println("記憶卡讀取失敗，請檢查記憶卡是否插入...");
  else Serial.println("記憶卡讀取成功");
}

void loop() {
  String payload = SendImageLine("拍照");
  Serial.println(payload);
  delay(5000);
}

//傳送照片到Line
String SendImageLine(String msg) {
  int buf=4096;
  Camera.getImage(CHANNEL, &img_addr, &img_len);

  WiFiSSLClient client_tcp;
  if (client_tcp.connect("notify-api.line.me", 443)) {
    Serial.println("連線到Line成功");
    //組成HTTP POST表頭
    String head = "--Cusboundary\r\nContent-Disposition: form-data;";
    head += "name=\"message\"; \r\n\r\n" + msg + "\r\n";
    head += "--Cusboundary\r\nContent-Disposition: form-data; ";
    head += "name=\"imageFile\"; filename=\"img.jpg\"";
    head += "\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--Cusboundary--\r\n";
    uint32_t imageLen = img_len;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;
    //開始POST傳送
    client_tcp.println("POST /api/notify HTTP/1.1");
    client_tcp.println("Connection: close");
    client_tcp.println("Host: notify-api.line.me");
    client_tcp.println("Authorization: Bearer " + Linetoken);
    client_tcp.println("Content-Length: " + String(totalLen));
    client_tcp.println("Content-Type: multipart/form-data; boundary=Cusboundary");

    client_tcp.println();
    client_tcp.print(head);
    uint8_t *fbBuf = (uint8_t *)img_addr;
    size_t fbLen = img_len;
    Serial.println("傳送影像檔...");
    for (size_t n = 0; n < fbLen; n = n + buf) {
      if (n + buf < fbLen) {
        client_tcp.write(fbBuf, buf);
        fbBuf += buf;
      } else if (fbLen % buf > 0) {
        size_t remainder = fbLen % buf;
        client_tcp.write(fbBuf, remainder);
      }
    }
    client_tcp.print(tail);
    client_tcp.println();
    String payload = "";
    boolean state = false;
    int waitTime = 5000;  //等候時間5秒鐘
    long startTime = millis();
    delay(1000);
    Serial.print("等候回應...");
    while ((startTime + waitTime) > millis()) {
      Serial.print(".");
      delay(100);
      while (client_tcp.available() && (startTime + waitTime) > millis()) {
        //已收到回覆，依序讀取內容
        char c = client_tcp.read();
        payload += c;
      }
    }
    client_tcp.stop();
    return payload;
  } else {
    return "傳送失敗，請檢查網路設定";
  }
}

void SaveImgTF(String filePathName) {
  File file = fs.open(filePathName);
  Camera.getImage(CHANNEL, &img_addr, &img_len);
  boolean a = file.write((uint8_t *)img_addr, img_len);
  file.close();
  if (a) Serial.println(filePathName + " 存檔成功...");
  else Serial.println("存檔失敗，請檢查記憶卡...");
}
