/*
 * PieRemote Library -- An ESP8266 IR Remote Library
 * Created by NETPIE.io
 */

#include "config.h"
#include "IRremoteESP8266.h"
#include <stdio.h>
#include <FS.h>
#include <pwm.h>

#define DEBUG

#define MODE_CONTROLLING  1
#define MODE_LEARNING     2

#define NEWSIGNAL         1
#define IRLIST            2
#define CMDRESULT         3

#define CMD_OK            1
#define CMD_FAILED        2

#define MAXCMDLENGT       64
#define SIGNALSTEPSIZE    10

#define DEFAULTFILENAME   "/noname"

class PieRemote {
  public :
    PieRemote();
    void tick();
    void encoding (decode_results *results);  
    bool executeCMD(char*);
    void setCallback(uint8_t, void (*callback)(char*, char*, uint8_t));
  private :
    uint16_t  readIR(void);
    void  printIRCode (decode_results *results);
    void  printInfo (decode_results *results);
    void  printRaw  (decode_results *results);
};

