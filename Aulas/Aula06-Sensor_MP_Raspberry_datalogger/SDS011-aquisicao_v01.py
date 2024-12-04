#!/usr/bin/python
# -*- coding: UTF-8 -*-
import serial, time, struct, array
import datetime
import os
import time
import sys
import os.path
from os import path

data = datetime.datetime.now().strftime("%Y-%m-%d")
hora = datetime.datetime.now().strftime("%Y-%m-%d_%H")
hora_atual = hora


if path.exists("/home/Aulas_2024_IoT/Dados/") != True:
    myCmd = 'mkdir /home/Aulas_2024_IoT/Dados/'
    os.system(myCmd)

if path.exists("/home/Aulas_2024_IoT/Dados/serie_temporal_mp.txt") != True:

    write_to_file_path =  "/home/Aulas_2024_IoT/Dados/serie_temporal_mp.txt";
    output_file = open(write_to_file_path, "a");
    output_file.close()

    # line = 'Resultados do sensor SDS011 NOVA PM'
    line = 'data-hora,pm2.5,pm10'
    print(line)
    output_file = open(write_to_file_path, "a")
    output_file.write(line)
    output_file.write('\n')
    output_file.close()


#########

ser = serial.Serial()
ser.port = "/dev/ttyUSB0" # Set this to your serial port
ser.baudrate = 9600

ser.open()
ser.flushInput()


byte, lastbyte = "\x00", "\x00"
cnt = 0
while True:
    lastbyte = byte
    byte = ser.read(size=1)
#    print("Got byte %x" %ord(byte))
    # We got a valid packet header
    if lastbyte == b'\xaa' and byte == b'\xc0':
        sentence = ser.read(size=8) # Read 8 more bytes
#        print "Sentence size {}".format(len(sentence))
        readings = struct.unpack('<hhxxcc',sentence) # Decode the packet - big endian, 2 shorts for pm2.5 and pm10, 2 reserved bytes, checksum, message tail
#        print array.array('B',sentence)
        pm_25 = readings[0]/10.0
        pm_10 = readings[1]/10.0
        #print(pm_25,pm_10)
        # ignoring the checksum and message tail
        
        if (cnt == 0 ):
            #line = " | PM 2.5, {} ,μg/m^3;  PM 10, {} ,μg/m^3".format(pm_25, pm_10)
            
            
            #print('Teste 1')
            
            pm25  = "{0:.2f}".format(float(pm_25)) #.encode("utf-8")
            pm10  = "{0:.2f}".format(float(pm_10)) #.encode("utf-8")
            #line  = " |  PM2.5 |  {0:.2f}  | μg/m^3".format(pm_25).encode('latin-1', 'ignore')
            #line2 = " |  PM10  |  {0:.2f}  | μg/m^3".format(pm_10).encode('latin-1', 'ignore')
                        
            horario = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            #print(type(horario),type(line))
            print(horario+','+str(pm25)+','+str(pm10))
            #print(horario+str(line2))
            write_to_file_path =  "/home/Aulas_2024_IoT/Dados/serie_temporal_mp.txt";
            output_file = open(write_to_file_path,"a")
            output_file.write(horario+','+str(pm25)+','+str(pm10))
            output_file.write('\n')
            output_file.close()
        cnt += 1
        if (cnt == 5):
            cnt = 0
 
