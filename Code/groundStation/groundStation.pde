import com.jmatio.types.*;
import com.jmatio.io.*;

// GUI Toolkit
import controlP5.*;

// Serial port interface
import processing.serial.*;

import java.util.Calendar;

// Internal program state
boolean recording = false;
boolean playing = false;
int playbackIndex = 0;
int lastPlaybackTime = 0;
MatFileReader inputMatFileReader;
Long recordedMessages = new Long(0);
Serial myPort;
ControlTimer recordTimer;
ControlP5 controlP5;
DropdownList serialPortsList;
Textlabel recordTimerLabel;
Textlabel recordedMessagesLabel;
Toggle recordToggle;
Button selectMatFile;


// Reference
PVector trueNorth = new PVector(0, 1, 0);

// Current timestep boat state data
PVector L2 = new PVector(0,0,0);
PVector globalPosition = new PVector(0,0,0);
float heading = 0.0;
PVector localPosition = new PVector(0,0,0);
PVector velocity = new PVector(0,0,0);
PVector waypoint0 = new PVector(0,0,0);
PVector waypoint1 = new PVector(0,0,0);
int rudderPot = 0;
boolean rudderPortLimit = false;
boolean rudderSbLimit = false;
float gpsLatitude = 0.0;
float gpsLongitude = 0.0;
float gpsAltitude = 0.0;
byte gpsYear = 0;
byte gpsMonth = 0;
byte gpsDay = 0;
byte gpsHour = 0;
byte gpsMinute = 0;
byte gpsSecond = 0;
float gpsCourse = 0.0;
float gpsSpeed = 0.0;
float gpsHdop = 0.0;
byte gpsFix = 0;
byte gpsSatellites = 0;
byte reset = 0;
byte load = 0;
float rudderAngle = 0.0;
int propRpm = 0;
byte statusBits = 0;
byte ordering = 0;
float rudderAngleCommand = 0.0;
int throttleCommand = 0;
float batteryVoltage = 0.0;
float batteryAmperage = 0.0;

// Boat state data recording
ArrayList<float[]> L2List = new ArrayList<float[]>(255);
ArrayList<float[]> globalPositionList = new ArrayList<float[]>(255);
ArrayList<Float> headingList = new ArrayList<Float>(255);
ArrayList<float[]> localPositionList = new ArrayList<float[]>(255);
ArrayList<float[]> velocityList = new ArrayList<float[]>(255);
ArrayList<float[]> waypoint0List = new ArrayList<float[]>(255);
ArrayList<float[]> waypoint1List = new ArrayList<float[]>(255);
ArrayList<Integer> rudderPotList = new ArrayList<Integer>(255);
ArrayList<Byte> rudderPortLimitList = new ArrayList<Byte>(255);
ArrayList<Byte> rudderSbLimitList = new ArrayList<Byte>(255);
ArrayList<Byte> gpsYearList = new ArrayList<Byte>(255);
ArrayList<Byte> gpsMonthList = new ArrayList<Byte>(255);
ArrayList<Byte> gpsDayList = new ArrayList<Byte>(255);
ArrayList<Byte> gpsHourList = new ArrayList<Byte>(255);
ArrayList<Byte> gpsMinuteList = new ArrayList<Byte>(255);
ArrayList<Byte> gpsSecondList = new ArrayList<Byte>(255);
ArrayList<Float> gpsCourseList = new ArrayList<Float>(255);
ArrayList<Float> gpsSpeedList = new ArrayList<Float>(255);
ArrayList<Float> gpsHdopList = new ArrayList<Float>(255);
ArrayList<Byte> gpsFixList = new ArrayList<Byte>(255);
ArrayList<Byte> gpsSatellitesList = new ArrayList<Byte>(255);
ArrayList<Byte> resetList = new ArrayList<Byte>(255);
ArrayList<Byte> loadList = new ArrayList<Byte>(255);
ArrayList<Float> rudderAngleList = new ArrayList<Float>(255);
ArrayList<Integer> propRpmList = new ArrayList<Integer>(255);
ArrayList<Byte> statusBitsList = new ArrayList<Byte>(255);
ArrayList<Byte> orderingList = new ArrayList<Byte>(255);
ArrayList<Float> rudderAngleCommandList = new ArrayList<Float>(255);
ArrayList<Integer> throttleCommandList = new ArrayList<Integer>(255);
ArrayList<Float> batteryVoltageList = new ArrayList<Float>(255);
ArrayList<Float> batteryAmperageList = new ArrayList<Float>(255);

// Boat state data playback
double[][] L2Playback;
double[][] globalPositionPlayback;
double[] headingPlayback;
double[][] localPositionPlayback;
double[][] velocityPlayback;
double[][] waypoint0Playback;
double[][] waypoint1Playback;
int[] rudderPotPlayback;
boolean[] rudderPortLimitPlayback;
boolean[] rudderSbLimitPlayback;
byte[] gpsYearPlayback;
byte[] gpsMonthPlayback;
byte[] gpsDayPlayback;
byte[] gpsHourPlayback;
byte[] gpsMinutePlayback;
byte[] gpsSecondPlayback;
float[] gpsCoursePlayback;
float[] gpsSpeedPlayback;
float[] gpsHdopPlayback;
byte[] gpsFixPlayback;
byte[] gpsSatellitesPlayback;
byte[] resetPlayback;
byte[] loadPlayback;
float[] rudderAnglePlayback;
int[] propRpmPlayback;
byte[] statusBitsPlayback;
byte[] orderingPlayback;
float[] rudderAngleCommandPlayback;
int[] throttleCommandPlayback;
float[] batteryVoltagePlayback;
float[] batteryAmperagePlayback;

