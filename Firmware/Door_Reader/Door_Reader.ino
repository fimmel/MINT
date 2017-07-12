// Homebrew HID wg_code_full Access System
// Forest Immel
// Credit to all libraries I'm using, code has no guarentees or support
// 
// Revision 1.01
// July 2017
#include <Wire.h>          // For i2c

#define LED_CARD_READ     11    // Dig Out for the Read LED
#define LED_CARD_VALID    10    // Dig Out for the Valid LED
#define LED_CARD_INVALID  9   // Dig Out for the Invalid LED
#define LED_INPUT_1     8   // Dig Out for the Input 1 LED
#define LED_INPUT_2     7   // Dig Out for the Input 2 LED
#define LED_DEBUG       6   // Dig Out for the Debug LED

#define RELAY_STRIKE    A1    // Dig Out for the Strike terminals
#define RELAY_RELAY     A0    // Dig Out for the Relay terminals
#define INPUT_1       4   // Dig In for the Input 1 terminals
#define INPUT_2       5   // Dig In for the Input 2 terminals

#define WIEGAND_D0      2   // weigand 0 bit, has to be pin 2 for the ISRs to work
#define WIEGAND_D1      3   // weigand 1 bit, has to be pin 3 for the ISRs to work

#define MAX_BITS      100   // max number of wiegand bits 
#define WEIGAND_WAIT_TIME   3000  // time to wait for another weigand pulse. 

#define I2C_ADDR      3   // i2c address (has to be unique for each reader)

#define SERIAL_SPEED    115200  // speed for serial debugging
//#define SERIAL_DEBUG    1     // comment / uncoment out for enabling serial debugging


int incomingByte = 0;
 
unsigned char wg_data[MAX_BITS];  // stores all of the data bits
unsigned char wg_bit_count;     // number of bits currently captured
unsigned char wg_flag_done;     // goes low when data is currently being captured
unsigned int wg_counter;      // countdown until we assume there are no more bits
 
unsigned long wg_code_fac=0;    // decoded facility code
unsigned long wg_code_card=0;   // decoded card code
unsigned long int wg_code_full = 0; // decoded full card code
      
bool i2c_new = false;       // Flag for new card being ready and waiting to be checked
bool i2c_debug = false;       // Flag to make debug LED cycle on and off for i2c checks
unsigned long timer_timeout = 5000; // Timer for strike to be active aafter valid scan
unsigned long timer_time = 0;   // Timer value, uses Millis

void setup()
{
  // Setup DIO
  pinMode(LED_CARD_READ, OUTPUT);
  pinMode(LED_CARD_VALID, OUTPUT);
  pinMode(LED_CARD_INVALID, OUTPUT);
  pinMode(LED_INPUT_1, OUTPUT);
  pinMode(LED_INPUT_2, OUTPUT);
  pinMode(LED_DEBUG, OUTPUT);
  pinMode(RELAY_STRIKE, OUTPUT);
  pinMode(RELAY_RELAY, OUTPUT);
  pinMode(INPUT_1, INPUT);
  pinMode(INPUT_2, INPUT);
  pinMode(WIEGAND_D0, INPUT);
  pinMode(WIEGAND_D1, INPUT);
  
  // Begin i2c and bind functions
  Wire.begin(I2C_ADDR);
  Wire.onRequest(i2cRequestEvent);
  Wire.onReceive(i2cReceiveEvent); 

#ifdef SERIAL_DEBUG
  // Begin Serial
  Serial.begin(SERIAL_SPEED);
#endif

  // Bind ISRs for the wiegand data
  attachInterrupt(0, ISR_INT0, FALLING);  
  attachInterrupt(1, ISR_INT1, FALLING);
  
  // Init Wiegand Counter
  wg_counter = WEIGAND_WAIT_TIME;
}
 
void loop()
{
  // Remember, no Delays, use millis() and a loop / condition
  
  // Run timer routine to close door / turn off LEDs
  timer_check();
  
  // Wiegand Loop Funtion
  wg_check();
}

// Wiegand ISRs
void ISR_INT0(){ // interrupt that happens when INTO goes low (0 bit)
  wg_bit_count++;
  wg_flag_done = 0;
  wg_counter = WEIGAND_WAIT_TIME;  
 
}
void ISR_INT1(){ // interrupt that happens when INT1 goes low (1 bit)
  wg_data[wg_bit_count] = 1;
  wg_bit_count++;
  wg_flag_done = 0;
  wg_counter = WEIGAND_WAIT_TIME;  
}

