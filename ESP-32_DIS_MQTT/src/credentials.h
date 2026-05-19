/*Made by Mike Eitel to store device credenitals on one place
  No rights reserved when private use. Otherwise contact author.

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
  and associated documentation files. The above copyright notice and this permission notice shall 
  be included in all copies or substantial portions of the software.
*/

// Wifi acess definitions of network to connect to  
#define wifi_ssid "xxx"
#define wifi_password "yyy"

#define myIP "100"                                            // Device ID

// Wifi definitions of this device  
//IPAddress staticIP(192,168,x,atoi(myIP));                      // IOT device IP
IPAddress staticIP(1,1,1,atoi(myIP));       // REPLACE !!!!
IPAddress subnet(255,255,255,0);                               // Network subnet size
//IPAddress gateway(192,168,x,y);                                // Network router IP
IPAddress gateway(1,1,1,1);                // REPLACE !!!!

// Mosquitto MQTT Broker definitions
#define mqtt_server    "192.168.x.z"                           // IOT MQTT server IP
#define mqtt_user      "admin"
#define mqtt_password  "admin"
#define mqtt_port      1883
#define WiFi_timeout    100                                    // How many times to try before give up
#define mqtt_timeout     10                                    // How many times to try before try Wifi reconnect


// MQTT Topics
#define mytype         "esp/32S-DIS-"             // Client Typ
#define iamclient      mytype myIP                // Client name 
#define in_topic       iamclient "/command"       // This is received from MQTT
#define out_param      iamclient "/sensors"       // This is send to MQTT
#define out_error      iamclient "/status"        // This is send to MQTT
#define out_topic      iamclient "/loop"          // This is send to MQTT
#define out_watchdog   iamclient "/watchdog"      // This is send to MQTT
#define out_button     iamclient "/button"        // This is send to MQTT

// error =  -1    Wrong command received
// error =   0    Normal status
// error =   1    First time connected
// error =   2    Reconnect succesfull
// error =   3    Not implemented

// Constant how often the mqtt message is send
#if defined(TEST)
  const long interval =  1000;                    // Interval at which to publish sensor readings
#else
  const long interval = 1000; // 5000;                   // Interval at which to publish sensor readings
#endif