// Rendering variables
PFont regularFont;
PFont boldFont;

void setup() {
  size(800, 600);
  frameRate(50);
  
  regularFont = loadFont("DejaVuSansMono-15.vlw");
  textFont(regularFont);
  boldFont = loadFont("DejaVuSansMono-Bold-15.vlw");
  
  controlP5 = new ControlP5(this);
  serialPortsList = controlP5.addDropdownList("serialPortsList",100,100,100,120);
  serialPortsList.setId(0);
  recordToggle = controlP5.addToggle("record",false,100,100,20,20);
  recordToggle.setId(1);
  recordTimer = new ControlTimer();
  recordTimer.setSpeedOfTime(1);
  recordTimerLabel = controlP5.addTextlabel("recordTimer",recordTimer.toString(),20,100);
  recordedMessagesLabel = controlP5.addTextlabel("recordedMessages",String.format("Messages: %d", recordedMessages),20,120);
  selectMatFile = controlP5.addButton("Select .mat file",0,350,50,100,20);
  selectMatFile.setId(2);
  customizeSerialPortsList(serialPortsList);
}

int lastDrawTime = 0;

void draw() {
  // Redraw the background at every timestep
  // (Necessary for clearing things like the dropdown)
  background(0);
  
  // Restore the cursor to the regular arrow.
  cursor(ARROW);
  
  // Reset fill to green (used for drawing the RX status circle)
  fill(0,100,0);
  
  // Grab some data from the serial port
  if (myPort != null && myPort.available() > 0 && !playing) {
    int bytesProcessed = 0;
    while(myPort.available() > 0) {
      byte[] inBuffer = new byte[127];
      int bytesToRead = myPort.readBytes(inBuffer);
      for (int i = 0; i < bytesToRead; i++) {
        if (buildAndCheckMessage(inBuffer[i])) {
              fill(0,255,0);
        }
      }
    }
  } else if (playing) {
    if (playbackIndex < headingPlayback.length) {
      if (millis() - lastPlaybackTime >= 20) { // Assume .02s sampletime
        L2.x = (float)L2Playback[playbackIndex][0];
        L2.y = (float)L2Playback[playbackIndex][1];
        L2.z = (float)L2Playback[playbackIndex][2];
        globalPosition.x = (float)globalPositionPlayback[playbackIndex][0];
        globalPosition.y = (float)globalPositionPlayback[playbackIndex][1];
        globalPosition.z = (float)globalPositionPlayback[playbackIndex][2];
        heading = (float)headingPlayback[playbackIndex];
        localPosition.x = (float)localPositionPlayback[playbackIndex][0];
        localPosition.y = (float)localPositionPlayback[playbackIndex][1];
        localPosition.z = (float)localPositionPlayback[playbackIndex][2];
        velocity.x = (float)velocityPlayback[playbackIndex][0];
        velocity.y = (float)velocityPlayback[playbackIndex][1];
        velocity.z = (float)velocityPlayback[playbackIndex][2];
        waypoint0.x = (float)waypoint0Playback[playbackIndex][0];
        waypoint0.y = (float)waypoint0Playback[playbackIndex][1];
        waypoint0.z = (float)waypoint0Playback[playbackIndex][2];
        waypoint1.x = (float)waypoint1Playback[playbackIndex][0];
        waypoint1.y = (float)waypoint1Playback[playbackIndex][1];
        waypoint1.z = (float)waypoint1Playback[playbackIndex][2];
        rudderPot = rudderPotPlayback[playbackIndex];
        rudderPortLimit = (boolean)rudderPortLimitPlayback[playbackIndex];
        rudderSbLimit = (boolean)rudderSbLimitPlayback[playbackIndex];
        gpsYear = gpsYearPlayback[playbackIndex];
        gpsMonth = gpsMonthPlayback[playbackIndex];
        gpsDay = gpsDayPlayback[playbackIndex];
        gpsHour = gpsHourPlayback[playbackIndex];
        gpsMinute = gpsMinutePlayback[playbackIndex];
        gpsSecond = gpsSecondPlayback[playbackIndex];
        gpsCourse = gpsCoursePlayback[playbackIndex];
        gpsSpeed = gpsSpeedPlayback[playbackIndex];
        gpsHdop = gpsHdopPlayback[playbackIndex];
        gpsFix = gpsFixPlayback[playbackIndex];
        gpsSatellites = gpsSatellitesPlayback[playbackIndex];
        reset = resetPlayback[playbackIndex];
        load = loadPlayback[playbackIndex];
        rudderAngle = rudderAnglePlayback[playbackIndex];
        propRpm = propRpmPlayback[playbackIndex];
        statusBits = statusBitsPlayback[playbackIndex];
        ordering = orderingPlayback[playbackIndex];
        rudderAngleCommand = rudderAngleCommandPlayback[playbackIndex];
        throttleCommand = throttleCommandPlayback[playbackIndex];
        batteryVoltage = batteryVoltagePlayback[playbackIndex];
        batteryAmperage = batteryAmperagePlayback[playbackIndex];
        
        playbackIndex++;
        lastPlaybackTime = millis();
      }
    } else {
      playing = false;
    }
  }
  // Draw the RX status
  arc(90, 90, 10, 10, 0, TWO_PI);
  // Draw the TX status
  fill(100,0,0);
  arc(75, 90, 10, 10, 0, TWO_PI);
  
  // Reset fill color to white
  fill(255);
  
  // Add text section headers
  textFont(boldFont);
  text("Rudder", 400, 390);
  text("GPS", 500, 390);
  text("Velocity", 200, 270);
  text("Throttle command", 200, 340);
  text("Local position", 200, 390);
  text("Global position", 200, 450);
  text("Waypoint0", 50, 290);
  text("Waypoint1", 50, 350);
  text("Power", 50, 420);
  text("Heading", 400, 290);
  text("Rudder angle command", 400, 340);
  text("Load", 570, 290);
  text("Reset status", 500, 70);
  text("Rudder angle", 630, 290);
  text("Prop speed", 630, 350);
  text("Mode:", 630, 400);
  text("Status:", 20, 170);
  textFont(regularFont);
  text("(press SPACE to toggle)", 100, 150);
  
  // Draw the rudder angle in degrees
  text(String.format("%3.1f\u00B0", rudderAngle * 180 / 3.14159), 630, 310);
  
  // Draw the prop speed in RPM
  text(String.format("%3d RPM", propRpm), 630, 370);

  // Draw the operational mode of the boat
  if ((statusBits & 0x01) != 0) {
    fill(0, 200, 0);
    textFont(boldFont);
    text("auto", 680, 400);
    textFont(regularFont);
    fill(255);
  } else {
    fill(200, 0, 0);
    textFont(boldFont);
    text("manual", 680, 400);
    textFont(regularFont);
    fill(255);
  }
  
  // Draw the various possible states within the status bit.
  // First we output the state of the HIL actuator sensor override line
  int nextStatusLineStart = 190;
  if ((statusBits & 0x02) != 0) {
    text("HIL actuator sensor override", 20, nextStatusLineStart);
    nextStatusLineStart += 20;
  }
  if ((statusBits & 0x04) != 0) {
    text("RC receiver disconnected", 20, nextStatusLineStart);
    nextStatusLineStart += 20;
  }
  if ((statusBits & 0x08) != 0) {
    text("CAN transmission error", 20, nextStatusLineStart);
    nextStatusLineStart += 20;
  }
  if ((statusBits & 0x10) != 0) {
    text("CAN reception error", 20, nextStatusLineStart);
    nextStatusLineStart += 20;
  }
  
  // Draw the reset bits
  int vertical = 90;
  if ((reset & 0x01) != 0) {
    text("Starting up...", 500, vertical);
    vertical += 20;
  }
  if ((reset & 0x02) != 0) {
    text("HIL mode changed", 500, vertical);
    vertical += 20;
  }
  if ((reset & 0x04) != 0) {
    text("HIL disconnected", 500, vertical);
    vertical += 20;
  }
  if ((reset & 0x08) != 0) {
    text("GPS unavailable", 500, vertical);
    vertical += 20;
  }
  if ((reset & 0x10) != 0) {
    text("Resetting after switching track", 500, vertical);
    vertical += 20;
  }
  if ((reset & 0x20) != 0) {
    text("Rudder uncalibrated", 500, vertical);
    vertical += 20;
  }
  if ((reset & 0x40) != 0) {
    text("Undergoing rudder calibration", 500, vertical);
    vertical += 20;
  }
  if ((reset & 0x80) != 0) {
    text("E-stop activated", 500, vertical);
  }
  
  // Draw the battery voltage and power draw in amps
  text(String.format("%2.1f V", batteryVoltage), 50, 440);
  text(String.format("%2.1f A", batteryAmperage), 50, 460);
  
  // Draw the load %
  text(String.format("%3d%%",load), 570, 310);
  
  // Reset fill color to white
  fill(255);
  
  // Add rudder sensor information
  text(rudderPot, 400, 405);
  text("Pt: " + (rudderPortLimit?"true":"false"), 400, 420);
  text("Sb: " + (rudderSbLimit?"true":"false"), 400, 435);
  
  // Add GPS sensor information
  text(String.format("%3.8f", gpsLatitude), 500, 405);
  text(String.format("%3.8f", gpsLongitude), 500, 420);
  text(gpsAltitude, 500, 435);
  text(String.format("%02d/%02d/%04d", gpsDay, gpsMonth, 2000+gpsYear), 500, 450);
  text(String.format("%02d:%02d:%02d", gpsHour, gpsMinute, gpsSecond), 500, 465);
  text(gpsCourse, 500, 480);
  text(gpsSpeed, 500, 495);
  text(gpsHdop, 500, 510);
  text(gpsFix, 500, 525);
  text(gpsSatellites, 500, 540);
  
  // Draw the velocity vector values
  text(String.format("%2.1f m/s", velocity.mag()), 200, 285);
  text(String.format("%2.1f knots", velocity.mag()*1.944), 200, 300);
  text(String.format("%2.1f mph", velocity.mag()*2.237), 200, 315);
  
  // Display the commanded throttle values. Since the throttle is a commanded current
  // and each step is 1/1024 of the maximum current I map the value into Amperes before
  // display.
  text(String.format("%3.1f A", float(throttleCommand)/1024*15), 200, 360);
  
  // Draw the local position values
  text(localPosition.x, 200, 405);
  text(localPosition.y, 200, 420);
  text(localPosition.z, 200, 435);
  
  // Draw the global position values
  text(globalPosition.x, 200, 465);
  text(globalPosition.y, 200, 480);
  text(globalPosition.z, 200, 495);
  
  // Draw the current and next waypoints
  text(waypoint0.x, 50, 305);
  text(waypoint0.y, 50, 320);
  text(waypoint0.z, 50, 335);
  text(waypoint1.x, 50, 365);
  text(waypoint1.y, 50, 380);
  text(waypoint1.z, 50, 395);
  
  // Display the boat heading converted to degrees
  text(String.format("%2.1f\u00B0", heading*57.2958), 400, 305);
  
  // Draw the commanded rudder angle in degrees
  text(String.format("%3.1f\u00B0", rudderAngleCommand * 180 / 3.14159), 400, 360);
  
  // Draw the boat and rudder
  fill(155,155,0);
  pushMatrix();
  translate(420,200);
  rotate(heading);
  pushMatrix();
  translate(0, 49);
  rotate(rudderAngle);
  rect(-2.5, 0, 5, 18);
  popMatrix();
  rect(-20, -50, 40, 100);
  popMatrix();
  
  // Draw the velocity vector on top of the boat
  if (velocity.mag() > 0) {
    // Prepare the vector from a NED to an ESD vector draw call
    drawVector(new PVector(velocity.y, -velocity.x, velocity.z), 420, 200, 40, 2, new PVector(34, 139, 34));
  }
  
  // Draw the L2 vector on top of the boat also
  if (L2.mag() > 0) {
    // Prepare the vector from a NED to an ESD vector draw call
    drawVector(new PVector(L2.y, -L2.x, L2.z), 420, 200, 10, 2, new PVector(34, 34, 139));
  }
  
  // Reset fill color to white
  fill(255);
  stroke(255);
  
  if (recording) {
    recordTimerLabel.setValue(recordTimer.toString());
    recordedMessagesLabel.setValue(String.format("Messages: %d", recordedMessages));
  }
}

