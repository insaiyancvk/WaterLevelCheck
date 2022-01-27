#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// wifi credentials
// ASSUMPTION: THE WIFI IS NEVER DOWN - nobody cares
const char* ssid = "<SSID>";
const char* password = "<PASSWORD>";

#define BOTtoken "<TELEGRAM BOT TOKEN>" // telegram bot token
#define CHAT_ID "<TELEGRAM CHAT ID>"    // chat ID - Group ID

const unsigned long BOT_MTBS = 1000;    // mean time between scan messages
unsigned long bot_lasttime;             // last time messages' scan has been done
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

int horizontal, vertical, contactless;
int ps = 0;
int pv = 0;
int wifi_check = 0;

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
}

void handleNewMessages(int numNewMessages)
{
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);
  
  String answer = "";
  for (int i = 0; i < numNewMessages; i++)
  {
    telegramMessage &msg = bot.messages[i];
    Serial.println("Received " + msg.text);
    if (msg.text == "/help@WaterSenseBot")
      answer = "So you need _help_, uh? me too! use /start or /status";
    else if (msg.text == "/start@WaterSenseBot")
      answer = "Welcome my new friend! You are the first *" + msg.from_name + "* I've ever met";
    else if (msg.text == "/status@WaterSenseBot")
    {
      horizontal = digitalRead(4);
      vertical = digitalRead(5);
      contactless = digitalRead(14);
    
      horizontal = !horizontal;
      vertical = !vertical;
      contactless = !contactless;

      if(vertical == HIGH)
          answer += "Sump Status: _ENOUGH_\n";
      else if (vertical == LOW)
          answer += "Sump Status: *LOW*\n";
        
      if(horizontal == HIGH) 
          answer += "Tank Status: _FULL_\n";
      else if(contactless == LOW)
          answer += "Tank Status: *LOW*\n";
      else if(horizontal == LOW && contactless == HIGH)
          answer += "Tank Status: More than *LOW* but less than _FULL_\n";
      
      if(digitalRead(12) == HIGH)
          answer += "Motor Status: *ON*\n";
      else if(digitalRead(12) == LOW)
          answer += "Motor Status: _OFF_\n";
      }
    else
      answer = "Say what? _"+msg.text+"_";

    bot.sendMessage(CHAT_ID, answer, "Markdown");
  }
}


void bot_setup()
{
  const String commands = F("["
                            "{\"command\":\"help\",  \"description\":\"Get bot usage help\"},"
                            "{\"command\":\"start\", \"description\":\"Message sent when you open a chat with a bot\"},"
                            "{\"command\":\"status\",\"description\":\"Answer device current status\"}" // no comma on last command
                            "]");
  bot.setMyCommands(commands);
}

void setup() {

  Serial.begin(115200);
  configTime(0, 0, "pool.ntp.org");      // get UTC time via NTP  ********IMPORTANT FOR TELEGRAM BOT********
  client.setTrustAnchors(&cert);         // Add root certificate for api.telegram.org
  Serial.print("Connecting Wifi: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(1000);
      if(wifi_check == 20)
        break;
      wifi_check++;
    }
  
  if(WiFi.status() == WL_CONNECTED)
  {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    Serial.println(WiFi.status());
    bot.sendMessage(CHAT_ID, "Bot started up!","");

    bot.sendMessage(CHAT_ID, "Use /status to get the current status of Motor, Sump and Tank levels","");

  }
  pinMode(4, INPUT_PULLUP);   //D2 - Horizontal water switch
  pinMode(5, INPUT_PULLUP);   //D1 - Vertical water switch
  pinMode(14,INPUT_PULLUP);   //D5 - ContactLess sensor
  pinMode(12, OUTPUT);        // D6 - Relay Signal

  Serial.print("Retrieving time: ");
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);

  bot_setup();
}

void loop() {

  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }
  
  horizontal = digitalRead(4);
  vertical = digitalRead(5);
  contactless = digitalRead(14);
  
  horizontal = !horizontal;
  vertical = !vertical;
  contactless = !contactless;
  
//  Serial.println();
//  Serial.print("\tHorizontal switch: ");
//  Serial.print(horizontal);
//  Serial.print("\tVertical switch: ");
//  Serial.print(vertical);
//  Serial.print("\tContactless sensor: ");
//  Serial.print(contactless);
//  Serial.println();
  
  if(vertical == HIGH) {

    if(horizontal == HIGH) {   // Full tank 
        digitalWrite(12, LOW); // Relay LOW
        ps = 2;
        if(pv!=ps){
          if(WiFi.status() == WL_CONNECTED){
            bot.sendMessage(CHAT_ID, "TANK FULL!!\nTurning OFF the water motor\n", "");
            }
          else
            initWiFi();
          pv = 2;
        }
      }

      else if(horizontal == LOW && contactless == HIGH) // Below full tank but not empty
      {
            ps = 6;
              if(pv!=ps) {
                if(WiFi.status() == WL_CONNECTED)
                  bot.sendMessage(CHAT_ID, "There's enough water in tank and sump\n", "");
              pv = 6;
              }
        }
        

      else if(contactless == LOW) {   // Low water tank
        digitalWrite(12, HIGH); // Relay HIGH
        ps = 1;
        if(pv!=ps) {
          if(WiFi.status() == WL_CONNECTED)
            bot.sendMessage(CHAT_ID, "NO WATER IN TANK!!\nTurning ON the water motor\n", "");
          pv = 1;
        }
      }

      
    }
  else if(vertical == LOW) {
      digitalWrite(12, LOW); // Relay LOW

      if(horizontal == HIGH) {
        ps = 4;
        if(pv!=ps) {
          if(WiFi.status() == WL_CONNECTED)
            bot.sendMessage(CHAT_ID, "NO WATER IN SUMP!!\nTank is full. Safe for now\n", "");
          else
            initWiFi();
          pv = 4;
        }
      }
      
      else if(contactless == LOW) {
        ps = 3;
        if(pv!=ps){
          if(WiFi.status() == WL_CONNECTED)
            bot.sendMessage(CHAT_ID, "NO WATER IN TANK AND SUMP\nGet the sump filled ASAP or consider using water from the other tank\n", "");
          else
            initWiFi();
          pv = 3;
        }
      }

      else {
          ps = 5;
          if(pv!=ps) {
            if(WiFi.status() == WL_CONNECTED)
              bot.sendMessage(CHAT_ID, "NO WATER IN SUMP!!\nTank is neither full nor empty. Safe for now\nConsider using water from the other tank", "");
            else
              initWiFi();
            pv = 5;
            }
        }
    }
  if(WiFi.status() != WL_CONNECTED){
    initWiFi();
    }
}