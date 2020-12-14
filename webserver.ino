#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>

// size of buffer used to capture HTTP requests
#define REQ_BUF_SZ   50

// MAC address from Ethernet shield sticker under board
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 180); // IP address, may need to change depending on network
EthernetServer server(80);  // create a server at port 80
File webFile;               // the web page file on the SD card
char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string
char req_index = 0;              // index into HTTP_req buffer


int rotatorLeft = 8;
int rotatorRight = 9;
int switchOnOff = 7;

int angel, newAzimuth;

String setDirection = "stop";
bool powerStatusBool = false;

//filter
int analog_val;
int val[3];
int val_filter;
byte index;
String alarmMsg;

void setup()
{    
    alarmMsg = "";
    newAzimuth = 180;
    angel = 0;
    // disable Ethernet chip
    pinMode(10, OUTPUT);
    digitalWrite(10, HIGH);

    Serial.begin(9600);       // for debugging
    
    // initialize SD card
    Serial.println("Initializing SD card...");
    if (!SD.begin(4)) {
        Serial.println("ERROR - SD card initialization failed!");
        return;    // init failed
    }
    Serial.println("SUCCESS - SD card initialized.");
    // check for index.htm file
    if (!SD.exists("index.htm")) {
        Serial.println("ERROR - Can't find index.htm file!");
        return;  // can't find index file
    }
    Serial.println("SUCCESS - Found index.htm file.");
    
    Ethernet.begin(mac, ip);  // initialize Ethernet device
    server.begin();           // start to listen for clients

    pinMode (rotatorLeft, OUTPUT); //CCW 
    pinMode (rotatorRight, OUTPUT);//CW
    pinMode (switchOnOff, OUTPUT); //ON-OFF 
    
}

