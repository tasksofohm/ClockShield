
//Pin connected to SER (Pin 14) of 74HC595
#define DATA 2
//Pin connected to SCK (Pin 11) of 74HC595
#define CLOCK 3
//Pin connected to RCK (Pin 12) of 74HC595
#define LATCH 4
//Pin connected to the colon pin of the display
#define COLON 5
//Pin connected to HOUR button
#define HOUR_BUTTON 6
//Pin connected to MIN button
#define MIN_BUTTON 7

// With prescalar of 1024, TCNT1 increments 15,625 times per second
// 65535 - 15625 = 49910 ==> 1 s interval
// 65535 - 7812 = 57723 ==> 0,5 s interval
// The intervals below have been determined by testing on my boards. Drifts caused by tolerances
// of parts causes variances in cycles. These variances are thus compensated.
//#define TIMER1_SECOND_START 49911
#define TIMER1_SECOND_START 57718

volatile byte Hours = 15;
volatile byte Minutes = 40;
volatile byte Seconds = 0;
volatile byte aDigits[4] = {0, 0, 0, 0};
volatile boolean aDigitState[4] = {HIGH, HIGH, HIGH, HIGH};
volatile byte currentDigit = 0;
volatile boolean ticked = false;
volatile boolean HoursUpdated = true;
volatile boolean MinutesUpdated = true;
volatile boolean BlinkMinutes = false;
volatile boolean BlinkHours = false;

void setup() {
  //set pins to output because they are addressed in the main loop
  pinMode(LATCH, OUTPUT);
  pinMode(CLOCK, OUTPUT);
  pinMode(DATA, OUTPUT);
  pinMode(COLON, OUTPUT);
  digitalWrite(COLON, LOW);

  cli();		// disable global interrupts

  // initialize Timer1
  TCCR1A = 0;		// set entire TCCR1A register to 0
  TCCR1B = 0;		// same for TCCR1B
  
  // Set CS10 and CS12 bits for 1024 prescaler:
  TCCR1B |= (1 << CS12) | (1 << CS10);
  // enable timer overflow interrupt:
  TIMSK1 &= ~(1 << TOIE1); // clear TOIE1
  TIMSK1 |= (1 << TOIE1); // set TOIE1
  // With prescalar of 1024, TCNT1 increments 15,625 times per second
  // 65535 - 15625 = 49910 ==> 1 s interval
  // 65535 - 7812 = 57723 ==> 0,5 s interval
  TCNT1 = TIMER1_SECOND_START;

  // Disable the timer2 overflow interrupt
  TIMSK2 &= ~(1 << TOIE2);

  // Set timer2 to normal mode
  TCCR2A &= ~((1 << WGM21) | (1 << WGM20));
  TCCR2B &= ~(1 << WGM22);

  // Use internal I/O clock
  ASSR &= ~(1 << AS2);

  // Disable compare match interrupt
  TIMSK2 &= ~(1 << OCIE2A);

  // Prescalar is clock divided by 128
  TCCR2B |= (1 << CS22);
  TCCR2B &= ~(1 << CS21);
  TCCR2B |= (1 << CS20);

  // Start the counting at 0
  TCNT2 = 0;

  // Enable the timer2 overflow interrupt
  TIMSK2 |= (1 << TOIE2);  

  sei();			// enable global interrupts

}

void loop() {

  while (true) {
    
    // calculate the new digits once the hours or minutes are updated
    if (MinutesUpdated) {
      aDigits[0] = Minutes % 10;
      aDigits[1] = Minutes / 10;
      MinutesUpdated = false;
    }
    if (HoursUpdated) {
      aDigits[2] = Hours % 10;
      aDigits[3] = Hours / 10;
      HoursUpdated = false;
    }

    // check if any of the buttons is pressed
    
  }
}

// Timer 1 interrupt.
// This part is run every 0,5 sec. I.e. we update the seconds only every second run.
// But on every run we toggle the colon which results in a blinking one (0,5 sec off and 0,5 sec on).
ISR(TIMER1_OVF_vect) {
  TCNT1 = TIMER1_SECOND_START;

  if (ticked) {
    ticked = ~(ticked);
    digitalWrite(COLON, LOW);
    Seconds++;
    if (Seconds == 60) {
      Seconds = 0;
      Minutes++;
      MinutesUpdated = true;
      if (Minutes == 60) {
        Minutes = 0;
        Hours++;
        HoursUpdated = true;
        if (Hours == 24) {
          Hours = 0;
        }
      }
    }
    // shall we blink minutes?
    if (BlinkMinutes) {
      aDigitState[0] = HIGH;
      aDigitState[1] = HIGH;
    }
    // shall we blink hours?
    if (BlinkHours) {
      aDigitState[2] = HIGH;
      aDigitState[3] = HIGH;
    }
  } else {
    ticked = ~(ticked);
    digitalWrite(COLON, HIGH);
    // shall we blink minutes?
    if (BlinkMinutes) {
      aDigitState[0] = LOW;
      aDigitState[1] = LOW;
    }
    // shall we blink hours?
    if (BlinkHours) {
      aDigitState[2] = LOW;
      aDigitState[3] = LOW;
    }
  }
  
}

// This is the display interrupt to implement multiplexing of the digits.
ISR(TIMER2_OVF_vect) {
  byte data = 0;

  TCNT2 = 0;

  if (++currentDigit > 3) {
    currentDigit = 0;
  }

  // Upper 4 bits of data are the value for the current digit.
  // They are loaded into shift register outputs QA-QD
  data = (aDigits[currentDigit] << 4);

  // Lower 4 bits 3-0 represent which digit to turn on.
  // 3 is most significant digit, 0 is least
  // They are loaded into shift register outputs QE-QH
  // Digit transistors are active low, so set them all high
  data |= 0x0F;

  // now turn off the bit for digit we want illuminated.
  if (aDigitState[currentDigit] == HIGH) {
    // digit should be illuminated --> bit off
    data &= ~(1 << currentDigit);
  }

  digitalWrite(LATCH, LOW);
  shiftOut(DATA, CLOCK, LSBFIRST, data);
  digitalWrite(LATCH, HIGH);
}
