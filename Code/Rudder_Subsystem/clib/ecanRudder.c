#include "ecanDefinitions.h"
#include "rudder_Subsystem.h"
#include "MessageScheduler.h"
#include "ecanRudder.h"
#include "ecanFunctions.h"
#include "nmea2000.h"
#include "Nmea2000Encode.h"
#include "Nvram.h"
#include "CanMessages.h"

// Declare some constants for use with the message scheduler
// (don't use PGN or message ID as it must be a uint8)
enum {
    SCHED_ID_RUDDER_ANGLE   = 1,
    SCHED_ID_CUSTOM_LIMITS  = 2,
    SCHED_ID_TEMPERATURE    = 3,
    SCHED_ID_STATUS         = 4
};

// Define some calibration setting constants
#define TO_PORT      1
#define TO_STARBOARD 0
enum {
	RUDDER_CAL_STATE_NULL,
	RUDDER_CAL_STATE_INIT,
	RUDDER_CAL_STATE_FIRST_TO_PORT,
	RUDDER_CAL_STATE_FIRST_TO_STARBOARD,
	RUDDER_CAL_STATE_SECOND_TO_PORT,
	RUDDER_CAL_STATE_SECOND_TO_STARBOARD,
	RUDDER_CAL_STATE_RECENTER
};

// Store the unique ID for this node.
const uint8_t nodeId = CAN_NODE_RUDDER_CONTROLLER;

// Instantiate a struct to store calibration data.
struct RudderCalibrationData rudderCalData = {0};

// Instantiate a struct to store rudder input data.
struct RudderSensorData rudderSensorData = {0};

// Initialize the message scheduler

// Set up the message scheduler for MAVLink transmission
#define ECAN_MSGS_SIZE 4
static uint8_t ids[ECAN_MSGS_SIZE] = {
    SCHED_ID_RUDDER_ANGLE,
    SCHED_ID_CUSTOM_LIMITS,
    SCHED_ID_TEMPERATURE,
    SCHED_ID_STATUS
};
static uint16_t tsteps[ECAN_MSGS_SIZE][2][8] = {};
static uint8_t  mSizes[ECAN_MSGS_SIZE];
static MessageSchedule sched = {
	ECAN_MSGS_SIZE,
	ids,
	mSizes,
	0,
	tsteps
};

void RudderSubsystemInit(void)
{
	// Transmit the rudder angle at 10Hz
	if (!AddMessageRepeating(&sched, SCHED_ID_RUDDER_ANGLE, 10)) {
		while (1);
	}

	// Transmit status at 4Hz
	if (!AddMessageRepeating(&sched, SCHED_ID_CUSTOM_LIMITS, 4)) {
		while (1);
	}

	// Transmit temperature at 1Hz
	if (!AddMessageRepeating(&sched, SCHED_ID_TEMPERATURE, 1)) {
		while (1);
	}

	// Transmit error/status at 2Hz
	if (!AddMessageRepeating(&sched, SCHED_ID_STATUS, 2)) {
		while (1);
	}
}

