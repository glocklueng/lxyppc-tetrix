
/** \file humid_sht.c
 *  \brief SHTxx sensor interface
 *
 *   This reads the values for humidity and temperature from the SHTxx sensor through bit banging.
 *
 */

#include "std.h"
#include "LPC21xx.h"
#include "mcu_periph/uart.h"
#include "messages.h"
#include "subsystems/datalink/downlink.h"
#include "humid_sht.h"

#include "led.h"

#ifndef DOWNLINK_DEVICE
#define DOWNLINK_DEVICE DOWNLINK_AP_DEVICE
#endif

uint16_t humidsht, tempsht;
float fhumidsht, ftempsht;
bool_t humid_sht_available;
uint8_t humid_sht_status;

uint8_t s_write_byte(uint8_t value);
uint8_t s_read_byte(uint8_t ack);
void s_transstart(void);
void s_connectionreset(void);
uint8_t s_read_statusreg(uint8_t *p_value, uint8_t *p_checksum);
uint8_t s_write_statusreg(uint8_t *p_value);
uint8_t s_measure(uint16_t *p_value, uint8_t *p_checksum, uint8_t mode);
uint8_t s_start_measure(uint8_t mode);
uint8_t s_read_measure(uint16_t *p_value, uint8_t *p_checksum);
void calc_sht(uint16_t hum, uint16_t tem, float *fhum ,float *ftem);
uint8_t humid_sht_reset( void );


uint8_t s_write_byte(uint8_t value)
{
  uint8_t i, error=0;

  for (i=0x80;i>0;i/=2)                 //shift bit for masking
  {
    if (i & value) DATA_SET;            //masking value with i , write to SENSI-BUS
     else DATA_CLR;
     SCK_SET;                           //clk for SENSI-BUS
     SCK_SET;SCK_SET;SCK_SET;           //pulswith approx. 5 us
//     _nop_();_nop_();_nop_();           //pulswith approx. 5 us
     SCK_CLR;
  }
  DATA_SET;                             //release DATA-line
  SCK_SET;                              //clk #9 for ack
  error=DATA_IN;                        //check ack (DATA will be pulled down by SHT11)
  SCK_CLR;

  return error;                         //error=1 in case of no acknowledge
}

uint8_t s_read_byte(uint8_t ack)
{
  uint8_t i,val=0;

  DATA_SET;                             //release DATA-line
  for (i=0x80;i>0;i/=2)                 //shift bit for masking
  {
    SCK_SET;                            //clk for SENSI-BUS
    if (DATA_IN) val=(val | i);         //read bit
    SCK_CLR;
  }

  if (ack) DATA_CLR;                    //in case of "ack==1" pull down DATA-Line
  SCK_SET;                              //clk #9 for ack
  SCK_SET;SCK_SET;SCK_SET;              //pulswith approx. 5 us
//  _nop_();_nop_();_nop_();              //pulswith approx. 5 us
  SCK_CLR;
  DATA_SET;                             //release DATA-line
  return val;
}

void s_transstart(void)
{
// generates a transmission start
//       _____         ________
// DATA:      |_______|
//           ___     ___
// SCK : ___|   |___|   |______

  DATA_SET; SCK_CLR;                      //Initial state
SCK_CLR;//  _nop_();
  SCK_SET;
SCK_SET;//  _nop_();
  DATA_CLR;
DATA_CLR;//  _nop_();
  SCK_CLR;
SCK_CLR;SCK_CLR;SCK_CLR;  //  _nop_();_nop_();_nop_();
  SCK_SET;
SCK_SET;//  _nop_();
  DATA_SET;
DATA_SET;//  _nop_();
  SCK_CLR;
}

void s_connectionreset(void)
{
// communication reset: DATA-line=1 and at least 9 SCK cycles followed by transstart
//          _____________________________________________________         ________
// DATA:                                                         |_______|
//             _    _    _    _    _    _    _    _    _        ___     ___
// SCK :    __| |__| |__| |__| |__| |__| |__| |__| |__| |______|   |___|   |______

  uint8_t i;

  DATA_SET; SCK_CLR;                    //Initial state
  for(i=0;i<9;i++)                      //9 SCK cycles
  {
    SCK_SET;
    SCK_CLR;
  }
  s_transstart();                       //transmission start
}

