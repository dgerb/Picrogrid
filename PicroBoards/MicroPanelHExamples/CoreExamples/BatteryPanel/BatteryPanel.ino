/*
  Battery Panel

  WARNING: Never use unprotected batteries... ever. You will burn down your home.

  Enhances the Smart Panel example with battery discharge control. Assumes a nanogrid with battery is connected to
  the high power port and loads connected to low power ports. Ensures battery does not get over discharged with a
  low-voltage cutoff.

  This example is intended to power a DC nanogrid. The channels are assigned as:
  Ch 1: lights
  Ch 2: room 1 fans and electronics
  Ch 3: room 2 fans and electronics
  Ch 4: refrigerator

  The loads on channels 2 and 3 might draw initial inrush current greater than the 5.5A hardware current limit.
  We use the inrush override feature to disable the hardware current shutoff for 1000 microseconds.

  Channel 4 may draw >5.5A of inrush current for several seconds. Here, we disable the hardware current shutoff altogether.
  Be careful when disabling the hardware current shutoff; make sure you understand clearly that the inrush current will not
  damage the connectors or board traces, rated for 5A. I recommend you put a fuse or breaker in series with this channel,
  set to the absolute max inrush current you would expect.

  We can still protect Channel 4 through software. The automatic software shutoff in the library will be too fast for our
  needs, thus we create our own code and timer in controlUpdate for channel 4.

  The BMS calculation does not bother accounting for cell temperature. The temperature can increase by 10°C if the battery is
  is subject to <1C discharge for a long period. In general, lithium battery pack internal resistance increases by
  0.5-1% per °C, so we expect the internal resistance to vary at most by 5-10%. In this application (0.5C), that translates to a
  battery voltage variance of 0.2V out of 48V, which has an overall minimal effect on the SOC calculation's accuracy.

  created 20 August 2024
  updated 11 November 2025
  by Daniel Gerber
*/

#include <MicroPanelH.h>

MicroPanelH micropanel;
long slowInterruptCounter = 0;

// specify the follwoing absolute max battery values from battery datasheet
const int SOCMIN = 5; // Absolute minimum SOC after which all channels get turned off
const unsigned int VBATMINABS = 10500*4; // absolute min battery voltage in mV regardless of current
const int IBATDISMAX = 15000; // max battery discharging current in mA
const unsigned int RINTERNAL = 110; // estimated internal resistance plus cable to Micropanel (mohms)

// battery curve lookup table
const int LUTN = 9;
const unsigned int BATTV[LUTN] = {43439, 46006, 48225, 49663, 50820, 52756, 52814, 53174, 53953};
// const unsigned int BATTV[LUTN] = {21720, 23003, 24112, 24832, 25410, 26378, 26407, 26587, 26977};
const int BATTSOC[LUTN] = {0, 1, 3, 5, 8, 51, 86, 95, 100};

// Battery Converter global variables
int iBatExtIn = 0; // raw battery input current (-512 to 512), which must be measured externally or ignored
int iBatExtOut = 0; // raw battery input current (-512 to 512), which must be measured externally or ignored
int soc; // global variable to track the SOC, calculated by battery voltage
unsigned int vBatMinAbs = 0; // absolute min battery voltage any current
// unsigned int vActivate = 0; // if undervoltage cutoff, needs this voltage to reactivate
unsigned int battVArr[LUTN];

// // global variables for coulomb counting (relevant for SOC calculations)
// long coulombCounter = 0; // coulomb counter (mA-sec)
// long ccntAccumulator = 0; // sub-second column counter accumulator for averaging (raw 0-1023)

// cumulative moving average for channel 4 software shutoff
const int ICH4LIMIT = 5000; // channel 4 moving average current limit (mA)
const int CH4AVGWINDOW = 8; // moving average window size
int iCh4Limit = 0;
int iCh4Acc[CH4AVGWINDOW] = {0}; // accumulator for moving average

