
#https://www.influxdata.com/blog/getting-started-python-influxdb/
# $ python3 -m pip install influxdb
import sys

import Adafruit_DHT
import datetime as dateAndTime
import time as Time
import sys
import uuid
#from influxdb_client import InfluxDBClient
from influxdb import InfluxDBClient
import argparse
import os 
import sys

Port = 8088
hostname = '35.154.100.203' 
SENSOR_MODEL = 11 
#Port = 27002 
database = 'defaultdb'
def ConnectToInflux() :

    client = InfluxDBClient(host=hostname,
                        port=Port,
                        username='girish',
                        password='gks3050',
                        ssl=False,
                        verify_ssl=False)

    return client
def main() :
   InflexClient = ConnectToInflux()
   print("Influx connected") 
   print(InflexClient)
   #print(InflexClient.get_list_database())
   InflexClient.switch_database(database)
   while True:
      humidity, temperature = Adafruit_DHT.read_retry(SENSOR_MODEL, 4)
      print(humidity,temperature)
      json_body = [{
                  "measurement": "ClimateEvents",
                  "tags": {
                            "event": "periodic",
                            "SensorId": "00001",
                          },
                  "fields": {
                           "Temperature" : temperature,
                           "Humidity" : humidity
                          }
                }]
      print(json_body)
      #result   = InflexClient.write_points(json_body)
      #print(result)

      Time.sleep(30) #Sleep for 5  minute

if  __name__  == "__main__":
      main()