void RudderCalibrate(void)
{
	if (rudderCalData.CalibrationState == RUDDER_CAL_STATE_INIT) {
            rudderCalData.Calibrated = false;
		rudderCalData.Calibrating = true;
		rudderCalData.CommandedRun = true;
                rudderCalData.PortLimitValue = 0;
                rudderCalData.StarLimitValue = 0;
		if (rudderSensorData.StarLimit) {
			rudderCalData.CalibrationState = RUDDER_CAL_STATE_FIRST_TO_PORT;
			rudderCalData.CommandedDirection = TO_PORT;
		} else {
			rudderCalData.CalibrationState = RUDDER_CAL_STATE_FIRST_TO_STARBOARD;
			rudderCalData.CommandedDirection = TO_STARBOARD;
		}
	} else if (rudderCalData.CalibrationState == RUDDER_CAL_STATE_FIRST_TO_PORT) {
		if (rudderSensorData.PortLimit) {
			rudderCalData.PortLimitValue = rudderSensorData.PotValue;
			rudderCalData.CalibrationState = RUDDER_CAL_STATE_SECOND_TO_STARBOARD;
			rudderCalData.CommandedDirection = TO_STARBOARD;
		}
	} else if (rudderCalData.CalibrationState == RUDDER_CAL_STATE_FIRST_TO_STARBOARD) {
		if (rudderSensorData.StarLimit) {
			rudderCalData.StarLimitValue = rudderSensorData.PotValue;
			rudderCalData.CalibrationState = RUDDER_CAL_STATE_SECOND_TO_PORT;
			rudderCalData.CommandedDirection = TO_PORT;
		}
	} else if (rudderCalData.CalibrationState == RUDDER_CAL_STATE_SECOND_TO_PORT) {
		if (rudderSensorData.PortLimit) {
			rudderCalData.PortLimitValue = rudderSensorData.PotValue;
			SaveRudderCalibrationRange();
			rudderCalData.CalibrationState = RUDDER_CAL_STATE_RECENTER;
			rudderCalData.CommandedDirection = TO_STARBOARD;
			rudderCalData.Calibrated = true;
		}
	} else if (rudderCalData.CalibrationState == RUDDER_CAL_STATE_SECOND_TO_STARBOARD) {
		if (rudderSensorData.StarLimit) {
			rudderCalData.StarLimitValue = rudderSensorData.PotValue;
			SaveRudderCalibrationRange();
			rudderCalData.CalibrationState = RUDDER_CAL_STATE_RECENTER;
			rudderCalData.CommandedDirection = TO_PORT;
			rudderCalData.Calibrated = true;
		}
	}else if (rudderCalData.CalibrationState == RUDDER_CAL_STATE_RECENTER) {
		if (fabs(rudderSensorData.RudderPositionAngle) < 0.1) {
			rudderCalData.CommandedRun = false;
			rudderCalData.Calibrating = false;
			rudderCalData.CalibrationState = RUDDER_CAL_STATE_NULL;
		}
	}
}

void RudderSendNmea(void)
{
	// Set CAN header information.
	tCanMessage msg;
	PackagePgn127245(&msg, nodeId, 0xFF, 0xF, 0.0, rudderSensorData.RudderPositionAngle);

	// And finally transmit it.
	ecan1_buffered_transmit(&msg);
}

void RudderSendCustomLimit(void)
{
	tCanMessage msg;
	CanMessagePackageRudderDetails(&msg, rudderSensorData.PotValue,
										 rudderCalData.PortLimitValue,
										 rudderCalData.StarLimitValue,
										 rudderSensorData.PortLimit,
										 rudderSensorData.StarLimit,
										 true,
										 rudderCalData.Calibrated,
										 rudderCalData.Calibrating
										 );

	ecan1_buffered_transmit(&msg);
}

//configures the message into a tCanMessage, and sends it
void RudderSendTemperature(void)
{
	// Specify a new CAN message w/ metadata
	tCanMessage msg;
	msg.id = Iso11783Encode(PGN_ENV_PARAMETERS2, 10, 0xFF, 2);
	msg.buffer = 0;
	msg.message_type = CAN_MSG_DATA;
	msg.frame_type = CAN_FRAME_EXT;
	msg.validBytes = 8;

	// Now set the data.
	msg.payload[0] = 0xFF;      // SID
	msg.payload[1] = 2;         // Temperature instance (Inside)
	msg.payload[1] |= 0x3 << 6; // Humidity instance (Invalid)
	// Record the temperature in units of .01Kelvin.
	uint16_t temp = (uint16_t)((rudderSensorData.Temperature + 273.2) * 100);
	msg.payload[2] = (uint8_t)temp;
	msg.payload[3] = (uint8_t)(temp >> 8);
	msg.payload[4] = 0xFF; // Humidity is invalid
	msg.payload[5] = 0xFF; // Humidity is invalid
	msg.payload[6] = 0xFF; // Atmospheric pressure is invalid
	msg.payload[7] = 0xFF; // Atmospheric pressure is invalid

	// And finally transmit it.
	//ecan1_buffered_transmit(&msg);
}

// Transmits the STATUS message that every node should transmit at 2Hz.
void RudderSendStatus(void)
{
	// Specify a new CAN message w/ metadata
	tCanMessage msg;
	uint16_t status = (rudderSensorData.PortLimit << 3) |
	                  (rudderSensorData.StarLimit << 2) |
	                  (rudderCalData.Calibrated << 0) |
	                  (rudderCalData.Calibrating << 1);
	uint16_t errors = 0;
	CanMessagePackageStatus(&msg, nodeId, status, errors);

	// And finally transmit it.
	ecan1_buffered_transmit(&msg);
}

