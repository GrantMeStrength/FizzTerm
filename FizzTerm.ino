/*
  FizzTerm - A Simple Serial File System Manager

  Use it to record and play back serial data to an SD card. It's particularly
  useful if using a retro computer that communicates over RS232 and you
  need to capture program listings to disk.

  You can also access the SD card on your computer, edit them there,
  and then load them back into your retro system.
  
  Requires an Aduino Mega or compatible, with multiple serial port support.

  Assumes that MAX232-type adaptors are connected to the Arduino's default
  Serial1 and Serial2 ports, and the SD Card reader is connected in the usual
  way (although the Chip Select pin may depend on your hardware).

  Note: In order to make LIST command work, you must duplicate the SD library
  and change all the references to Serial. to Serial1. If you don't,
  when you type LIST you won't see the list of files on your card.

  Note: Baud rate is set at 9600. You may need to change this.

  v0.3 = Toggle between 9600 and 115200 with the baud keyword
  v0.3 = The command checking is no longer case-sensitive
  
*/


#include <SPI.h>
#include <SD.h>

// Styles

#define GREEN "\033[0;32m"
#define WHITE "\033[1;37m"

// Set up variables using the SD utility library functions:
Sd2Card card;
SdVolume volume;
SdFile root;
const int chipSelect = 53;  // Might be different on your hardware

// Variables used by this app
long BAUDRATE = 9600;
bool HIDE_ECHO = false;
bool RECORDING = false;
char buffer[80];
char buffer_count = 0;
File myFile;


void setup() {

  //Serial.begin(BAUDRATE); // For debugging
  Serial1.begin(BAUDRATE); // The Terminal
  Serial2.begin(BAUDRATE); // The Computer

  Serial1.println(GREEN);
  Serial1.println("  ______ _      _______                  ");
  Serial1.println(" |  ____(_)    |__   __|                 ");
  Serial1.println(" | |__   _ _______| | ___ _ __ _ __ ___  ");
  Serial1.println(" |  __| | |_  /_  / |/ _ \\ '__| '_ \\` _ \\ ");
  Serial1.println(" | |    | |/ / / /| |  __/ |  | | | | | |");
  Serial1.println(" |_|    |_/___/___|_|\\___|_|  |_| |_| |_|\n");
  Serial1.println("                                      v0.3\n");
  Serial1.println(WHITE);

 
  //Serial.println("Debugging. v0.2\n");
 
  if (!card.init(SPI_HALF_SPEED, chipSelect)) {
    Serial1.println("Error: SD Card was not found.");
    //while (1); // You may not want the terminal to hang with no SD card present.
  }

  // Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
  if (!volume.init(card)) {
    Serial1.println("Error: SD Card was found, but could not be read.");
    //while (1); // You may not want the terminal to hang with no SD card present.
  }

  // Clear buffer
  for (int i = 0; i < 80; i++)
    buffer[i] = 0;
}


// Let the user enter a line of text from the terminal, and echo it back to the terminal.
// If it's a backspace, delete the character previous entered.
// Check for <0 or >80 characters.
// When the user enters NEWLINE, well, then we do stuff.
// If you wanted to add support for cursor up and down to go through a history, then
// this is where you would do it.