void loop()
{
  
     //data
     if (++index > 2) 
        index = 0; // переключаем индекс с 0 до 2 (0, 1, 2, 0, 1, 2Е)
     // read analog pin A0
     val[index] = analogRead(0); // записываем значение с датчика в массив
     // фильтровать медианным фильтром из 3Єх ѕќ—Ћ≈ƒЌ»’ измерений
     analog_val = middle_of_3(val[0], val[1], val[2]);
    //data end

    //prevent uncontrolled rotation
    alarmMsg = sequreParams();
    
    EthernetClient client = server.available();  // try to get client
  
    if (client) {  // got client?
        boolean currentLineIsBlank = true;
        while (client.connected()) {
            if (client.available()) {   // client data available to read
                char c = client.read(); // read 1 byte (character) from client
                // buffer first part of HTTP request in HTTP_req array (string)
                // leave last element in array as 0 to null terminate string (REQ_BUF_SZ - 1)
                if (req_index < (REQ_BUF_SZ - 1)) {
                    HTTP_req[req_index] = c;          // save HTTP request character
                    req_index++;
                }
                // last line of client request is blank and ends with \n
                // respond to client only after last line received
                if (c == '\n' && currentLineIsBlank) {
                    // send a standard http response header
                    client.println("HTTP/1.1 200 OK");

                    if (StrContains(HTTP_req, "start")){                                  
                        client.println("POWER ON");
                        //relay ON
                         digitalWrite (switchOnOff, HIGH);
                         powerStatusBool = true;                                          
                         //STOP (just in case)
                        digitalWrite (rotatorLeft, LOW);
                        digitalWrite (rotatorRight, LOW);
                    
                    }else if (StrContains(HTTP_req, "finish")){                                       
                        powerStatusBool = false;                            
                        digitalWrite (rotatorLeft, LOW);
                        digitalWrite (rotatorRight, LOW);
                        digitalWrite (switchOnOff, LOW);                        
                        setDirection = "stop";
                        client.println("POWER OFF");
                    }else if (StrContains(HTTP_req, "left")){                       
                      client.println("ROTATION: LEFT");
                       setDirection = "left";
                       digitalWrite (rotatorRight, LOW);
                       digitalWrite (rotatorLeft, HIGH);
                       
                    }else if (StrContains(HTTP_req, "right")){                       
                      client.println("ROTATION: RIGHT");
                       setDirection = "right";
                       digitalWrite (rotatorLeft, LOW);
                       digitalWrite (rotatorRight, HIGH);
                       
                    }else if (StrContains(HTTP_req, "stop")) {
                       client.println("ROTATION: STOP!");
                       setDirection = "stop";
                       newAzimuth = NULL;
                       digitalWrite (rotatorLeft, LOW);
                       digitalWrite (rotatorRight, LOW);
                       
                    } else if (StrContains(HTTP_req, "azimuth"))   {                       
                                            
                      newAzimuth = getAzimuth(HTTP_req);
                      
                      
                      if (!newAzimuth || newAzimuth >= 360 || newAzimuth <= 0){                                                
                        setDirection = "stop";                        
                        digitalWrite (rotatorLeft, LOW);
                        digitalWrite (rotatorRight, LOW);    
                      } else {
                        if (newAzimuth > angel)  {
                          setDirection = "right";
                          digitalWrite (rotatorLeft, LOW);
                          digitalWrite (rotatorRight, HIGH);
                        } else {
                          setDirection = "left";
                          digitalWrite (rotatorRight, LOW);
                          digitalWrite (rotatorLeft, HIGH);
                        }
                      }  
                       
                    } else if (StrContains(HTTP_req, "ajax_inputs")) {
                      // remainder of header follows below, depending on if
                    // web page or XML page is requested
                    // Ajax request - send XML file
                        // send rest of HTTP header
                        client.println(F("Content-Type: text/xml"));
                        client.println(F("Connection: keep-alive"));
                        client.println();
                        // send XML file containing input states                        
                        XML_response(client);
                    
                    } else {  
                        // web page request
                        // send rest of HTTP header
                        client.println(F("Content-Type: text/html"));
                        client.println(F("Connection: keep-alive"));
                        client.println();
                        

                        // HTTP request for web page
                        // send web page - contains JavaScript with AJAX calls
                        client.println(F("<!DOCTYPE html>"));
                        client.println(F("<html lang='en'>"));
                        client.println(F("<head>"));
                        client.println(F("<title>Arduino Web Page</title>"));
                        client.println(F("<style>"));                        
                        client.println(F(".btn {width: 120px; height: 50px; font-size: 18px; -webkit-appearance: none; background-color:#dfe3ee; }"));
                        client.println(F(".btn_small { -webkit-appearance: none; background-color:#dfe3ee; }"));
                        client.println(F(".active {width: 140px; height: 50px; font-size: 22px; -webkit-appearance: none; background-color:red; color:white }"));
                        client.println(F("</style>"));
                        client.println(F("<script src=\"//cdn.rawgit.com/Mikhus/canvas-gauges/gh-pages/download/2.1.7/radial/gauge.min.js\"></script>"));
                        client.println(F("<script>"));                        
                        client.println(F("var nocache = \"&nocache=\" + Math.random() * 1000000;"));
                        client.println(F("var request = new XMLHttpRequest();"));
                        client.println(F("function doCmd(param) {"));    
                        client.println(F("lightCheck(param);"));                            
                        client.println(F("if (param == 'azimuth') {value = document.getElementById(\"azimuth_value\").value;} else {value = 1;}"));                                   
                        client.println(F("request.onreadystatechange = function() {"));
                        client.println(F("if (this.readyState == 4) {"));
                        client.println(F("if (this.status == 200) {"));
                        client.println(F("if (this.responseText != null) {"));
                        //client.println(F("document.getElementById(\"status_txt\").innerHTML = this.responseText;"));
                        client.println(F("}}}}"));
                        client.println(F("request.open(\"GET\", param +\"=\"+value + nocache, true);"));                        
                        //???                        
                        client.println(F("request.send(null);"));
                        client.println(F("if (param == 'start' || param == 'finish' || param == 'azimuth'){ setTimeout(location.reload(), 1500);}"));
                        client.println(F("}"));
                        client.println(F("</script>"));
                        client.println(F("</head>"));                        
                        client.println(F("<h1>Ethernet Controller G450A</h1>"));
                        //client.println(F("<p id=\"status_txt\">Awaiting...</p>"));                       
                        client.print(F("<br/><br/>"));

                          if (alarmMsg.length() > 1) {
                            client.println(F("<script>alert("));
                            client.println(alarmMsg);
                            client.println(F(")</script>"));
                            alarmMsg = "";
                          }
                        
                          if (!powerStatusBool) {
                            client.println(F("<button class=\"btn\" onclick=\"doCmd('start')\">START</button>"));                            
                          } else {                                  
                                  if (setDirection.compareTo("left") == 0) {
                                     client.println(F("<button class=\"active\" id=\"btn_1\" onclick=\"doCmd('left')\">LEFT</button>"));
                                  } else {
                                     client.println(F("<button class=\"btn\" id=\"btn_1\" onclick=\"doCmd('left')\">LEFT</button>"));
                                  }
        
                                  if (setDirection.compareTo("stop") == 0) {
                                     client.println(F("<button class=\"active\" id=\"btn_2\" onclick=\"doCmd('stop')\">STOP</button>"));
                                  } else {
                                     client.println(F("<button class=\"btn\" id=\"btn_2\" onclick=\"doCmd('stop')\">STOP</button>"));
                                  }
        
                                  if (setDirection.compareTo("right") == 0) {
                                     client.println(F("<button class=\"active\" id=\"btn_3\" onclick=\"doCmd('right')\">RIGHT</button>"));
                                  } else {
                                     client.println(F("<button class=\"btn\" id=\"btn_3\" onclick=\"doCmd('right')\">RIGHT</button>"));
                                  }
                                  
                                  client.println(F("<button class=\"btn\" onclick=\"doCmd('finish')\">SHUTDOWN</button></div>"));
                          
                                  client.println(F("<br/><br/>"));
                                  client.println(F("New Azimuth: <input type=\"text\" value="));                                                                     
                                  client.println(newAzimuth > 0 ? newAzimuth : 180 );                                  
                                  client.println(F(" id=\"azimuth_value\" /><button class=\"btn_small\" onclick=\"doCmd('azimuth')\" id=\"goBtn\">Go!</button>"));                                                                            
                        }
                        client.print(F("<br/><br/>"));
                        delay(100);      // give the web browser time to receive the data
                        
                        
                        // send web page
                        webFile = SD.open("index.htm");        // open web page file
                        if (webFile) {
                            while(webFile.available()) {
                                client.write(webFile.read()); // send web page to client
                            }
                            webFile.close();
                        }
                    }
                    // display received HTTP request on serial port
                    //Serial.print(HTTP_req);
                    // reset buffer index and all buffer elements to 0
                    req_index = 0;
                    StrClear(HTTP_req, REQ_BUF_SZ);
                    break;
                }
                // every line of text received from the client ends with \r\n
                if (c == '\n') {
                    // last character on line of received text
                    // starting new line with next character read
                    currentLineIsBlank = true;
                } 
                else if (c != '\r') {
                    // a text character was received from client
                    currentLineIsBlank = false;
                }
            } // end if (client.available())
        } // end while (client.connected())
        delay(100);      // give the web browser time to receive the data
        client.stop(); // close the connection
    } // end if (client)
}

