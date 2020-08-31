import serial
import json 
import os

port="/dev/ttyUSB4"
baud=9600
callbackCommand="/coding/mailbox/onMessage.sh"

# init
ser=serial.Serial(port, baud)
ser.baudrate=baud

# functions
def callback(r, l, w, b):
   args = r + " " + l + " " + w + " " + b
   cmd = callbackCommand + " " + args
   #print(cmd)
   os.system(cmd)   

def onMessage(data):
   d = data.strip()
   if "Received LoRa packet" in d and "rssi" in d and "w:" in d and "b:" in d:
      darr = d.split(";")
      if len(darr) == 4:
         title = darr[0]
         rssi = darr[1].replace("rssi=","")
	 #print(rssi)
         length = darr[2].replace("length=","")
  	 #print(length)
         data = darr[3].strip()
	 data = data.replace("data=","")
         data = data.replace('w:','"w":')
         data = data.replace('b:','"b":')
         o = json.loads(data)
         weigth = str(o["w"])
         battery = str(o["b"])    
         #print(weigth)
         #print(battery)
         callback(rssi, length, weigth, battery)
      else:
         "Invalid message size"	
   else:
      print "Invalid message data"

# main
try:
   while True:	
      read_serial_data=ser.readline()
      #print(read_serial_data)
      # test data
      #read_serial_data="Received LoRa packet;rssi=-43;length=17;data={w:267872,b:3746}"
      # on message
      onMessage(read_serial_data)
except:
   print("Program terminated!")
