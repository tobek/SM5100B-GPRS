/* SM5100B-GPRS
 * this Arduino program sends an HTTP request over GPRS using the SM5100B GSM shield
 * author: http://toby.is
 * more info: http://github.com/tobek/SM5100B-GPRS
*/

// include the SoftwareSerial library to send serial commands to the cellular module:
#include <SoftwareSerial.h>

SoftwareSerial cell(2,3);  // Create a 'fake' serial port. Pin 2 is the Rx pin, pin 3 is the Tx pin. connect this to the GSM module

const String apn = "rcomnet"; // access-point name for GPRS

const String ip = "174.120.246.220"; // IP address of server we're connecting to
const String host = "avantari.co.uk"; // required in HTTP 1.1 - what's the name of the host at this IP address?
const String request = "GET /m/testpage.php?data=testing HTTP/1.1";
const String useragent = "Mozilla/5.0"; // for our purposes the user agent doesn't matter - if I understand correctly it's helpful to use something generic the server will recognize

/* this will send the following packet:
 * 
 * GET /m/testpage.php?data=testing HTTP/1.1
 * Host: avantari.co.uk
 * User-Agent: Mozilla/5.0
 * 
 * this is the equivalent of visiting http://avantari.co.uk/m/testpage.php?data=testing
*/

void setup()
{
  //Initialize serial ports for communication with computer
  Serial.begin(9600);
  
  Serial.println("Starting SM5100B Communication...");
  cell.begin(9600);
  
  waitTil("+SIND: 4"); // keep printing cell output til we get "+SIND: 4"
  Serial.println("Module ready");
}

void loop()
{
  Serial.println("Attaching GPRS...");
  cell.println("AT+CGATT=1");
  waitFor("OK");
  
  Serial.println("Setting up PDP Context...");
  cell.println("AT+CGDCONT=1,\"IP\",\""+apn+"\"");
  waitFor("OK");

  Serial.println("Activating PDP Context...");
  cell.println("AT+CGACT=1,1");
  waitFor("OK");
  
  Serial.println("Configuring TCP connection to TCP Server...");
  cell.println("AT+SDATACONF=1,\"TCP\",\""+ip+"\",80");
  waitFor("OK");
  
  Serial.println("Starting TCP Connection...");
  cell.println("AT+SDATASTART=1,1");
  waitFor("OK");
  
  delay(5000); // wait for the socket to connect
  
  // now we'll loop forever, checking the socket status and only breaking when we connect
  while (1) {
    Serial.println("Checking socket status:");
    cell.println("AT+SDATASTATUS=1"); // we'll get back SOCKSTATUS and then OK
    String sockstat = getMessage();
    waitFor("OK");
    if (sockstat=="+SOCKSTATUS:  1,0,0104,0,0,0") {
      Serial.println("Not connected yet. Waiting 1 second and trying again.");
      delay(1000);
    }
    else if (sockstat=="+SOCKSTATUS:  1,1,0102,0,0,0") {
      Serial.println("Socket connected");
      break;
    }
    else {
      Serial.println("We didn't expect that.");
      cellOutputForever();
    }
  }
  
  // we're now connected and can send HTTP packets!
  
  int packetLength = 26+host.length()+request.length()+useragent.length(); // 26 is size of the non-variable parts of the packet, see SIZE comments below
  
  Serial.println("Sending HTTP packet...");
  cell.print("AT+SDATATSEND=1,"+String(packetLength)+"\r");
  waitFor('>'); // wait for GSM module to tell us it's ready to recieve the packet. NOTE: some have needed to remove this line
  cell.print(request+"\r\n"); // SIZE: 2
  cell.print("Host: "+host+"\r\n"); // SIZE: 8
  cell.print("User-Agent: "+useragent+"\r\n\r\n"); // SIZE: 16
  cell.write(26); // ctrl+z character: send the packet
  waitFor("OK");
  
  
  // now we're going to keep checking the socket status forever
  // break when the server tells us it acknowledged data
  while (1) {
    cell.println("AT+SDATASTATUS=1"); // we'll get back SOCKSTATUS and then OK
    String s = getMessage(); // we want s to contain the SOCKSTATUS message
    if (s == "+STCPD:1") // this means server sent data. cool, but we want to see SOCKSTATUS, so let's get next message
      s = getMessage();
    if (s == "+STCPC:1") // this means socket closed. cool, but we want to see SOCKSTATUS, so let's get next message
      s = getMessage();
    waitFor("OK");
    
    if (!s.startsWith("+SOCKSTATUS")) {
      Serial.println("Wait, this isn't the SOCKSTATUS message!");
      cellOutputForever(); // something went wrong
    }
    if (checkSocketString(s) == packetLength) // checks that packetLength bytes have been acknowledged by server
      break; // we're good!
    else {
      Serial.println("Sent data not yet acknowledged by server, waiting 1 second and checking again.");
      delay(1000);
    }
  }
  Serial.println("Yes! Sent data acknowledged by server!");

  // we could skip the checking of SOCKSTATUS in the above while-loop
  // instead we could just wait for one or both of these:
  //waitTil("+STCPD:1"); // this means data is received
  //waitTil("+STCPC:1"); // this means socket is closed
  
  // TODO: actually check if we received data, don't just do this blindly
  Serial.println("Reading data from server...");
  cell.println("AT+SDATAREAD=1"); // how we read data server has sent
  // WARNING: this might not work - software serial can be too slow for receiving data
    
  //cellHexForever(); // useful for debugging
  cellOutputForever(); // just keep printing whatever GSM module is telling us
}