// Timer check to do things after the timeout has been reached.
// Its not a real timer but uses millis(), this is to save on ISRs
// It is plenty accurate for what we need
void timer_check(){
  // Reset LEDs and turn off relays after timout has been reached
  if (timer_time == millis()){
    digitalWrite(LED_CARD_READ, LOW);
    digitalWrite(LED_CARD_VALID, LOW);
    digitalWrite(LED_CARD_INVALID, LOW);
    digitalWrite(LED_INPUT_1, LOW);
    digitalWrite(LED_INPUT_2, LOW);
    digitalWrite(RELAY_STRIKE, LOW);
  }
}

// Wiegand routine to see if enough bits have been collected for a valid card
void wg_check(){
  // This waits to make sure that there have been no more data pulses before processing data
  if (!wg_flag_done) {
    if (--wg_counter == 0)
      wg_flag_done = 1;  
  }
 
  // if we have bits and we the weigand counter went out
  if (wg_bit_count > 0 && wg_flag_done) {
    unsigned char i;
    // we will decode the bits differently depending on how many bits we have
    // see www.pagemac.com/azure/data_formats.php for mor info
    if (wg_bit_count == 26){
      // standard 26 bit format
      // facility code = bits 2 to 9
      for (i=1; i<9; i++){
        wg_code_fac <<=1;
        wg_code_fac |= wg_data[i];
      }

      // wg_code_full code = bits 10 to 23
      for (i=9; i<25; i++){
        wg_code_card <<=1;
        wg_code_card |= wg_data[i];
      }
      // run out processing routine to validate card
      wg_process();  
    }
    else {
    // you can add other formats if you want!
    // Serial.println("Unable to decode."); 
    }

    // cleanup and get ready for the next card to be read
    wg_bit_count = 0;
    wg_code_fac = 0;
    wg_code_card = 0;
    for (i=0; i<MAX_BITS; i++) 
    {
      wg_data[i] = 0;
    }
  }
}

void wg_process(){
  //Turn on LED to indicate a card has been read
  digitalWrite(LED_CARD_READ, HIGH);
  
  //Make the full code of the card
  wg_code_full = (wg_code_fac*100000)+wg_code_card;
  
#ifdef SERIAL_DEBUG
  //Print out the card data
  Serial.print("FC = ");
  Serial.print(wg_code_fac);
  Serial.print(", CC = ");
  Serial.println(wg_code_card); 
  Serial.println(String(wg_code_full));
#endif
  //set the new card read flag for i2c
  i2c_new = true;
}

// Access Granted, turn on Valid LED and energize strike relay
void access_granted(){
  #ifdef SERIAL_DEBUG
  Serial.print("Access Granted");
  #endif
  digitalWrite(LED_CARD_VALID, HIGH);     //Turn on Card Valid LED
  digitalWrite(RELAY_STRIKE, HIGH);     //Activate Strike Relay
  timer_time = timer_timeout + millis();    //Set timer so it all turns off
}
// Acccess Denied, turn on Invalid LED
void access_denied(){
  #ifdef SERIAL_DEBUG
  Serial.print("Access Denied");
  #endif
  digitalWrite(LED_CARD_INVALID, HIGH);   //Turn on Card Inalid LED
  timer_time = (timer_timeout/2) + millis();  //Set timer so it all turns off (timer is half length)
}

// i2c data requested from master, if new flag, send card, if not send x's
void i2cRequestEvent() {
  if (i2c_new){
    char buf[8];
    sprintf(buf, "%lu", wg_code_full);
    Wire.write(buf);
    i2c_new = false;
  }
  //all x's will be interpreted on the master as not a card
  Wire.write("xxxxxxxx");
  
  //Flash the Debug LED alternating each time the i2c is read
  if (i2c_debug == false){
    i2c_debug = true;
  }
  else{
    i2c_debug = false;
  }
  digitalWrite(LED_DEBUG, i2c_debug);
}

// i2c data recieved, should only be a v or d depending on if card was valid or denied
void i2cReceiveEvent(int howMany) {
  char c;
  while (0 < Wire.available()) {  // loop through all but the last
    c = Wire.read();      // receive byte as a character
    #ifdef SERIAL_DEBUG
    Serial.print(c);      // print the character
    #endif
  }
  if (c == 118){          //118 is v in ascii v for valid
    access_granted();
  }
  if (c == 100){          //100 is d in ascii d for denied
    access_denied();
  }
}
