// Load C Libraries
#include "stdlib.h"

// Load Wi-Fi library
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "ThingSpeak.h"

// Load SPI/I2S Libraries
#include <SPI.h>
#include <Audio.h>
#include <SD.h>
#include <FS.h>

// Define SD Card connections
#define SD_CS 5
#define SPI_MOSI 23
#define SPI_MISO 19
#define SPI_SCK 18


// Define I2S connections
#define I2S_DOUT 25
#define I2S_BCLK 27
#define I2S_LRC 26

Audio audio;
File root;
WiFiClient client;

//Music Info Struct
struct Music_info
{
    String filePath;
    String songName;
    String artist;
    String album;
    int length;
    int runtime;
    int volume;
    int status;
    int mute_volume;
} music_info = {"","","","", 0, 0, 0, 0, 0};

//Initialize File List
String file_list[400];
int file_num = 0;
int file_index = 0;
int songCounter = 0;

// Replace with your network credentials
const char *ssid = "***";
const char *password = "***";

//Thingspeak API
const char* writeAPIKey = "***";
const char* readAPIKey = "***";
const long channel2 = ***;

const unsigned int nameFieldTS = 1;
const unsigned int emailFieldTS = 2;
const unsigned int artistFieldTS = 3;
const unsigned int songFieldTS = 4;

//Test user
String userName = "John";
String email = "john@smith.com";

// Set static IP
IPAddress staticIP(192, 168, 1, 18);

// Set web server port number to 80
WebServer server(80);

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String soundOutput = "pause";

// variable to Store SD status
#define SD_ON 1
#define SD_OFF 0
int SD_State = SD_ON; 


// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0;
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

Music_info getSongInfo(String songPath){
  Music_info result;
  
  String str = songPath;
  int start =1 ;  
  int nextDelimeter = str.substring(start,str.length()).indexOf("/")+start;
  result.artist= str.substring(start,nextDelimeter);
  
  start = nextDelimeter+1;
  nextDelimeter = str.substring(start,str.length()).indexOf("/")+start;
  result.album= str.substring(start,nextDelimeter);                              
  
  start = nextDelimeter+1;
  nextDelimeter = str.substring(start,str.length()).indexOf(".")+start;
  result.songName= str.substring(start,nextDelimeter);
  return result;
}


//========================    event handlers        ===============================
void handleListFiles() {

    Serial.println("listing files ...");
  
    // Create a JSON document
    DynamicJsonDocument doc(3072);
    
    // Create a JsonArray for the list of files
    JsonArray songs = doc.createNestedArray("songs");
    
    // Copy the array of songs into the JSON object
    for (int i = 0; i < file_num; i++) {

      //------------------- Parse the string in the file_list to get the artist, album and song name -----------
                                    
      Music_info currentSong = getSongInfo(file_list[i]);
      
      //----------- add song info the JSON responce-----------
      
      JsonObject song = songs.createNestedObject();
    
      // Add the song info to the JsonObject
      song["artist"] = currentSong.artist;
      song["album"] = currentSong.album;
      song["song"] = currentSong.songName;
      song["track_id"]= i;
      
    }


   // Serialize the JSON object to a string
    String json;
    serializeJson(doc, json);
    
    // Send the JSON response
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
  
}

void handleRootRequest() {
  
 if(SD_State == SD_ON)
 {
    // read the HTML file from SD and send it
    File file = SD.open("/music player.html");
    if (!file) {
      Serial.println("Failed to open file");
      server.send(404, "text/plain", "File not found");
      return;
    }
    server.streamFile(file, "text/html");
    file.close();
 }
 else
 {
   
    
 }
}

void handle_track_info(int file_index_or_track_id) {
  Serial.println("PLAY");

    // get song info for the given track ID
    Music_info currentSong = getSongInfo(file_list[file_index_or_track_id]);
 
    // create JSON object with song info
    StaticJsonDocument<256> doc;
    doc["artist"] = currentSong.artist;
    doc["songName"] = currentSong.songName;
    String response;
    serializeJson(doc, response);
    Serial.print("Sending this Json back: ");
    Serial.println(response);

    //Send history to server
    savePlayHistTS(currentSong.songName, currentSong.artist);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", response);
}

void handle_sound_play(){
  
  // check if the track_id parameter was sent in the request
  if (server.hasArg("track_id")) {
    String track_id_str = server.arg("track_id");
    int track_id = track_id_str.toInt();
    Serial.print("Playing track ID: ");
    Serial.println(track_id);
    handle_track_info(track_id);
    open_new_song(file_list[track_id]);
    file_index = track_id;
    handleRootRequest();
	  } else {	
     soundOutput = "play";	
     audio.pauseResume();	
  }
  handleRootRequest();
}

void handle_sound_pause() {
  Serial.println("Pause");
  soundOutput = "pause";
  audio.pauseResume();
  handleRootRequest();

}

void handle_sound_next(){
  Serial.println("Next");
  
  if (file_index < file_num - 1)
    file_index++;
  else
    file_index = 0;
  
  handle_track_info(file_index);
  open_new_song(file_list[file_index]);
  handleRootRequest();
}