// the setup function runs once when you press reset or power the board
void setup() {
  micropanel.setupPinMode(); // set pins to input or output
  micropanel.initializeSensors(); // set filtered sensor values to initial reading
  micropanel.setCurrentLimit1(6000); // set software current shutdown at 6A peak current 
  micropanel.setCurrentLimit2(6000); // set software current shutdown at 6A peak current 
  micropanel.setCurrentLimit3(6000); // set software current shutdown at 6A peak current 
  micropanel.setCurrentLimit4(10000); // effectively remove channel 4 software current shutdown
  micropanel.setCurrentLimitTotal(IBATDISMAX); // set gate shutdown at max battery current 

  // set up UART and I2C command support
  // add the command interpretation function below as a listener for serial commands
  micropanel.addCommandCallback(&interpretRXCommand);
  // initialize UART communication at 9600 bits per second
  micropanel.startUART(); // can optionally specify baud rate with an argument
  // initialize I2C communciation and register receive and request callback functions
  // requires using lambda functions; it's clunky but mandatory since Wire.h doesn't allow member callbacks
  // just copy the following code for any I2C projects
  ReceiveEventI2C receiveEvent = [] (int howMany) {micropanel.receiveEventI2C(howMany);};
  RequestEventI2C requestEvent = [] () {micropanel.requestEventI2C();};
  micropanel.startI2C(8, receiveEvent, requestEvent); // first argument is the slave device address (max 127)

  // set battery raw parameters (raw 0-1023)
  vBatMinAbs = micropanel.mV2raw(VBATMINABS);
  // vActivate = micropanel.mV2raw(VACTIVATE);
  micropanel.setRDroop(RINTERNAL);
  for (int n = 0; n < LUTN; n++)
    battVArr[n] = micropanel.mV2raw(BATTV[n]);

  // initialize inrush override for channels
  micropanel.setDefaultInrushOverride(200); // hold default channel protection for 200us to ride through inrush current
  micropanel.setDefaultInrushOverride(2, 1000); // channel 2 (fans and electronics) needs 1000us inrush ride through
  micropanel.setDefaultInrushOverride(3, 1000); // channel 3 (fans and electronics) needs 1000us inrush ride through
  micropanel.disableHardwareShutoff(4); // Expert Only: for channel 4, we disable the hardware current shutoff
  
  // set channel 4 current cumulative average limit
  iCh4Limit = micropanel.mA2raw(ICH4LIMIT);

  // initialize interrupt timer for periodic calls to control update funciton
  micropanel.initializeInterruptTimer(1000, &controlUpdate); // control update every 1ms (= 1000 microseconds)
}

void loop() {
  micropanel.updateVISensors(); // read voltage and current sensors and update moving average
}