void SendAndReceiveEcan(void)
{
	uint8_t messagesLeft = 0;
	tCanMessage msg;
	uint32_t pgn;

	do {
		int foundOne = ecan1_receive(&msg, &messagesLeft);
		if (foundOne) {
			// Process custom rudder messages. Anything not explicitly handled is assumed to be a NMEA2000 message.
			// If we receive a calibration message, start calibration if we aren't calibrating right now.
			if (msg.id == CAN_MSG_ID_RUDDER_SET_STATE) {
				if ((msg.payload[0] & 0x01) == 1 && rudderCalData.Calibrating == false) {
					rudderCalData.CalibrationState = RUDDER_CAL_STATE_INIT;
				}
			// Update send message rates
			} else if (msg.id == CAN_MSG_ID_RUDDER_SET_TX_RATE) {
				UpdateMessageRate(msg.payload[0], msg.payload[1]);
			} else {
				pgn = Iso11783Decode(msg.id, NULL, NULL, NULL);
				switch (pgn) {
				case PGN_RUDDER:
					ParsePgn127245(msg.payload, NULL, NULL, NULL, &rudderSensorData.CommandedRudderAngle, NULL);
				break;
				}
			}
		}
	} while (messagesLeft > 0);

	// And now transmit all messages for this timestep
        uint8_t msgs[ECAN_MSGS_SIZE];
	uint8_t count = GetMessagesForTimestep(&sched, msgs);
        int i;
	for (i = 0; i < count; ++i) {
            switch (msgs[i]) {
                case SCHED_ID_CUSTOM_LIMITS:
                    RudderSendCustomLimit();
                break;

                case SCHED_ID_RUDDER_ANGLE:
                    RudderSendNmea();
                break;

                case SCHED_ID_TEMPERATURE:
                    RudderSendTemperature();
                break;

                case SCHED_ID_STATUS:
                    RudderSendStatus();
                break;
            }
	}
}

void UpdateMessageRate(const uint8_t angleRate, const uint8_t statusRate)
{
    // Handle the angle message first
    if (angleRate != 0xFF) {
        if (angleRate == 0x00) {
            // TODO: write code for this
        } else if ((angleRate <= 100) && (angleRate >= 1)) {
            RemoveMessage(&sched, SCHED_ID_RUDDER_ANGLE);
            AddMessageRepeating(&sched, SCHED_ID_RUDDER_ANGLE, angleRate);
        }
    }

    // Handle the status message
    if (statusRate != 0xFF) {
        if (statusRate == 0x00) {
            // TODO: write code for this
        } else if ((statusRate <= 100) && (statusRate >= 1)) {
            RemoveMessage(&sched, SCHED_ID_CUSTOM_LIMITS);
            AddMessageRepeating(&sched, SCHED_ID_CUSTOM_LIMITS, statusRate);
        }
    }
}

void CalculateRudderAngle(void)
{
    rudderSensorData.RudderPositionAngle = PotToRads(rudderSensorData.PotValue, rudderCalData.StarLimitValue, rudderCalData.PortLimitValue);
}

float PotToRads(uint16_t input, uint16_t highSide, uint16_t lowSide)
{
    // This function converts the potentiometer reading from the boat's rudder sensors
    // to a value in degrees. It relies on each limit being hit to set the bounds on
    // the rudder sensor value and then maps that value from that range into the new
    // range of -.07854 to .07854 (+-45 deg). It is loosely based off of the integer
    // mapping function at: http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1289758376

    // Prepare our input. Here we subtract the baseline 'lowSide' value off of
    // the input so we start with a range from [0..highSide-lowSide] and map it
    // into the output range of 45 degrees.
    int32_t in_max = highSide - lowSide;
    float val = (float)((int32_t)input - (int32_t)lowSide);

    // Do the actual conversion reversing the range and mapping it into [.7854:-0.7854]
    float rads = (0.5 - val/((float)in_max))*2*0.7854;

    // Finally cap the value to +- our range to prevent odd errors later.
    if (rads > 0.7854) {
        rads = 0.7854;
    } else if (rads < -0.7854) {
        rads = -0.7854;
    }
	return rads;
}