/**
 * Implements the built-in callback keyReleased() that is provided by Processing
 * for handling keyboard events. This function is used to check for the spacebar
 * being pressed. If it was we toggle the recording state.
 */
void keyReleased() {
  if (key == ' ') {
      if (recording) {
        recording = false;
        stopRecordingAndSave();
      } else {
        recording = true;
        startRecording();
      }
      recordToggle.setState(recording);
  }
}

void controlEvent(ControlEvent theEvent) {
  // PulldownMenu is of type ControlGroup.
  // A controlEvent will be triggered from within the ControlGroup.
  // therefore you need to check the originator of the Event with
  // if (theEvent.isGroup())
  // to avoid an error message from controlP5.

  switch (theEvent.id()) {
    case(0):
      // check if the Event was triggered from a ControlGroup
      cursor(WAIT);
      if (myPort != null) {
        myPort.stop();
      }
      
      // TODO: This try/catch statement needs to be fixed to properly suppress the error
      // warning from gnu.io.PortInUseException and inform the user.
      try {
        myPort = new Serial(this, theEvent.group().stringValue(), 115200);
      }
      catch (Exception e) {
        println("Port in use or otherwise unavailable. Please select another.");
      }
      break;
    case(1):
      if (theEvent.value() == 0) {
        recording = false;
        stopRecordingAndSave();
      } else if (theEvent.value() == 1) {
        recording = true;
        startRecording();
      } 
      break;
    case(2):
      String inputMatFileName = selectInput();
      cursor(WAIT);
      if (inputMatFileName != null) {
        try {
          inputMatFileReader = new MatFileReader(inputMatFileName);
          L2Playback = ((MLDouble)inputMatFileReader.getMLArray("L2")).getArray().clone();
          globalPositionPlayback = ((MLDouble)inputMatFileReader.getMLArray("globalPosition")).getArray().clone();
          
          double[][] doubleData = ((MLDouble)inputMatFileReader.getMLArray("heading")).getArray();
          headingPlayback = new double[doubleData.length];
          for (int i=0;i<doubleData.length;i++) {
            headingPlayback[i] = doubleData[i][0];
          }
          
          localPositionPlayback = ((MLDouble)inputMatFileReader.getMLArray("localPosition")).getArray().clone();
          velocityPlayback = ((MLDouble)inputMatFileReader.getMLArray("velocity")).getArray().clone();
          waypoint0Playback = ((MLDouble)inputMatFileReader.getMLArray("waypoint0")).getArray().clone();
          waypoint1Playback = ((MLDouble)inputMatFileReader.getMLArray("waypoint1")).getArray().clone();
          
          long[][] longData = ((MLInt64)inputMatFileReader.getMLArray("rudderPot")).getArray();
          rudderPotPlayback = new int[longData.length];
          for (int i=0;i<longData.length;i++) {
            rudderPotPlayback[i] = (int)longData[i][0];
          }
          
          byte[][] byteData = ((MLInt8)inputMatFileReader.getMLArray("rudderPortLimit")).getArray();
          rudderPortLimitPlayback = new boolean[byteData.length];
          for (int i=0;i<byteData.length;i++) {
            rudderPortLimitPlayback[i] = byteData[i][0] > 0;
          }
          
          byteData = ((MLInt8)inputMatFileReader.getMLArray("rudderSbLimit")).getArray();
          rudderSbLimitPlayback = new boolean[byteData.length];
          for (int i=0;i<byteData.length;i++) {
            rudderSbLimitPlayback[i] = byteData[i][0] > 0;
          }
          
          byteData = ((MLInt8)inputMatFileReader.getMLArray("gpsYear")).getArray();
          gpsYearPlayback = new byte[byteData.length];
          for (int i=0;i<byteData.length;i++) {
            gpsYearPlayback[i] = (byte)byteData[i][0];
          }
          
          byteData = ((MLInt8)inputMatFileReader.getMLArray("gpsMonth")).getArray();
          gpsMonthPlayback = new byte[byteData.length];
          for (int i=0;i<byteData.length;i++) {
            gpsMonthPlayback[i] = (byte)byteData[i][0];
          }
          
          byteData = ((MLInt8)inputMatFileReader.getMLArray("gpsDay")).getArray();
          gpsDayPlayback = new byte[byteData.length];
          for (int i=0;i<byteData.length;i++) {
            gpsDayPlayback[i] = (byte)byteData[i][0];
          }
          
          byteData = ((MLInt8)inputMatFileReader.getMLArray("gpsHour")).getArray();
          gpsHourPlayback = new byte[byteData.length];
          for (int i=0;i<byteData.length;i++) {
            gpsHourPlayback[i] = (byte)byteData[i][0];
          }
          
          byteData = ((MLInt8)inputMatFileReader.getMLArray("gpsMinute")).getArray();
          gpsMinutePlayback = new byte[byteData.length];
          for (int i=0;i<byteData.length;i++) {
            gpsMinutePlayback[i] = (byte)byteData[i][0];
          }
          
          byteData = ((MLInt8)inputMatFileReader.getMLArray("gpsSecond")).getArray();
          gpsSecondPlayback = new byte[byteData.length];
          for (int i=0;i<byteData.length;i++) {
            gpsSecondPlayback[i] = (byte)byteData[i][0];
          }
          
          doubleData = ((MLDouble)inputMatFileReader.getMLArray("gpsCourse")).getArray();
          gpsCoursePlayback = new float[doubleData.length];
          for (int i=0;i<doubleData.length;i++) {
            gpsCoursePlayback[i] = (float)doubleData[i][0];
          }
          
          doubleData = ((MLDouble)inputMatFileReader.getMLArray("gpsSpeed")).getArray();
          gpsSpeedPlayback = new float[doubleData.length];
          for (int i=0;i<doubleData.length;i++) {
            gpsSpeedPlayback[i] = (float)doubleData[i][0];
          }
          
          doubleData = ((MLDouble)inputMatFileReader.getMLArray("gpsHdop")).getArray();
          gpsHdopPlayback = new float[doubleData.length];
          for (int i=0;i<doubleData.length;i++) {
            gpsHdopPlayback[i] = (float)doubleData[i][0];
          }
          
          byteData = ((MLInt8)inputMatFileReader.getMLArray("gpsFix")).getArray();
          gpsFixPlayback = new byte[byteData.length];
          for (int i=0;i<byteData.length;i++) {
            gpsFixPlayback[i] = (byte)byteData[i][0];
          }
          
          byteData = ((MLInt8)inputMatFileReader.getMLArray("gpsSatellites")).getArray();
          gpsSatellitesPlayback = new byte[byteData.length];
          for (int i=0;i<byteData.length;i++) {
            gpsSatellitesPlayback[i] = (byte)byteData[i][0];
          }
          
          byteData = ((MLUInt8)inputMatFileReader.getMLArray("reset")).getArray();
          resetPlayback = new byte[byteData.length];
          for (int i=0;i<byteData.length;i++) {
            resetPlayback[i] = (byte)byteData[i][0];
          }
          
          byteData = ((MLUInt8)inputMatFileReader.getMLArray("load")).getArray();
          loadPlayback = new byte[byteData.length];
          for (int i=0;i<byteData.length;i++) {
            loadPlayback[i] = (byte)byteData[i][0];
          }
          
          doubleData = ((MLDouble)inputMatFileReader.getMLArray("rudderAngle")).getArray();
          rudderAnglePlayback = new float[doubleData.length];
          for (int i=0;i<doubleData.length;i++) {
            rudderAnglePlayback[i] = (float)doubleData[i][0];
          }
          
          longData = ((MLInt64)inputMatFileReader.getMLArray("propRpm")).getArray();
          propRpmPlayback = new int[longData.length];
          for (int i=0;i<longData.length;i++) {
            propRpmPlayback[i] = (int)longData[i][0];
          }
          
          byteData = ((MLUInt8)inputMatFileReader.getMLArray("statusBits")).getArray();
          statusBitsPlayback = new byte[byteData.length];
          for (int i=0;i<byteData.length;i++) {
            statusBitsPlayback[i] = (byte)byteData[i][0];
          }
          
          byteData = ((MLUInt8)inputMatFileReader.getMLArray("ordering")).getArray();
          orderingPlayback = new byte[byteData.length];
          for (int i=0;i<byteData.length;i++) {
            orderingPlayback[i] = (byte)byteData[i][0];
          }
          
          doubleData = ((MLDouble)inputMatFileReader.getMLArray("rudderAngleCommand")).getArray();
          rudderAngleCommandPlayback = new float[doubleData.length];
          for (int i=0;i<doubleData.length;i++) {
            rudderAngleCommandPlayback[i] = (float)doubleData[i][0];
          }
          
          longData = ((MLInt64)inputMatFileReader.getMLArray("throttleCommand")).getArray();
          throttleCommandPlayback = new int[longData.length];
          for (int i=0;i<longData.length;i++) {
            throttleCommandPlayback[i] = (int)longData[i][0];
          }
          
          doubleData = ((MLDouble)inputMatFileReader.getMLArray("batteryVoltage")).getArray();
          batteryVoltagePlayback = new float[doubleData.length];
          for (int i=0;i<doubleData.length;i++) {
            batteryVoltagePlayback[i] = (float)doubleData[i][0];
          }
          
          doubleData = ((MLDouble)inputMatFileReader.getMLArray("batteryAmperage")).getArray();
          batteryAmperagePlayback = new float[doubleData.length];
          for (int i=0;i<doubleData.length;i++) {
            batteryAmperagePlayback[i] = (float)doubleData[i][0];
          }
          
        } catch (IOException e) {
          e.printStackTrace();
          exit();
        }
        
        if (inputMatFileReader != null) {
          playing = true;
          playbackIndex = 0;
        }
      }
      break;
  }
}