// main controller update function, which runs on every timer interrupt
void controlUpdate(void)
{
  micropanel.readUART(); // if using UART, check every cycle if there are new characters in the UART buffer
  micropanel.checkCurrentShutdown(); // checks average current and shut down gates if necessary

  // analog read battery voltage
  int vBat = micropanel.getRawVBus(); // battery port voltage, aka. bus voltage
  /* analog read battery current
    - the battery output current can be measured by the MicroPanel as the sum of channel currents.
    - the battery input current must be measured externally, possibly from a PiSupply,
        uploaded to a Pi, and downloaded to the MicroPanel.
    - we can generally ignore battery input current for simplicity
      - this will reduce accuracy of SOC and/or coulumb counting calculations, if that matters to you
      - for BMS, input current causes vBat to be higher, iBat to be lower, and the vBat minimum threshold to be lower
        - this means if discharging, any charging will cause BMS to react at a lower SOC
        - to be safe, you can set VBATTMINABS = VBATTMIN - RINTERNAL * IBATDISMAX, it's conservative, but you can safely
          ignore battery input current without ever having to worry about overdischarge
  */
  int iBatOut = micropanel.getRawITotal(); // current out of battery (positive)
  int iBat = iBatOut + iBatExtOut - iBatExtIn; // iBat represents the raw net current out of the battery
  
  // BMS code: turn off loads if battery SOC is too low
  int vBat0A = vBat + micropanel.getVDroopRaw(iBat); // adjusted battery voltage, accounting for voltage droop due to iBat
  soc = interpolate(battVArr, BATTSOC, LUTN, vBat0A);
  // if (vBat < vBatMinAbs || soc < SOCMIN) { // battery voltage or SOC goes below absolute min threshold
  //   if (micropanel.isSomeChannelsActive())
  //     micropanel.shutdownChannels();
  // }

  slowInterruptCounter++; // in this example, do some special stuff every 1 second (1000ms)
  if (slowInterruptCounter > 1000) {
    slowInterruptCounter = 0;
    micropanel.updateVCC(); // read on-board VCC voltage, update stored average (shouldn't change)

    // check ch4 current moving average is less than the channel 4 limit
    int iCh4 = micropanel.getRawI4();
    // update moving average accumulator
    long iCh4Average = 0;
    for (int n = 1; n < CH4AVGWINDOW; n++) {
      iCh4Acc[n-1] = iCh4Acc[n];
      iCh4Average = iCh4Average + iCh4Acc[n];
    }
    iCh4Acc[CH4AVGWINDOW - 1] = iCh4;
    iCh4Average = iCh4Average + iCh4Acc[0];
    iCh4Average = iCh4Average/CH4AVGWINDOW;
    // check if moving average is less than limit
    if (iCh4Average > iCh4Limit) {
      micropanel.setCh4(LOW);
    }

    // prints channel state (as binary), VCC, VBus, I1, I2, I3, I4 to the serial console of attached computer
    Serial.print("State: ");
    Serial.print(micropanel.getCh1());
    Serial.print(micropanel.getCh2());
    Serial.print(micropanel.getCh3());
    Serial.print(micropanel.getCh4());
    Serial.print(", VCC=");
    Serial.print(micropanel.getVCC());
    Serial.print("mV, VBus=");
    // Serial.print(vBat0A));
    Serial.print(micropanel.getVBus());
    Serial.print("mV, soc=");
    Serial.print(soc);
    Serial.print("%, I1=");  
    Serial.print(micropanel.getI1());
    Serial.print("mA, I2=");  
    Serial.print(micropanel.getI2());
    Serial.print("mA, I3=");  
    Serial.print(micropanel.getI3());
    Serial.print("mA, I4=");  
    Serial.print(micropanel.getI4());
    Serial.print("mA, ITot=");
    Serial.print(micropanel.getITotal());
    Serial.print("mA, vbat:");
    Serial.print(vBat);
    Serial.print(", vdrp:");
    Serial.println(micropanel.getVDroopRaw(iBat));
  }
}

// interpolation for finding battery SOC based on voltage using lookup table
int interpolate(int x[], int y[], int n, int x_query)
{
    // Handle out-of-range queries by clamping
    if (x_query <= x[0]) return y[0];
    if (x_query >= x[n - 1]) return y[n - 1];
    // Find the interval [x[i], x[i+1]] containing x_query
    int i = 0;
    while (i < n - 1 && x_query > x[i + 1])
      ++i;
    int x0 = x[i], x1 = x[i + 1];
    int y0 = y[i], y1 = y[i + 1];
    // Protect against zero-division if x values are identical
    if (x1 == x0)
      return y0;
    // Linear interpolation formula
    return y0 + (y1 - y0) * (x_query - x0) / (x1 - x0);
}

