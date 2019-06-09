/*
  Modbus RS485 Soil Moisture Sensor with temp
   - read all the holding and register values for ID 1,11-16
   - method to change ID 1 to a new ID
   - Sends stream and status to (formerly)carriots IOT interface
        NOW https://www.altairsmartcore.com
  Circuit:
   - MKR 1000 board
   - MKR 485 shield
        pins A5(RE)and A6(DE), 13 and 14 and voltage
   - Modbus RS485 soil moisture sensor
        by Catnip electronics
        https://www.tindie.com/products/miceuz/modbus-rs485-soil-moisture-sensor-2/ 
   - SparkFun 16x2 SerLCD - RGB on Black 3.3V   
        https://www.sparkfun.com/products/14073
        https://github.com/sparkfun/SparkFun_SerLCD_Arduino_Library
   - 6 position relay board - generic
   - AM2302 (wired DHT22) temperature-humidity sensor
        https://www.adafruit.com/product/393
        
        
*/
//  see link for code from arduino online 'Create'