void Enter_Line()
{
  if (Serial1.available())
  {

    // Read in and echo back to terminal. Don't send to computer yet.

    int inByte = Serial1.read();
    Serial1.write(inByte);
    buffer[buffer_count] = inByte;
    bool found_command = false;

    if (inByte == 3 && RECORDING)
    {
      RECORDING = false;
      myFile.close();
      Serial1.print(GREEN);
      Serial1.println("Saving complete.");
      Serial1.print(WHITE);
    }


    if (inByte == 8) // User pressed delete
    {
      buffer_count--;
      if (buffer_count < 0)
      {
        buffer_count = 0;
      }
    }
    else
    {
      buffer_count++;
      if (buffer_count >= 80)
      {
        buffer_count = 79;
      }
    }

    if (inByte == 13) // User pressed return
    {
      // Check for commmands

      buffer[buffer_count - 1] = 0; // Get rid of the newline character in there

      if (strncasecmp(buffer, "!HELP", 5) == 0)
      {
        FS_HELP();
        found_command = true;
      }

      if (strncasecmp(buffer, "!BAUD", 5) == 0)
      {
        FS_BAUD();
        found_command = true;
      }

      if (strncasecmp(buffer, "!LIST", 5) == 0)
      {
        FS_LIST();
        found_command = true;
      }

      if (strncasecmp(buffer, "!SAVE", 5) == 0)
      {
        char filename[13];
        for (int i = 6; i < 18; i++)
          filename[i - 6] = buffer[i];
        filename[12] = 0;
        FS_SAVE(filename);
        found_command = true;
      }

      if (strncasecmp(buffer, "!LOAD", 5) == 0)
      {
        char filename[13];
        for (int i = 6; i < 18; i++)
          filename[i - 6] = buffer[i];
        filename[12] = 0;
        FS_LOAD(filename);
        found_command = true;
      }


      if (strncasecmp(buffer, "!WIPE", 4) == 0) // Yes, DEL or ERA would have been better but I'm being lazy and assuming 5 chars
      {
        char filename[13];
        for (int i = 6; i < 18; i++)
          filename[i - 6] = buffer[i];
        filename[12] = 0;
        FS_WIPE(filename);

        found_command = true;
      }

      if (strncasecmp(buffer, "!DUMP", 5) == 0)
      {
        char filename[13];
        for (int i = 6; i < 18; i++)
          filename[i - 6] = buffer[i];
        filename[12] = 0;
        FS_DUMP(filename);
        found_command = true;
      }

      if (strncasecmp(buffer, "!STOP", 5) == 0)
      {

        if (RECORDING)
        {
          RECORDING = false;
          myFile.close();
          Serial1.print(GREEN);
          Serial1.println("Saving complete.");
          Serial1.println(WHITE);
        }
        else
        {
          Serial1.print(GREEN);
          Serial1.println("Error: Nothing was being saved.");
          Serial1.println(WHITE);
        }

        found_command = true;
      }

      if (found_command == true)
      {
        buffer_count = 0;
        for (int i = 0; i < 80; i++)
          buffer[i] = 0;
        return;
      }

      buffer[buffer_count - 1] = 13; // Add back the newline character so computer acts on text

      // No commands found, so a regular string to pass onto computer

      HIDE_ECHO = true; // Don't display the text the computer spits back that includes the line we just sent.
      // Why? Sometimes there is a > prompt for example, and that would mess up what the user
      // sees.

      Serial1.write(13);
      Serial2.write(buffer);
      buffer_count = 0;
      for (int i = 0; i < 80; i++)
        buffer[i] = 0;
    }
  }
}

// This displays the information coming from the computer. It's also where we save it to the SD
// card if that option is currenly active.

void Display_From_Computer()
{
  if (Serial2.available())
  {

    // Read in and echo back
    int inByte = Serial2.read();

    if (HIDE_ECHO == false)
    {
      Serial1.write(inByte);

      if (RECORDING) // Save any data from the computer to an open file on the SD card.
      {
        myFile.write(inByte);
      }

    }

    if (inByte == 13) // Wait to echo text back until we get the CR, so absorb the echo of the sent string
    {
      HIDE_ECHO = false;
    }
  }
}

void loop()
{
  Enter_Line();
  Display_From_Computer();   
}




void FS_HELP()
{
  Serial1.println(GREEN);
  Serial1.println();
  Serial1.println("FIZZTERM");
  Serial1.println();
  Serial1.println("!HELP       This helpful text.");
  Serial1.println("!BAUD       Toggle the bit rate between 9600 and 115200.");
  Serial1.println("!LIST       List the files stored on the SD card.");
  Serial1.println("!LOAD file  Load the named file, sending it directly to the connected computer.");
  Serial1.println("!DUMP file  Display the contents the named file, but only to the terminal.");
  Serial1.println("!SAVE file  Save incoming data to SD from the computer until !STOP or CTRL C.");
  Serial1.println("!STOP       Stop saving data to file.");
  Serial1.println("!WIPE file  Delete the named file from the SD card.");
  Serial1.println(WHITE);
}


