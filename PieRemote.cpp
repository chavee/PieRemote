/*
 * PieRemote Library -- An ESP8266 IR Remote Library
 * Created by NETPIE.io
 */

#include "PieRemote.h"

Dir dir;
File f;
uint8_t mode;
uint16_t raw_data[RAWBUF];

IRrecv irrecv(RECV_PIN);
IRsend irsend(SEND_PIN);

void (* cb_newsignal)(char*, char*, uint8_t);
void (* cb_irlist)(char*, char*, uint8_t);
void (* cb_cmdresult)(char*, char*, uint8_t);

PieRemote::PieRemote() {
  irsend.begin();
  mode = MODE_CONTROLLING;
}

void  PieRemote::printIRCode (decode_results *results) {
  if (results->decode_type == PANASONIC) {    // Panasonic has an Address
    Serial.print(results->panasonicAddress, HEX);
    Serial.print(":");
  }
  Serial.print(results->value, HEX);
}

void  PieRemote::printInfo (decode_results *results) {
  Serial.print("Encoding  : ");
  Serial.println(results->decode_type);
  Serial.print("Code    : ");
  printIRCode(results);
  Serial.print(" (");
  Serial.print(results->bits, DEC);
  Serial.println(" bits)");
}

void  PieRemote::printRaw (decode_results *results) {
  char buff[20];
  Serial.print("Timing[");
  Serial.print(results->rawlen-1, DEC);
  Serial.println("]: ");
  for (uint16_t i = 1;  i < results->rawlen;  i++) {
    if (i&1==0) Serial.print("-");
    else Serial.print("+");
    sprintf(buff, "%5d", results->rawbuf[i] * USECPERTICK);
    Serial.print(buff);
    if(i < results->rawlen-1) Serial.print(",");
    if (!(i % 8))  Serial.println("");
  }
  Serial.println();
}

uint16_t PieRemote::readIR(void) {
  uint16_t num = 0;
  uint16_t index = 0;  
  char buff[50];
  decode_results  results;
   
  if (irrecv.decode(&results)) {   // grab IR code
    switch(results.decode_type) {
      default:
      case UNKNOWN:    num =  0;     break;
      case NEC:      num =  1;     break;
      case SONY:     num =  2;     break;
      case RC5:      num =  3;     break;
      case RC6:      num =  4;     break;
      case DISH:     num =  5;     break;
      case SHARP:    num =  6;     break;
      case JVC:      num =  7;     break;
      case SAMSUNG:    num =  8;     break;
      case LG:       num =  9;     break;
      case WHYNTER:    num = 10;     break;
      case PANASONIC:  num = 11;     break;
    }

    #ifdef DEBUG
      printInfo(&results);          // Output the results
      printRaw(&results);           // Output the results in RAW format
      Serial.println(results.value,HEX);
      Serial.println("learned");
    #endif

    irrecv.resume();           // Prepare for the next value

    dir = SPIFFS.openDir("/");
    f = SPIFFS.open(DEFAULTFILENAME, "w");
    if (!f) {
      if (cb_cmdresult) cb_cmdresult("new","File creation failed",CMD_FAILED);
      #ifdef DEBUG
        Serial.println("file creation failed");
      #endif
    }
    else {
      sprintf(buff,"%d:%d:0:%d\r\n",num,(results.rawlen - 1),results.value);
      f.print(buff);
      for (index = 1; index < results.rawlen; index++) {
        f.print((results.rawbuf[index]*USECPERTICK)/SIGNALSTEPSIZE);
        f.print(",");
      }
      f.print("\r\n");
    }
    f.close();
    mode = MODE_CONTROLLING;
    if (cb_newsignal) cb_newsignal(NULL,NULL,CMD_OK);
  }
  else {

  }
}