uint8_t s_read_statusreg(uint8_t *p_value, uint8_t *p_checksum)
{
// reads the status register with checksum (8-bit)
   uint8_t error=0;

   s_transstart();                   //transmission start
   error=s_write_byte(STATUS_REG_R); //send command to sensor
   *p_value=s_read_byte(ACK);        //read status register (8-bit)
   *p_checksum=s_read_byte(noACK);   //read checksum (8-bit)
   return error;                     //error=1 in case of no response form the sensor
}

uint8_t s_write_statusreg(uint8_t *p_value)
{
// writes the status register with checksum (8-bit)
   uint8_t error=0;

   s_transstart();                   //transmission start
   error+=s_write_byte(STATUS_REG_W);//send command to sensor
   error+=s_write_byte(*p_value);    //send value of status register
   return error;                     //error>=1 in case of no response form the sensor
}

uint8_t s_measure(uint16_t *p_value, uint8_t *p_checksum, uint8_t mode)
{
// makes a measurement (humidity/temperature) with checksum
   uint8_t error=0;
   uint32_t i;

   s_transstart();                   //transmission start
   switch(mode){                     //send command to sensor
      case TEMP : error+=s_write_byte(MEASURE_TEMP); break;
      case HUMI : error+=s_write_byte(MEASURE_HUMI); break;
      default      : break;
   }
   for (i=0;i<6665535;i++) if(DATA_IN==0) break; //wait until sensor has finished the measurement
   if(DATA_IN) error+=1;                       // or timeout (~2 sec.) is reached
   *(p_value) = s_read_byte(ACK) << 8;         //read the first byte (MSB)
   *(p_value) |= s_read_byte(ACK);             //read the second byte (LSB)
   *p_checksum = s_read_byte(noACK);           //read checksum

   return error;
}

uint8_t s_start_measure(uint8_t mode)
{
// makes a measurement (humidity/temperature) with checksum
   uint8_t error=0;

   s_transstart();                   //transmission start
   switch(mode){                     //send command to sensor
      case TEMP : error+=s_write_byte(MEASURE_TEMP); break;
      case HUMI : error+=s_write_byte(MEASURE_HUMI); break;
      default      : break;
   }

   return error;
}

uint8_t s_read_measure(uint16_t *p_value, uint8_t *p_checksum)
{
// reads a measurement (humidity/temperature) with checksum
   uint8_t error=0;

   if(DATA_IN) error+=1;                       //still busy?
   *(p_value) = s_read_byte(ACK) << 8;         //read the first byte (MSB)
   *(p_value) |= s_read_byte(ACK);             //read the second byte (LSB)
   *p_checksum = s_read_byte(noACK);           //read checksum

   return error;
}

void calc_sht(uint16_t hum, uint16_t tem, float *fhum ,float *ftem)
{
// calculates temperature [ C] and humidity [%RH]
// input : humi [Ticks] (12 bit)
//             temp [Ticks] (14 bit)
// output: humi [%RH]
//             temp [ C]

   const float C1 =-4.0;             // for 12 Bit
   const float C2 = 0.0405;          // for 12 Bit
   const float C3 =-0.0000028;       // for 12 Bit
   const float T1 = 0.01;            // for 14 Bit @ 5V
   const float T2 = 0.00008;         // for 14 Bit @ 5V
   float rh;                         // rh:      Humidity [Ticks] 12 Bit
   float t;                          // t:       Temperature [Ticks] 14 Bit
   float rh_lin;                     // rh_lin:  Humidity linear
   float rh_true;                    // rh_true: Temperature compensated humidity
   float t_C;                        // t_C   :  Temperature [ C]

   rh = (float)hum;                  //converts integer to float
   t = (float)tem;                   //converts integer to float

   t_C=t*0.01 - 39.66;               //calc. Temperature from ticks to [°C] @ 3.5V
   rh_lin=C3*rh*rh + C2*rh + C1;     //calc. Humidity from ticks to [%RH]
   rh_true=(t_C-25)*(T1+T2*rh)+rh_lin;   //calc. Temperature compensated humidity [%RH]
   if(rh_true>100)rh_true=100;       //cut if the value is outside of
   if(rh_true<0.1)rh_true=0.1;       //the physical possible range
   *ftem = t_C;                      //return temperature [ C]
   *fhum = rh_true;                  //return humidity[%RH]
}

