/**
 * This file contains all of the MAVLink interfacing necessary by Sealion.
 * It relies heavily on the MavlinkMessageScheduler for scheduling transmission
 * of MAVLink messages such as to not overload the interface.
 *
 * The main functions are at the bottom: MavLinkTransmit() and MavLinkReceive()
 * handle the dispatching of messages (and sending of non-FSM reliant ones) and
 * the reception of messages and forwarding of reception events to the relevent
 * FSMs. The two state machine functions (MavLinkEvaluateMissionState and
 * MavLinkEvaluateParameterState) both contain all of the state logic for the
 * MAVLink mission and parameter protocols. As the specifications for those two
 * protocols are not fully defined they have been tested with QGroundControl to
 * work correctly.
 *
 * This code was written to be as generic as possible. If you remove all of the
 * custom messages and switch the transmission from uart1EnqueueData() it should
 * be almost exclusively relient on modules like controller (for the MissionManager*() functions)
 * and the scheduler.
 */

#ifndef MAVLINK_GLUE_H
#define MAVLINK_GLUE_H

// Need this in the header because of MavLinkSendStatusText()
#include <mavlink.h>

#include "Types.h"
// Define M_PI_2 here for the MAVLink library as the XC16 doesn't provide this constant by default.
#define M_PI_2 1.57079632679489661923

// The main Simulink project `controller.mdl` declares the InternalVariables struct.
#include "controller.h"
extern InternalVariables controllerVars; // Track a bunch of internal variables from the controller.

/**
 * Initialize MAVLink transmission. This just sets up the MAVLink scheduler with the basic
 * repeatedly-transmit messages.
 */
void MavLinkInit(void);

/**
 * Sends the specified text in a Common::STATUSTEXT message out over UART1.
 * @param text An up-to-50 character string for transmitting.
 */
void MavLinkSendStatusText(enum MAV_SEVERITY severity, const char *text);

/**
 * Transmit the current mission index via UART1. GetCurrentMission returns a -1 if there're no missions,
 * so we check and only transmit valid current missions.
 */
void MavLinkSendCurrentMission(void);

/**
 * Transmit a CONTROLLER_DATA message. This message was not designed to be scheduled as normal, which
 * is why it actually has function arguments. This should be sent explicitly immediately after the
 * controller loop has run.
 */
void MavLinkSendControllerData(const float attitude_quat[4], float commandedRudder, int16_t commandedThrottle);

void GetMavLinkManualControl(float *rc, int16_t *tc);

/**
 * Set the starting point for the mission manager to the boat's current location.
 */
void SetStartingPointToCurrentLocation(void);

/**
 * Increments the parameter counter for use within MAVLink's parameter protocol. Should be called at
 * a constant rate.
 */
void IncrementParameterCounter(void);

/**
 * Increments the mission counter for use within MAVLink's parameter protocol. Should be called at
 * a constant rate.
 */
void IncrementMissionCounter(void);

/**
* @brief Receive communication packets and handle them. Should be called at the system sample rate.
*
* This function decodes packets on the protocol level and also handles
* their value by calling the appropriate functions.
*/
void MavLinkReceive(void);

/**
 * This function handles transmission of MavLink messages taking into account transmission
 * speed, message size, and desired transmission rate. These messages are output over UART1 to the
 * groundstation radio.
 */
void MavLinkTransmitGroundstation(void);

/**
 * This function handles transmission of MavLink messages taking into account transmission
 * speed, message size, and desired transmission rate. These messages are output over UART2 to the
 * datalogger.
 */
void MavLinkTransmitDatalogger(void);

/**
 * Transmit all MAVLink parameters as double-transmitted PARAM_VALUE messages. This useful for
 * debugging purposes as then the log contains a record of all parameter settings. Note that if this
 * process is interrupted by a request for all parameters, it will stop doing its state machine and
 * switch over to following the parameter protocol. This is not the case when receiving a SET message,
 * that will be ignored and have to be caught by a timeout on the groundstation side.
 */
void MavLinkTransmitAllParameters(void);

#endif // MAVLINK_GLUE_H