/* NOTES
 * 
 * what is +STIN:1 ?
 * 
 * to disconnect after transmission: AT+CGACT=0,1 breaks socket. AT+CGATT=0 seems to work more authoritatively?
 * AT+SDATASTART=1,0 // close TCP connection
 * AT+SDATASTATUS=1 // clear sent/ack bytes from SOCKSTATUS
 * 
*/

// ====== HELPER FUNCTIONS ====== //

// keep reading the serial messages we receive from the module
// loop forever until we get a nonzero string ending in \r\n - print and return that.
// TODO: implement a timeout that returns 0?
String getMessage() {
  String s="";
  while(1) {
    if(cell.available()>0) {
      s = s+(char)cell.read();
      if (s.length()>1 && s[s.length()-2]=='\r' && s[s.length()-1]=='\n') { // if last 2 chars are \r\n
        if (s==" \r\n" || s=="\r\n") { // skip these, move on
          s="";
        }
        else { // we have a message!
          Serial.println(s.substring(0,s.length()-2));
          return s.substring(0,s.length()-2);
        }
      }
    }
  }
}

// for eating a single message we expect from the module
// prints out the next message from the module. if it's not the expected value, die
void waitFor(String s) {
  String message=getMessage();
  if (message != s) {
    Serial.println("Wait, that's not what we were expecting. We wanted \""+s+"\"");
    cellOutputForever();
  }
  delay(100); // wait for a tiny bit before sending the next command
}

// keep spitting out messages from the module til we get the one we expect
void waitTil(String s) {
  String message;
  while (1) {
    message = getMessage();
    if (message == s){
      delay(100); // cause we're probably about to send another command
      return;
    }
  }
}

// keep reading characters until we get char c
void waitFor(char c) {
  while(1) {
    if(cell.available()>0) {
      if ((char)cell.read() == c) {
        delay(100);
        return;
      }
    }
  }
}

// if something goes wrong, abort and just display cell module output so we can see error messages
// this will loop forever
void cellOutputForever() {
  Serial.println("Looping forever displaying cell module output...");
  while(1) {
    if(cell.available()>0) {
      Serial.print((char)cell.read());
    }
  }
}

// like above, but in hex, useful for debugging
void cellHexForever() {
  while(1) {
    if(cell.available()>0) {
      char c = (char)cell.read();
//      Serial.print("a char: ");
      Serial.print(c, HEX);
      Serial.print(" ");
      Serial.println(c);
    }
  }
}


// receive string such as "SOCKSTATUS: 1,1,0102,10,10,0"
// 0 is connection id. 1 is whether connected or not. 2 is status (0104 is connecting, 0102 is connected, others)
// 3 is sent bytes. 4 is acknowledged bytes. 5 is "received data counter"
// THIS FUNCTION WILL check that sent bytes == ack bytes, and return that value
// return 0 if they don't match or if amount of data is 0
int checkSocketString(String s) {
  if (socketStringSlice(3,s) == 0)
    return 0;
  else if (socketStringSlice(3,s) == socketStringSlice(4,s))
    return socketStringSlice(3,s);
  else
    return 0;
}

// returns the index of the nth instance of char c in String s
int nthIndexOf(int n, char c, String s) {
  int index=0;
  for (int i=0; i<=n; i++) {
    index = s.indexOf(c,index+1);
  }
  return index;
}

// expects string such as "SOCKSTATUS: 1,1,0102,10,10,0"
// returns nth chunk of data, delimited by commas
int socketStringSlice(int n, String s) {
  String slice = s.substring(nthIndexOf(n-1,',',s)+1,nthIndexOf(n,',',s));
  char cArray[slice.length()+1];
  slice.toCharArray(cArray, sizeof(cArray));
  return atoi(cArray);
}
