# Network Watchdog
 Network Watchdog is a simple watchdog for monitoring network connectivity and power cycle a device based on ICMP ping replies. 
 
 ## Functionality
- Powered from the controlled Device (usually a wifi repeater or access point)
- Can be used both in local and public networks.
- Ping is working both for IP Addresses and URLs.
- Redundancy functionality using secondary address.
- Manual restart of the controlled Device using the Web Interface or the built-in button.
- Configuration Reset Switch (Keep Restart button pressed for 3 seconds)

## Bill of Materials
1) ESP8266-01
2) AMS1117 3.3V
3) IRLZ44N N-Channel Mosfet
4) 10μF Capacitor (2 pcs)
5) 100nF Capacitor
6) 1k Resistor
7) 10k Resistor
8) 220k Resistor
9) Push Button
10) DC Barrel Power Jack/Connector (Input)
11) DC Barrel Power Jack Plug (Output)
12) An ESP Programmer or another way (i.e using Arduino UNO), to program your ESP-01 Board

 ## Schematic
<p align="center">
  <img src="/schematic/networkWatchdog.jpg">
</p>

## Web Interface
When you finish uploading the code to ESP8266-01 and deploy the circuit, you can configure the device by navigating to http://nm-wd.local or finding the IP of the device using your router's web interface.
<p align="center">
  <img src="/screenshots/ap.png">
</p>

After the initial configuration you have to restart your Watchdog in order to connect to your WiFi network as a client. The Watchdog will deploy a webserver with information and more functionality at the same URL (http://nm-wd.local or the device's IP)
<p align="center">
  <img src="/schematic/control-page.png">
</p>

<a href="https://novamostra.com/2023/01/07/network-watchdog/">Read more at novamostra.com</a>
