# Openssl + wiFi + GPIO control (for blinking lights based on https connections)

First you should configure the project by "make menuconfig":
  Example Configuration -> 
    1. WIFI SSID: WIFI network to which your PC is also connected to. 
    2. WIFI Password: WIFI password
    3. Output Device GPIO Number: The GPIO number used to control the output device (example: light switch).
    4. Swap Output Polarity: Set this if you want the output device to have low voltage mean ON and high voltage to mean OFF.
    
To test out: 
  1. compile the code and flash it 
  2. input the context of "https://my.esp.ip.goes.here/blink" into your web browser to cause it to blink once
  2. input the context of "https://my.esp.ip.goes.here/switchon" into your web browser to cause it to turn on
  2. input the context of "https://my.esp.ip.goes.here/switchoff" into your web browser to cause it to turn off
  3. You may see that it shows the website is not able to be trusted, but you should select that "go on to visit it"
  
Note:
  The private key and certification at the example are not trusted by web browser, because they are not created by CA official, just by ourselves.
  You can alse create your own private key and ceritification by "openssl at ubuntu or others". 
  We have the document of "ESP8266_SDKSSL_User_Manual_EN_v1.4.pdf" at "https://www.espressif.com/en/support/download/documents". By it you can gernerate the private key and certification with the fomate of ".pem"

See the README.md file in the upper level 'examples' directory for more information about examples.