void customizeSerialPortsList(DropdownList ddl) {
  ddl.setBackgroundColor(color(190));
  ddl.setItemHeight(20);
  ddl.setBarHeight(15);
  
  ddl.captionLabel().set("Select serial port");
  ddl.captionLabel().style().marginTop = 3;
  ddl.captionLabel().style().marginLeft = 3;
  ddl.valueLabel().style().marginTop = 3;
  
  String[] ports = Serial.list();
  for(int i=0;i<ports.length;i++) {
    ddl.addItem(ports[i],i);
  }
  
  ddl.setColorBackground(color(60));
  ddl.setColorActive(color(255,128));
}

public void startRecording() {
  recordTimer.reset();
  
  // Clear all the data structures for new data
  L2List.clear();
  globalPositionList.clear();
  headingList.clear();
  localPositionList.clear();
  velocityList.clear();
  waypoint0List.clear();
  waypoint1List.clear();
  rudderPotList.clear();
  rudderPortLimitList.clear();
  rudderSbLimitList.clear();
  gpsYearList.clear();
  gpsMonthList.clear();
  gpsDayList.clear();
  gpsHourList.clear();
  gpsMinuteList.clear();
  gpsSecondList.clear();
  gpsCourseList.clear();
  gpsSpeedList.clear();
  gpsHdopList.clear();
  gpsFixList.clear();
  gpsSatellitesList.clear();
  resetList.clear();
  loadList.clear();
  rudderAngleList.clear();
  propRpmList.clear();
  statusBitsList.clear();
  orderingList.clear();
  rudderAngleCommandList.clear();
  throttleCommandList.clear();
  batteryVoltageList.clear();
  batteryAmperageList.clear();
  
  // Reset the messages counter
  recordedMessages = new Long(0);
}