void interpretRXCommand(char* command, char* value, int receiveProtocol) {
  if (strcmp(command, "RSOC") == 0) { // read SOC estimate (0-100)%
    sprintf(micropanel.getTXBuffer(receiveProtocol), "WSOC:%d", soc);
    micropanel.respondToMaster(receiveProtocol);
  } else if (strcmp(command, "WIEI") == 0) { // record information from the Pi on battery external input current
    writeBattInputCurrent(value, receiveProtocol);
  } else if (strcmp(command, "WIEO") == 0) { // record information from the Pi on battery external output current
    writeBattOutputCurrent(value, receiveProtocol);
  } else if (strcmp(command, "RFN") == 0) { // report file name
    readFileName(value, receiveProtocol);
  } else if (strcmp(command, "RCHA") == 0) { // report "1" if any channel is active, "0" if none active
    readIsChannelActive(value, receiveProtocol);
  } else {
    // Write Channel Protected: checks if battery voltage is above activate threshold before enabling channel
    int temp = atoi(value);
    // int vBat = micropanel.getRawVBus(); // battery port voltage, aka. bus voltage
    if (strcmp(command, "WCP1") == 0) { // write the desired terminal 1 state
      if (temp == 1 && micropanel.getCh1() == 0 && soc < SOCMIN)
        temp = 0;
      micropanel.setCh1(temp);
      sprintf(micropanel.getTXBuffer(receiveProtocol), "WCP1:=%d", micropanel.getCh1());
    } else if (strcmp(command, "WCP2") == 0) { // write the desired terminal 2 state
      if (temp == 1 && micropanel.getCh2() == 0 && soc < SOCMIN)
        temp = 0;
      micropanel.setCh2(temp);
      sprintf(micropanel.getTXBuffer(receiveProtocol), "WCP2:=%d", micropanel.getCh2());
    } else if (strcmp(command, "WCP3") == 0) { // write the desired terminal 3 state
      if (temp == 1 && micropanel.getCh3() == 0 && soc < SOCMIN)
        temp = 0;
      micropanel.setCh3(temp);
      sprintf(micropanel.getTXBuffer(receiveProtocol), "WCP3:=%d", micropanel.getCh3());
    } else if (strcmp(command, "WCP4") == 0) { // write the desired terminal 4 state
      if (temp == 1 && micropanel.getCh4() == 0 && soc < SOCMIN)
        temp = 0;
      micropanel.setCh4(temp);
      sprintf(micropanel.getTXBuffer(receiveProtocol), "WCP4:=%d", micropanel.getCh4());
    } else if (strcmp(command, "WCPA") == 0) { // write the desired state for all terminals
      if (temp == 1 && soc < SOCMIN && (micropanel.getCh1() == 0 || micropanel.getCh2() == 0 || micropanel.getCh3() == 0 || micropanel.getCh4() == 0))
        temp = 0;
      micropanel.setCh1(temp);
      micropanel.setCh2(temp);
      micropanel.setCh3(temp);
      micropanel.setCh4(temp);
      sprintf(micropanel.getTXBuffer(receiveProtocol), "WCPA:=%d", micropanel.getCh1());
    }
    micropanel.respondToMaster(receiveProtocol);
  }
}

// outputs the file name to serial
void readFileName(const char* valueStr, int receiveProtocol) {
  sprintf(micropanel.getTXBuffer(receiveProtocol), "WFN:%s", "BatteryPanel.ino");
  micropanel.respondToMaster(receiveProtocol);
}

// records the battery input current information from the Pi
void writeBattInputCurrent(const char* valueStr, int receiveProtocol) {
  int temp = atoi(valueStr);
  iBatExtIn = micropanel.mA2raw(temp);
}

// records the battery input current information from the Pi
void writeBattOutputCurrent(const char* valueStr, int receiveProtocol) {
  int temp = atoi(valueStr);
  iBatExtOut = micropanel.mA2raw(temp);
}

// report "1" if any channel is active, "0" if none active
void readIsChannelActive(const char* valueStr, int receiveProtocol) {
  int isChannelActive = micropanel.getCh1() || micropanel.getCh2() || micropanel.getCh3() || micropanel.getCh4();
  sprintf(micropanel.getTXBuffer(receiveProtocol), "WCHA:%d", isChannelActive);
  micropanel.respondToMaster(receiveProtocol);
}

  // --- Default Register Guide from Library ---
  // "RVB": read bus voltage
  // "RI1": read current at terminal 1
  // "RI2": read current at terminal 2
  // "RI3": read current at terminal 3
  // "RI4": read current at terminal 4
  // "RVCC": read the ~5V VCC bus voltage
  // "RCH1": read current at terminal 1
  // "RCH2": read current at terminal 2
  // "RCH3": read current at terminal 3
  // "RCH4": read current at terminal 4
  // "WCH1": write the desired terminal 1 state
  // "WCH2": write the desired terminal 2 state
  // "WCH3": write the desired terminal 3 state
  // "WCH4": write the desired terminal 4 state
  // "WIL1": write the terminal 1 current shutdown limit (mA)
  // "WIL2": write the terminal 2 current shutdown limit (mA)
  // "WIL3": write the terminal 3 current shutdown limit (mA)
  // "WIL4": write the terminal 4 current shutdown limit (mA)