// send the XML file containing analog value
void XML_response(EthernetClient cl)
{
        
    cl.print(F("<?xml version = \"1.0\" ?>"));
    cl.print(F("<inputs>"));        
    //angel fix
    angel = round(analog_val/1.9);
    cl.print(F("<analog>"));
    cl.print(angel);
    cl.print(F("</analog>"));
    cl.print(F("</inputs>"));
}

// медианный фильтр из 3Єх значений
int middle_of_3(int a, int b, int c) {
  int middle;
  if ((a <= b) && (a <= c)) {
    middle = (b <= c) ? b : c;
  }
  else {
    if ((b <= a) && (b <= c)) {
      middle = (a <= c) ? a : c;
    }
    else {
      middle = (a <= b) ? a : b;
    }
  }
  return middle;
}

// sets every element of str to 0 (clears array)
void StrClear(char *str, char length)
{
    for (int i = 0; i < length; i++) {
        str[i] = 0;
    }
}

int getAzimuth(char *str)
{
  char resultRow[3];  
  int getAzimuth;
  resultRow[0] = str[13];
  resultRow[1] = str[14];
  resultRow[2] = str[15];      
  getAzimuth = atoi(resultRow);  
  return getAzimuth;
}

// searches for the string sfind in the string str
// returns 1 if string found
// returns 0 if string not found
char StrContains(char *str, char *sfind)
{
  
    char found = 0;
    char index = 0;
    char len;

    len = strlen(str);
    
    if (strlen(sfind) > len) {
        return 0;
    }
    while (index < len) {
        if (str[index] == sfind[found]) {
            found++;
            if (strlen(sfind) == found) {
                return 1;
            }
        }
        else {
            found = 0;
        }
        index++;
    }

    return 0;
}

String sequreParams() {
  String msg;
 
  //sequring  
  if (angel > 360 || angel < 0) {
     msg = 'STOP: out of range!';
     Serial.println("//STOP: out of range!");    
     setDirection = "stop";
     digitalWrite (rotatorLeft, LOW);
     digitalWrite (rotatorRight, LOW);
  }

  //presession stop
  if (newAzimuth != NULL) {
    if ((setDirection.compareTo("left") == 0 && angel <= newAzimuth+3) || (setDirection.compareTo("right") == 0  && angel >= newAzimuth-3)) {
      msg = 'STOP: stop afetr go!';
      Serial.println("//STOP after GO");    
      setDirection = "stop";     
      digitalWrite (rotatorLeft, LOW);
      digitalWrite (rotatorRight, LOW);    
    }
  }

  if (msg.length() > 1) {
    return msg;
  }  
}