void handle_sound_back(){
  Serial.println("Back");
  if (file_index > 0)
    file_index--;
  else
    file_index = file_num - 1;
  handle_track_info(file_index);
  open_new_song(file_list[file_index]);
  handleRootRequest();
}

void handle_style_css(){
  if(SD_State == SD_ON) {
    // Send the style.css file from the SD card
    File file = SD.open("/style.css");
    if (!file) {
      // If the file does not exist, return a 404 error
      server.send(404, "text/plain", "File not found");
      return;
    }
    
    // Send the JavaScript file to the client
    server.streamFile(file, "text/css");
    file.close();
  }
  else {
   
  }
 }


void handle_script_js(){
  if(SD_State == SD_ON)
  {
    // Send the script.js file from the SD card
    File file = SD.open("/script.js");
    if (!file) {
      // If the file does not exist, return a 404 error
      server.send(404, "text/plain", "File not found");
      return;
    }
  
    // Send the JavaScript file to the client
    server.streamFile(file, "application/javascript");
    file.close();
  }
  else {

  }
}
//========================    End of event handlers  ==============================

//Print Directory - Create array of mp3 file paths
int get_music_list(fs::FS &fs, const char *dirname, uint8_t levels, String wavlist[40])
{
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if (!root)
    {
        Serial.println("Failed to open directory");
        return songCounter;
    }
    if (!root.isDirectory())
    {
        Serial.println("Not a directory");
        return songCounter;
    }
 
    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
          if (levels == 2){
            Serial.print("Artist : ");
            }          
          if (levels == 1){
            Serial.print("Album : ");
            }
            Serial.println(file.name());

          if(levels){
                get_music_list(SD, file.path(), levels - 1, file_list);
            }
          }
          else if (!levels){
            String temp = file.path();
            String songName = file.name();
            Serial.print("Song Name : ");
            Serial.println(songName);
            if (songName.endsWith("mp3"))
            {
                wavlist[songCounter] = temp;
                songCounter++;
            }
        }
        file = root.openNextFile();
    }
    return songCounter;
}

//New Song Function
void open_new_song(String filename)
{
    music_info.filePath = filename.substring(0, filename.indexOf("."));
    audio.connecttoFS(SD, filename.c_str());
    music_info.runtime = audio.getAudioCurrentTime();
    music_info.length = audio.getAudioFileDuration();
    music_info.volume = audio.getVolume();
    music_info.status = 1;
    Serial.println("**********Start a new song************");
    
}

//Send play history
void savePlayHistTS(String songNameTS, String artistTS){

ThingSpeak.setField(nameFieldTS, userName);
ThingSpeak.setField(emailFieldTS, email);
ThingSpeak.setField(artistFieldTS, songNameTS);
ThingSpeak.setField(songFieldTS, artistTS);
ThingSpeak.writeFields(channel2, writeAPIKey);

}

void setup()
{
   
    // Set SD Pins

    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    Serial.begin(115200);

    if (!SD.begin(SD_CS)) {
      Serial.println("Error initializing SD card !!!");
      SD_State = SD_OFF;
      file_num = 20;
      // fill temp content to the file list
      for(int i=0; i<file_num; i++)
      {
        file_list[i]= "/artist_"+String(i)+"/album_"+String(i)+"/song_"+String(i)+".mp3";
      }
    }
    else
    {
    SD_State = SD_ON;
    //Read SD and Create File List - get_music+list(Library to use, starting folder, number of sub folders, size of array)
    file_num = get_music_list(SD, "/", 2, file_list);
    Serial.print("Music file count:");
    Serial.println(file_num);
    Serial.println("All music:");
    for (int i = 0; i < file_num; i++)
    {
        Serial.println(file_list[i]);
    }
    }

    // Connect to Wi-Fi network with SSID and password
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    ThingSpeak.begin(client);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
  /*  // Static IP Status
    if (WiFi.config(staticIP, gateway, subnet, dns, dns) == false)
    {
        Serial.println("Configuration failed.");
    }
    */
    // Print local IP address and start web server
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    // -------------------------------- define server handlers -------------------------
    server.on("/list_files", handleListFiles);
    server.on("/", handleRootRequest);
    server.on("/sound/play", handle_sound_play);
    server.on("/sound/pause", handle_sound_pause);
    server.on("/sound/next", handle_sound_next);
    server.on("/sound/back", handle_sound_back);
    server.on("/style.css", handle_style_css);
    server.on("/script.js", handle_script_js);



    // -------------------------------end define server handlers -------------------------
    Serial.println("starting server ... ");
    server.begin();    
    Serial.println("Server is up and listening on port 80");
    
    // Set idle timeout to 30 seconds
    // server.setIdleTimeout(30);
   

    // Connect to I2S Module + Internet Radio
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(10);
    // audio.connecttoFS(SD, "/Rolling Stones, The/Beggars Banquet/01 Sympathy For The Devil.mp3");

    //Random song on open, to develop into function
    file_index = 0;

    open_new_song(file_list[file_index]);
}

void loop()
{
   server.handleClient();

   audio.loop();


}
