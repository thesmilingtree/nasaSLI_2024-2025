## This program reads the data from serial port and put it in a xls file
## 1. List all the serial ports
## 2. User choose which port to connet to
## 3. The data is wrapped in 'start' and 'end' signal.
## 4. We look for abopve signal and save all the data in a file and after that exit from the program 

import sys
import serial.tools.list_ports;
import datetime

# List all the serial ports on the computer

ports = serial.tools.list_ports.comports()
found = False
uploadToFile = False
serialInst = serial.Serial()
portList = []
for onePort in ports:
	portList.append(onePort)
	print(str(onePort))
# Asking user to select the port, user just have to enter number part of the port

port = input ("Select port:")
portToReadFrom = "NotFound"

for x in range(0,len(portList)):
	found = str(portList[x]).find(str(port))
	if found:
		portToReadFrom = portList[x]
		exit
	else:
		print("port not found in {0}".format(portList[x]))

## If port not founf, exit from the program

if found < 0 :
	sys.exit("Port Not Found")

# print which port we will read from
print("Final PortToReadFrom = {0}".format(portToReadFrom))

## set the baudrate

## All the port start from following constant string in mac machine
portToReadFrom= "COM"+ str(port)## replace COM with something else for Mac
serialInst.baudrate = 115200 
serialInst.port = str(portToReadFrom)


## Open the serial port for reading the data
serialInst.open()

## Open the file to write
now = datetime.datetime.now()
filename = "rocket-sensor-data-" + now.strftime("%Y-%m-%d-%H:%M") + ".csv" ## or .xls

## Open the file for writing
file = open(filename, "w")
print(filename)

## Run loop to read the file

while True:
	if serialInst.in_waiting:
		line = serialInst.readline()
	
		##Code exmaple shows that we have to decode using utf, in my testing this works for us. 
		line = line.decode('utf').rstrip('\n')	
		print(str(line))

		if line.startswith('start'):
			uploadToFile = True
			print("Start reading data from serial and save to file.")	
		if uploadToFile and line.startswith('end'):
			uploadToFile = False
			file.close()
			sys.exit("No More Data to write.") 
		if uploadToFile:
			file.write(line)
			print(line)