bool PieRemote::executeCMD(char* cmd) {
  String line;
  
  //** new **************************************
  if(strcmp(cmd,"new")==0) {
    if (mode == MODE_CONTROLLING) {
      #ifdef DEBUG
        Serial.println("Start learning mode..");
      #endif
      mode = MODE_LEARNING;
      irrecv.enableIRIn();  // Start the receiver
      if (cb_cmdresult) cb_cmdresult(cmd,"OK",CMD_OK);
      return true;
    }
    else {
      #ifdef DEBUG
        Serial.println("Cancel learning mode..");
      #endif
      mode = MODE_CONTROLLING;
      irrecv.resume();
      if (cb_cmdresult) cb_cmdresult(cmd,"CANCEL",CMD_FAILED);
      return true;
    }
  }

  //** mv: (move or rename) *********************
  if(strncmp(cmd,"mv:",3)==0) {
    char *p = cmd+3;
    while (*p!=':' && *p!='\n' && *p!='\0') p++;
    if (*p == ':') {
      *p = '\0';
      dir = SPIFFS.openDir("/");
      SPIFFS.rename(cmd+3, p+1);
      if (cb_cmdresult) cb_cmdresult(cmd,"OK",CMD_OK);
      return true;
    }
    else {
      if (cb_cmdresult) cb_cmdresult("cmd","FAILED",CMD_FAILED);
      #ifdef DEBUG
        Serial.println("Failed");
      #endif
      return false;
    }
  }

  //** ls (list all only print to serial) *******
  if(strcmp(cmd,"ls")==0) {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
       Serial.println(dir.fileName());
    }
    return true;
  }

  //** ls: (list specific path) *****************
  if(strncmp(cmd,"ls:",3)==0) {
    char rbuff[256];
    rbuff[0] = '\0';
  
    // if not just "ls:/" e.g. "ls:/tv"
    if (*(cmd+4) != '\0' ) {
      char plen = strlen(cmd+3);
      char fname[17];
      Dir dir = SPIFFS.openDir(cmd+3);
      while (dir.next()) {
         dir.fileName().toCharArray(fname,16);
         if (rbuff[0]!='\0') strcat(rbuff,";");
         strcat(rbuff,fname+plen+1);
      }
      #ifdef DEBUG
        Serial.println(rbuff);
      #endif
      if (cb_irlist) cb_irlist(cmd,rbuff,CMD_OK);
    }
    else {
      bool found = false;
      char *rname;
      char rlen = 0;
      char fname[17];

      Dir dir = SPIFFS.openDir(cmd+3);
      while (dir.next()) {
        dir.fileName().toCharArray(fname,16);
        if (strcmp(fname,"/noname")!=0) {
          char *r = fname+1;
          while (*r!='/' && *r!='\0') r++;
          *r = '\0';
          rname = fname+1;
          rlen = strlen(rname);
      
          char *p;
          found = false;
          for (p=rbuff; *p!='\0'; p++) {
            if (strncmp(rname,p,rlen)==0) {
              found = true;
              break;
            }
            else {
              while (*p!='\0' && *p!=';') p++;
            }
          }
          if (!found) {
            if (rbuff[0]!='\0') strcat(rbuff,";");
            strcat(rbuff,rname);
          }
        }
      }
      #ifdef DEBUG
        Serial.println(rbuff);    
      #endif
      if (cb_irlist) cb_irlist(cmd,rbuff,CMD_OK);
    }
  }
  
  //** rm: **************************************
  if(strncmp(cmd,"rm:",3)==0) {
    SPIFFS.remove(cmd+3);
    if (cb_irlist) cb_irlist(cmd,"OK",CMD_OK);
  }

  //** cat: (send itr signal) *******************
  if(strncmp(cmd,"cat:",4) == 0) {
    char d;
    f = SPIFFS.open(cmd+4 , "r");
    while(f.available()) {
      f.readBytes(&d,1);
      Serial.print(d);
    }
    Serial.println();
  }

  //** ir: (send itr signal) ********************
  if(strncmp(cmd,"ir:",3) == 0) {
    char dbuff[32];
    char d, *p;
    unsigned int sct, hct;
    unsigned long header[4];

    dbuff[0] = '\0';
    f = SPIFFS.open(cmd+3 , "r");

    if (!f.available()) {
      #ifdef DEBUG
        Serial.println("not found");
      #endif
       return false;
    }
    
    hct = 0;
    sct = 0;
    do {
      f.readBytes(&d,1);
      if (d != ':' && d != '\n') {
        dbuff[sct++] = d;
      }
      else {
        dbuff[sct] = '\0';
        sct = 0;
        header[hct++] = atoi(dbuff);
      }        
    } while (d != '\n');
    
    unsigned long irtype = header[0];
    unsigned long dsize = header[1];
    unsigned long address = header[2];
    unsigned long ircode = header[3];
    unsigned int rawsignal[RAWBUF] = {0};

    hct = 0;
    sct = 0;
    do {
      f.readBytes(&d,1);
      if (d != ',') {
        dbuff[sct++] = d;
      }
      else {
        dbuff[sct] = '\0';
        sct = 0;
        rawsignal[hct++] = SIGNALSTEPSIZE * atoi(dbuff);
      }
    } while (d!='\n' && f.available());

    //make the code size even, not sure if this is necessary?
    if (dsize & 1 && dsize+1 < RAWBUF) dsize=dsize+1;
    irsend.sendRaw(rawsignal, dsize, 38);

    #ifdef DEBUG
      for (uint16_t i=0; i<dsize-1; i++) {
        Serial.print(rawsignal[i]);
        Serial.print(",");
      }
    #endif
  }
}

void PieRemote::tick() {
  if (mode == MODE_LEARNING) {
    readIR();
  }
}

void PieRemote::setCallback(uint8_t cbtype, void (* callback)(char*, char*, uint8_t)  ) {
  switch (cbtype) {
    case NEWSIGNAL :  cb_newsignal = callback;
              break;
    case IRLIST  :  cb_irlist = callback;
              break;  
    case CMDRESULT :  cb_cmdresult = callback;
              break;  
  }
}

