The Internet is littered with failed wrecks and dead-end threads resulting from problems encountered setting up GPRS using the SM5100B GSM shield for Arduino. I have this working for HTTP POST and GET requests, and here it is. It's not optimized for actual use, but is laid out very wordily so as to help one understand the process.

This code sends the following commands to the module (bracketed values you need to set):

    AT+CGATT=1\r\n                      (attach GPRS)
    AT+CGDCONT=1,"IP","[apn]"\r\n       (set up PDP context using apn - access point name)
    AT+CGACT=1,1\r\n                    (activate PDP context)
    AT+SDATACONF=1,"TCP","[ip]",80\r\n  (configure TCP connection to server - specify IP)
    AT+SDATASTART=1,1\r\n               (start TCP connection)
    AT+SDATASTATUS=1\r\n                (check socket status - code reads values to make sure we're connected)
    AT+SDATATSEND=1,[packetlength]\r    (start sending the packet, give it the packet size. wait for '>' character from module)
    [packet]                            (send the packet, followed by ctrl+z, i.e. 0x1A)
    AT+SDATAREAD=1\r\n                  (if module gives us "+STCPD:1" it means we've received data - we can read it with this command)
    
A big problem a lot of people were having was with AT+SSTRSEND. That command terminates on line breaks, but the packet needs line breaks. Instead, we use AT+SDATATSEND, which terminates with the ctrl+z character (0x1A, or 26). The only annoyance with AT+SDATATSEND is that we need to tell the module the length of the packet we're sending. Also, unlike all the other commands which end in \r\n, for whatever reason it seems we have to end AT+SDATATSEND in only \r.

The packet this program sends is as follows:

    GET /m/testpage.php?data=testing HTTP/1.1
    Host: avantari.co.uk
    User-Agent: Mozilla/5.0
    
Which is equivalent to visiting http://avantari.co.uk/m/testpage.php?data=testing.

Good luck!

PS: You might not actually be able to read data received from the server from the module if you're using software serial, because it can be too slow. But it usually worked for me.

***

$ whoami

-   Toby
-   tobyfox@gmail.com
-   http://github.com/tobek
-   http://toby.is