uint8_t humid_sht_reset( void )
{
// resets the sensor by a softreset
   uint8_t error=0;

   s_connectionreset();                  //reset communication
   error+=s_write_byte(RESET);           //send RESET-command to sensor

   return error;                         //error=1 in case of no response form the sensor
}

void humid_sht_init( void )
{
  /* Configure DAT/SCL pin as GPIO */

#if (DAT_PIN<16)
    PINSEL0 &= ~(_BV(DAT_PIN*2)|_BV(DAT_PIN*2+1));
#else
    PINSEL1 &= ~(_BV((DAT_PIN-16)*2)|_BV((DAT_PIN-16)*2+1));
#endif

#if (SCK_PIN<16)
    PINSEL0 &= ~(_BV(SCK_PIN*2)|_BV(SCK_PIN*2+1));
#else
    PINSEL1 &= ~(_BV((SCK_PIN-16)*2)|_BV((SCK_PIN-16)*2+1));
#endif

  IO0DIR &= ~(_BV(DAT_PIN));
  IO0CLR = _BV(DAT_PIN);
  IO0DIR = _BV(SCK_PIN);
  IO0CLR = _BV(SCK_PIN);

  humid_sht_available = FALSE;
  humid_sht_status = SHT_IDLE;
}

#if 0
void humid_sht_periodic_orig(void)
{
  uint8_t error=0, checksum;

  s_connectionreset();

  error += s_measure(&humidsht, &checksum, HUMI);  //measure humidity
  error += s_measure(&tempsht,  &checksum, TEMP);  //measure temperature

  if (error != 0)
  {
    s_connectionreset();            //in case of an error: connection reset
  }
  else
  {
    calc_sht(humidsht, tempsht, &fhumidsht, &ftempsht);         //calculate humidity, temperature
    humid_sht_available = TRUE;
  }
}
#endif

void humid_sht_periodic(void) {
  uint8_t error=0, checksum;

  if (humid_sht_status == SHT_IDLE) {
    /* init humidity read */
    s_connectionreset();
    s_start_measure(HUMI);
    humid_sht_status = SHT_MEASURING_HUMID;
  }
  else if (humid_sht_status == SHT_MEASURING_HUMID) {
    /* get data */
    error += s_read_measure(&humidsht, &checksum);

    if (error != 0) {
      s_connectionreset();
      s_start_measure(HUMI);    //restart
      LED_TOGGLE(2);
    }
    else {
      error += s_start_measure(TEMP);
      humid_sht_status = SHT_MEASURING_TEMP;
    }
  }
  else if (humid_sht_status == SHT_MEASURING_TEMP) {
    /* get data */
    error += s_read_measure(&tempsht, &checksum);

    if (error != 0) {
      s_connectionreset();
      s_start_measure(TEMP);    //restart
      LED_TOGGLE(2);
    }
    else {
      calc_sht(humidsht, tempsht, &fhumidsht, &ftempsht);
      humid_sht_available = TRUE;
      s_connectionreset();
      s_start_measure(HUMI);
      humid_sht_status = SHT_MEASURING_HUMID;
      DOWNLINK_SEND_SHT_STATUS(DefaultChannel, DefaultDevice, &humidsht, &tempsht, &fhumidsht, &ftempsht);
      humid_sht_available = FALSE;
    }
  }
}