void FS_DUMP(const char * filename)
{

  Serial1.println(GREEN);

  if (!SD.begin(chipSelect))
  {
    Serial1.println("Error: Card failed, or not present");
    Serial1.println(WHITE);
    return;
  }

  if (!SD.exists(filename))
  {
    Serial1.println("Error: File not found");
    Serial1.println(WHITE);
    return;
  }

  File dataFile = SD.open(filename);

  // if the file is available, read from it
  if (dataFile)
  {
    while (dataFile.available())
    {
      Serial1.write(dataFile.read());
    }
    dataFile.close();
  }
  else
  {
    Serial1.println("Error: Could not open file");
  }
  Serial1.println(WHITE);
}


void FS_LOAD(const char* filename)
{

  Serial1.println(GREEN);

  if (!SD.begin(chipSelect))
  {
    Serial1.println("Error: Card failed, or not present");
    Serial1.println(WHITE);
    return;
  }

  if (!SD.exists(filename))
  {
    Serial1.println("Error: File not found");
    Serial1.println(WHITE);
    return;
  }



  File dataFile = SD.open(filename);

  Serial1.write("Loading..");

  // if the file is available, read from it
  if (dataFile) {
    while (dataFile.available()) {
      Serial2.write(dataFile.read());
      delay(10); // Little pause to give the computer time to digest the data. May need increased if computer is slow

      // Ignore any echo'd chars from computer
      if (Serial2.available()) {
        char inByte = Serial2.read();
      }
    }
    dataFile.close();
  }

  // if the file isn't open, display error
  else {
    Serial1.println("Error: Could not open file");
    Serial1.println(WHITE);
    return;
  }

  Serial1.write("OK");
  Serial1.println(WHITE);

}

void FS_BAUD()
{
  //
  // Toggle between 9600 and 115200
  // But you could change this to cycle between all possible speeds if required.
  //
  Serial.flush();
  Serial1.flush();
  Serial2.flush();
  Serial.end();
  Serial1.end();
  Serial2.end();

  delay(2500);

  if (BAUDRATE == 9600)
  {
    BAUDRATE = 115200;
  }
  else
    BAUDRATE = 9600;
  }

  delay(2500);

  setup();
}

void FS_LIST()
{

  Serial1.println(GREEN);

  if (!SD.begin(chipSelect)) {
    Serial1.println("Error: Card failed, or not present");
    Serial1.println(WHITE);
    return;
  }


  Serial1.println("Card contents (name, date and size in bytes):");
  root.openRoot(volume);

  // list all files in the card with date and size
  root.ls(LS_R | LS_DATE | LS_SIZE);

  // Turn off the green style
  Serial1.println(WHITE);

}

void FS_WIPE(const char *filename)
{
  Serial1.println(GREEN);

  if (!SD.begin(chipSelect)) {
    Serial1.println("Error: Card failed, or not present");
    Serial1.println(WHITE);
    return;
  }

  if (!SD.exists(filename))
  {

    Serial1.print("Error: File not found");
    Serial1.println(WHITE);
    return;
  }


  bool fail = !SD.remove(filename);

  if (fail)
  {
    Serial1.println("Error: file could not be deleted");
  }
  else
  {
    Serial1.println("File deleted");
  }

  Serial1.println(WHITE);


}

void FS_SAVE(const char* filename)
{

  Serial1.println(GREEN);

  if (!SD.begin(chipSelect)) {
    Serial1.println("Error: Card failed, or not present");
    Serial1.println(WHITE);
    return;
  }

  if (strlen(filename) < 1)
  {
    Serial1.println("Error: missing filename");
    Serial1.println(WHITE);
    return;
  }

  myFile = SD.open(filename, O_WRITE | O_CREAT);

  // if the file opened okay, write to it:
  if (myFile) {
    Serial1.println("Saving data to card. Stop with CTRL C or !STOP");
    Serial1.println(WHITE);
    RECORDING = true;

  } else {
    // if the file didn't open, print an error:
    Serial1.println("Error: Could not create the file");
    Serial1.println(WHITE);
  }
}