public void stopRecordingAndSave() {
  if (recordedMessages > 0) {
  
    float[][] t = new float[L2List.size()][3];
    
    L2List.toArray(t);
    double[][] output = new double[L2List.size()][3];
    for (int i = 0; i < L2List.size(); i++){
      for (int j=0;j<3;j++) {
        output[i][j] = (double)t[i][j];
      }
    }
    MLDouble L2ML = new MLDouble("L2", output);
    
    globalPositionList.toArray(t);
    output = new double[globalPositionList.size()][3];
    for (int i = 0; i < globalPositionList.size(); i++){
      for (int j=0;j<3;j++) {
        output[i][j] = (double)t[i][j];
      }
    }
    MLDouble globalPositionML = new MLDouble("globalPosition", output);
    
    localPositionList.toArray(t);
    output = new double[localPositionList.size()][3];
    for (int i = 0; i < localPositionList.size(); i++){
      for (int j=0;j<3;j++) {
        output[i][j] = (double)t[i][j];
      }
    }
    MLDouble localPositionML = new MLDouble("localPosition", output);
    
    Float[] float_g = new Float[headingList.size()];
    headingList.toArray(float_g);
    double[] gg = new double[headingList.size()];
    for (int i = 0; i < headingList.size(); i++){
      gg[i] = (double)float_g[i];
    }
    MLDouble headingML = new MLDouble("heading", gg, gg.length);
    
    velocityList.toArray(t);
    output = new double[velocityList.size()][3];
    for (int i = 0; i < velocityList.size(); i++){
      for (int j=0;j<3;j++) {
        output[i][j] = (double)t[i][j];
      }
    }
    MLDouble velocityML = new MLDouble("velocity", output);
    
    waypoint0List.toArray(t);
    output = new double[waypoint0List.size()][3];
    for (int i = 0; i < waypoint0List.size(); i++){
      for (int j=0;j<3;j++) {
        output[i][j] = (double)t[i][j];
      }
    }
    MLDouble waypoint0ML = new MLDouble("waypoint0", output);
    
    waypoint1List.toArray(t);
    output = new double[waypoint1List.size()][3];
    for (int i = 0; i < waypoint1List.size(); i++){
      for (int j=0;j<3;j++) {
        output[i][j] = (double)t[i][j];
      }
    }
    MLDouble waypoint1ML = new MLDouble("waypoint1", output);
    
    Integer[] int_g = new Integer[rudderPotList.size()];
    rudderPotList.toArray(int_g);
    long[] long_gg = new long[rudderPotList.size()];
    for (int i = 0; i < rudderPotList.size(); i++){
      long_gg[i] = (long)int_g[i];
    }
    MLInt64 rudderPotML = new MLInt64("rudderPot", long_gg, long_gg.length);
    
    Byte[] byte_g = new Byte[rudderPortLimitList.size()];
    rudderPortLimitList.toArray(byte_g);
    byte[] byte_gg = new byte[rudderPortLimitList.size()];
    for (int i = 0; i < rudderPortLimitList.size(); i++){
      byte_gg[i] = (byte)byte_g[i];
    }
    MLInt8 rudderPortML = new MLInt8("rudderPortLimit", byte_gg, byte_gg.length);
    
    byte_g = new Byte[rudderSbLimitList.size()];
    rudderSbLimitList.toArray(byte_g);
    byte_gg = new byte[rudderSbLimitList.size()];
    for (int i = 0; i < rudderSbLimitList.size(); i++){
      byte_gg[i] = (byte)byte_g[i];
    }
    MLInt8 rudderSbML = new MLInt8("rudderSbLimit", byte_gg, byte_gg.length);
    
    byte_g = new Byte[gpsYearList.size()];
    gpsYearList.toArray(byte_g);
    byte_gg = new byte[gpsYearList.size()];
    for (int i = 0; i < gpsYearList.size(); i++){
      byte_gg[i] = (byte)byte_g[i];
    }
    MLInt8 gpsYearML = new MLInt8("gpsYear", byte_gg, byte_gg.length);
    
    byte_g = new Byte[gpsMonthList.size()];
    gpsMonthList.toArray(byte_g);
    byte_gg = new byte[gpsMonthList.size()];
    for (int i = 0; i < gpsMonthList.size(); i++){
      byte_gg[i] = (byte)byte_g[i];
    }
    MLInt8 gpsMonthML = new MLInt8("gpsMonth", byte_gg, byte_gg.length);
    
    byte_g = new Byte[gpsDayList.size()];
    gpsDayList.toArray(byte_g);
    byte_gg = new byte[gpsDayList.size()];
    for (int i = 0; i < gpsDayList.size(); i++){
      byte_gg[i] = (byte)byte_g[i];
    }
    MLInt8 gpsDayML = new MLInt8("gpsDay", byte_gg, byte_gg.length);
    
    byte_g = new Byte[gpsHourList.size()];
    gpsHourList.toArray(byte_g);
    byte_gg = new byte[gpsHourList.size()];
    for (int i = 0; i < gpsHourList.size(); i++){
      byte_gg[i] = (byte)byte_g[i];
    }
    MLInt8 gpsHourML = new MLInt8("gpsHour", byte_gg, byte_gg.length);
    
    byte_g = new Byte[gpsMinuteList.size()];
    gpsMinuteList.toArray(byte_g);
    byte_gg = new byte[gpsMinuteList.size()];
    for (int i = 0; i < gpsMinuteList.size(); i++){
      byte_gg[i] = (byte)byte_g[i];
    }
    MLInt8 gpsMinuteML = new MLInt8("gpsMinute", byte_gg, byte_gg.length);
    
    byte_g = new Byte[gpsSecondList.size()];
    gpsSecondList.toArray(byte_g);
    byte_gg = new byte[gpsSecondList.size()];
    for (int i = 0; i < gpsSecondList.size(); i++){
      byte_gg[i] = (byte)byte_g[i];
    }
    MLInt8 gpsSecondML = new MLInt8("gpsSecond", byte_gg, byte_gg.length);
    
    float_g = new Float[gpsCourseList.size()];
    gpsCourseList.toArray(float_g);
    gg = new double[gpsCourseList.size()];
    for (int i = 0; i < gpsCourseList.size(); i++){
      gg[i] = (double)float_g[i];
    }
    MLDouble gpsCourseML = new MLDouble("gpsCourse", gg, gg.length);
    
    float_g = new Float[gpsSpeedList.size()];
    gpsSpeedList.toArray(float_g);
    gg = new double[gpsSpeedList.size()];
    for (int i = 0; i < gpsSpeedList.size(); i++){
      gg[i] = (double)float_g[i];
    }
    MLDouble gpsSpeedML = new MLDouble("gpsSpeed", gg, gg.length);
    
    float_g = new Float[gpsHdopList.size()];
    gpsHdopList.toArray(float_g);
    gg = new double[gpsHdopList.size()];
    for (int i = 0; i < gpsHdopList.size(); i++){
      gg[i] = (double)float_g[i];
    }
    MLDouble gpsHdopML = new MLDouble("gpsHdop", gg, gg.length);
    
    byte_g = new Byte[gpsFixList.size()];
    gpsFixList.toArray(byte_g);
    byte_gg = new byte[gpsFixList.size()];
    for (int i = 0; i < gpsFixList.size(); i++){
      byte_gg[i] = (byte)byte_g[i];
    }
    MLInt8 gpsFixML = new MLInt8("gpsFix", byte_gg, byte_gg.length);
    
    byte_g = new Byte[gpsSatellitesList.size()];
    gpsSatellitesList.toArray(byte_g);
    byte_gg = new byte[gpsSatellitesList.size()];
    for (int i = 0; i < gpsSatellitesList.size(); i++){
      byte_gg[i] = (byte)byte_g[i];
    }
    MLInt8 gpsSatellitesML = new MLInt8("gpsSatellites", byte_gg, byte_gg.length);
    
    byte_g = new Byte[resetList.size()];
    resetList.toArray(byte_g);
    byte_gg = new byte[resetList.size()];
    for (int i = 0; i < resetList.size(); i++){
      byte_gg[i] = (byte)byte_g[i];
    }
    MLUInt8 resetML = new MLUInt8("reset", byte_gg, byte_gg.length);
    
    byte_g = new Byte[loadList.size()];
    loadList.toArray(byte_g);
    byte_gg = new byte[loadList.size()];
    for (int i = 0; i < loadList.size(); i++){
      byte_gg[i] = (byte)byte_g[i];
    }
    MLUInt8 loadML = new MLUInt8("load", byte_gg, byte_gg.length);
    
    float_g = new Float[rudderAngleList.size()];
    rudderAngleList.toArray(float_g);
    gg = new double[rudderAngleList.size()];
    for (int i = 0; i < rudderAngleList.size(); i++){
      gg[i] = (double)float_g[i];
    }
    MLDouble rudderAngleML = new MLDouble("rudderAngle", gg, gg.length);
    
    int_g = new Integer[propRpmList.size()];
    propRpmList.toArray(int_g);
    long_gg = new long[propRpmList.size()];
    for (int i = 0; i < propRpmList.size(); i++){
      long_gg[i] = (long)int_g[i];
    }
    MLInt64 propRpmML = new MLInt64("propRpm", long_gg, long_gg.length);
    
    byte_g = new Byte[statusBitsList.size()];
    statusBitsList.toArray(byte_g);
    byte_gg = new byte[statusBitsList.size()];
    for (int i = 0; i < statusBitsList.size(); i++){
      byte_gg[i] = (byte)byte_g[i];
    }
    MLUInt8 statusBitsML = new MLUInt8("statusBits", byte_gg, byte_gg.length);
    
    byte_g = new Byte[orderingList.size()];
    orderingList.toArray(byte_g);
    byte_gg = new byte[orderingList.size()];
    for (int i = 0; i < orderingList.size(); i++){
      byte_gg[i] = (byte)byte_g[i];
    }
    MLUInt8 orderingML = new MLUInt8("ordering", byte_gg, byte_gg.length);
  
    float_g = new Float[rudderAngleCommandList.size()];
    rudderAngleCommandList.toArray(float_g);
    gg = new double[rudderAngleCommandList.size()];
    for (int i = 0; i < rudderAngleCommandList.size(); i++){
      gg[i] = (double)float_g[i];
    }
    MLDouble rudderAngleCommandML = new MLDouble("rudderAngleCommand", gg, gg.length);
    
    int_g = new Integer[throttleCommandList.size()];
    throttleCommandList.toArray(int_g);
    long_gg = new long[throttleCommandList.size()];
    for (int i = 0; i < throttleCommandList.size(); i++){
      long_gg[i] = (long)int_g[i];
    }
    MLInt64 throttleCommandML = new MLInt64("throttleCommand", long_gg, long_gg.length);
  
    float_g = new Float[batteryVoltageList.size()];
    batteryVoltageList.toArray(float_g);
    gg = new double[batteryVoltageList.size()];
    for (int i = 0; i < batteryVoltageList.size(); i++){
      gg[i] = (double)float_g[i];
    }
    MLDouble batteryVoltageML = new MLDouble("batteryVoltage", gg, gg.length);
  
    float_g = new Float[batteryAmperageList.size()];
    batteryAmperageList.toArray(float_g);
    gg = new double[batteryAmperageList.size()];
    for (int i = 0; i < batteryAmperageList.size(); i++){
      gg[i] = (double)float_g[i];
    }
    MLDouble batteryAmperageML = new MLDouble("batteryAmperage", gg, gg.length);
    
    ArrayList matList = new ArrayList();
    matList.add(L2ML);
    matList.add(headingML);
    matList.add(globalPositionML);
    matList.add(localPositionML);
    matList.add(velocityML);
    matList.add(waypoint0ML);
    matList.add(waypoint1ML);
    matList.add(rudderPotML);
    matList.add(rudderPortML);
    matList.add(rudderSbML);
    matList.add(gpsYearML);
    matList.add(gpsMonthML);
    matList.add(gpsDayML);
    matList.add(gpsHourML);
    matList.add(gpsMinuteML);
    matList.add(gpsSecondML);
    matList.add(gpsCourseML);
    matList.add(gpsSpeedML);
    matList.add(gpsHdopML);
    matList.add(gpsFixML);
    matList.add(gpsSatellitesML);
    matList.add(resetML);
    matList.add(loadML);
    matList.add(rudderAngleML);
    matList.add(propRpmML);
    matList.add(statusBitsML);
    matList.add(orderingML);
    matList.add(rudderAngleCommandML);
    matList.add(throttleCommandML);
    matList.add(batteryVoltageML);
    matList.add(batteryAmperageML);
    
    try {
      Calendar cal = Calendar.getInstance();
      SimpleDateFormat sdf = new SimpleDateFormat("yyyyMMdd-hhmmss");
      new MatFileWriter(sketchPath(sdf.format(cal.getTime())+".mat"), matList);
    }
    catch (Exception e){
      println("Failed to write output to a .mat file");
    }
  }
}

