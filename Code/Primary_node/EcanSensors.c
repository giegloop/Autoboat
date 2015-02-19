#include "EcanSensors.h"

#include "Ecan1.h"
#include "Nmea2000.h"
#include "Types.h"
#include "Rudder.h"
#include "CanMessages.h"
#include "Node.h"
#include "Acs300.h"
#include "Packing.h"
#include "PrimaryNode.h"

#include <string.h>

// Declare some macros for helping setting bit values
#define ON  1
#define OFF 0

/**
 * Check the current values of the 'state' timeout counter for the given sensor and update the sensor's
 * state accordingly. This is merely a helper macro for SENSOR_STATE_UPDATE.
 * @param sensor Should be one of the available sensors in the sensor
 * @param state Should be either active or enabled.
 */
#define SENSOR_STATE_UPDATE_STATE(sensor, state)                                                                   \
    if (sensorAvailability.sensor.state) {\
        if (sensorAvailability.sensor.state ## _counter < SENSOR_TIMEOUT) {        \
            ++sensorAvailability.sensor.state ## _counter;\
        } else {\
            sensorAvailability.sensor.state = false;                                                                   \
        }\
    } else if (!sensorAvailability.sensor.state && sensorAvailability.sensor.state ## _counter < SENSOR_TIMEOUT) { \
        sensorAvailability.sensor.state = true;                                                                    \
    }

/**
 * This macro update both the 'enabled' and 'active' state for a sensor
 */
#define SENSOR_STATE_UPDATE(sensor) \
    SENSOR_STATE_UPDATE_STATE(sensor, enabled); \
    SENSOR_STATE_UPDATE_STATE(sensor, active);

struct PowerData powerDataStore = {0};
struct WindData windDataStore = {0};
struct AirData airDataStore = {0};
struct WaterData waterDataStore = {0};
struct ThrottleData throttleDataStore = {0};
GpsData gpsDataStore = {0};
struct GpsDataBundle gpsNewDataStore = {0};
struct DateTimeData dateTimeDataStore = {// Initialize our system clock to clearly invalid values.
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    false
};
struct RevoGsData revoGsDataStore = {0};
TokimecOutput tokimecDataStore = {};
struct NodeStatusData nodeStatusDataStore[NUM_NODES] = {
    {INT8_MAX, UINT8_MAX, UINT8_MAX, UINT16_MAX, UINT16_MAX},
    {INT8_MAX, UINT8_MAX, UINT8_MAX, UINT16_MAX, UINT16_MAX},
    {INT8_MAX, UINT8_MAX, UINT8_MAX, UINT16_MAX, UINT16_MAX},
    {INT8_MAX, UINT8_MAX, UINT8_MAX, UINT16_MAX, UINT16_MAX},
    {INT8_MAX, UINT8_MAX, UINT8_MAX, UINT16_MAX, UINT16_MAX},
    {INT8_MAX, UINT8_MAX, UINT8_MAX, UINT16_MAX, UINT16_MAX},
    {INT8_MAX, UINT8_MAX, UINT8_MAX, UINT16_MAX, UINT16_MAX}
};
uint8_t nodeStatusTimeoutCounters[NUM_NODES] = {
    NODE_TIMEOUT,
    NODE_TIMEOUT,
    NODE_TIMEOUT,
    NODE_TIMEOUT,
    NODE_TIMEOUT,
    NODE_TIMEOUT,
    NODE_TIMEOUT
};
struct GyroData gyroDataStore = {0};

// At startup assume all sensors are disconnected.
struct stc sensorAvailability = {
    {1, SENSOR_TIMEOUT, 1, SENSOR_TIMEOUT},
    {1, SENSOR_TIMEOUT, 1, SENSOR_TIMEOUT},
    {1, SENSOR_TIMEOUT, 1, SENSOR_TIMEOUT},
    {1, SENSOR_TIMEOUT, 1, SENSOR_TIMEOUT},
    {1, SENSOR_TIMEOUT, 1, SENSOR_TIMEOUT},
    {1, SENSOR_TIMEOUT, 1, SENSOR_TIMEOUT},
    {1, SENSOR_TIMEOUT, 1, SENSOR_TIMEOUT},
    {1, SENSOR_TIMEOUT, 1, SENSOR_TIMEOUT},
    {1, SENSOR_TIMEOUT, 1, SENSOR_TIMEOUT}
};

float GetWaterSpeed(void)
{
    waterDataStore.newData = false;
    return waterDataStore.speed;
}

float GetPropSpeed(void)
{
    throttleDataStore.newData = false;
    return throttleDataStore.rpm;
}

void GetGpsData(GpsData *data)
{
    *data = gpsDataStore;
    gpsDataStore.newData = false;
}

void ClearGpsData(void)
{
    gpsDataStore.latitude = 0.0;
    gpsDataStore.longitude = 0.0;
    gpsDataStore.altitude = 0.0;
    gpsDataStore.cog = 0;
    gpsDataStore.sog = 0;
    gpsDataStore.newData = 0;
}

uint8_t ProcessAllEcanMessages(void)
{
    uint8_t messagesLeft = 0;
    CanMessage msg;
    uint32_t pgn;

    uint8_t messagesHandled = 0;

    do {
        int foundOne = Ecan1Receive(&msg, &messagesLeft);
        if (foundOne) {
            // Process non-NMEA2000 messages here. They're distinguished by having standard frames.
            if (msg.frame_type == CAN_FRAME_STD) {
                if (msg.id == ACS300_CAN_ID_HRTBT) { // From the ACS300
                    sensorAvailability.prop.enabled_counter = 0;
                    if ((msg.payload[6] & 0x40) == 0) { // Checks the status bit to determine if the ACS300 is enabled.
                        sensorAvailability.prop.active_counter = 0;
                    }
                    Acs300DecodeHeartbeat(msg.payload, (uint16_t*) & throttleDataStore.rpm, NULL, NULL, NULL);
                    throttleDataStore.newData = true;
                } else if (msg.id == ACS300_CAN_ID_WR_PARAM) {
                    // Track the current velocity from the secondary controller.
                    uint16_t address;

                    union {
                        uint16_t param_u16;
                        int16_t param_i16;
                    } value;
                    Acs300DecodeWriteParam(msg.payload, &address, &value.param_u16);
                    if (address == ACS300_PARAM_CC) {
                        currentCommands.secondaryManualThrottleCommand = value.param_i16;
                    }
                } else if (msg.id == CAN_MSG_ID_STATUS) {
                    uint8_t node, cpuLoad, voltage;
                    int8_t temp;
                    uint16_t status, errors;
                    CanMessageDecodeStatus(&msg, &node, &cpuLoad, &temp, &voltage, &status, &errors);

                    // If we've found a valid node, store the data for it.
                    if (node > 0 && node <= NUM_NODES) {
                        // Update all of the data broadcast by this node.
                        nodeStatusDataStore[node - 1].load = cpuLoad;
                        nodeStatusDataStore[node - 1].temp = temp;
                        nodeStatusDataStore[node - 1].voltage = voltage;
                        nodeStatusDataStore[node - 1].status = status;
                        nodeStatusDataStore[node - 1].errors = errors;

                        // And reset the timeout counter for this node.
                        nodeStatusTimeoutCounters[node - 1] = 0;

                        // And add some extra logic for integrating the RC node statusAvailability
                        // logic.
                        if (node == CAN_NODE_RC) {
                            sensorAvailability.rcNode.enabled_counter = 0;
                            // Only if the RC transmitter is connected and in override mode should the RC node be considered
                            // active.
                            if (status & 0x01) {
                                sensorAvailability.rcNode.active_counter = 0;
                            }
                        }
                    }
                } else if (msg.id == CAN_MSG_ID_RUDDER_DETAILS) {
                    sensorAvailability.rudder.enabled_counter = 0;
                    CanMessageDecodeRudderDetails(&msg,
                            &rudderSensorData.RudderPotValue,
                            &rudderSensorData.RudderPotLimitStarboard,
                            &rudderSensorData.RudderPotLimitPort,
                            &rudderSensorData.LimitHitPort,
                            &rudderSensorData.LimitHitStarboard,
                            &rudderSensorData.Enabled,
                            &rudderSensorData.Calibrated,
                            &rudderSensorData.Calibrating);
                    if (rudderSensorData.Enabled &&
                            rudderSensorData.Calibrated &&
                            !rudderSensorData.Calibrating) {
                        sensorAvailability.rudder.active_counter = 0;
                    }
                } else if (msg.id == CAN_MSG_ID_IMU_DATA) {
                    sensorAvailability.imu.enabled_counter = 0;
                    sensorAvailability.imu.active_counter = 0;
                    CanMessageDecodeImuData(&msg,
                            &tokimecDataStore.yaw,
                            &tokimecDataStore.pitch,
                            &tokimecDataStore.roll);
                } else if (msg.id == CAN_MSG_ID_GYRO_DATA) {
                    sensorAvailability.gyro.enabled_counter = 0;
                    sensorAvailability.gyro.active_counter = 0;
                    CanMessageDecodeGyroData(&msg, &gyroDataStore.zRate);
                } else if (msg.id == CAN_MSG_ID_ANG_VEL_DATA) {
                    sensorAvailability.imu.enabled_counter = 0;
                    sensorAvailability.imu.active_counter = 0;
                    CanMessageDecodeAngularVelocityData(&msg,
                            &tokimecDataStore.x_angle_vel,
                            &tokimecDataStore.y_angle_vel,
                            &tokimecDataStore.z_angle_vel);
                } else if (msg.id == CAN_MSG_ID_ACCEL_DATA) {
                    sensorAvailability.imu.enabled_counter = 0;
                    sensorAvailability.imu.active_counter = 0;
                    CanMessageDecodeAccelerationData(&msg,
                            &tokimecDataStore.x_accel,
                            &tokimecDataStore.y_accel,
                            &tokimecDataStore.z_accel);
                } else if (msg.id == CAN_MSG_ID_GPS_POS_DATA) {
                    sensorAvailability.imu.enabled_counter = 0;
                    sensorAvailability.imu.active_counter = 0;
                    CanMessageDecodeGpsPosData(&msg,
                            &tokimecDataStore.latitude,
                            &tokimecDataStore.longitude);
                } else if (msg.id == CAN_MSG_ID_GPS_EST_POS_DATA) {
                    sensorAvailability.imu.enabled_counter = 0;
                    sensorAvailability.imu.active_counter = 0;
                    CanMessageDecodeGpsPosData(&msg,
                            &tokimecDataStore.est_latitude,
                            &tokimecDataStore.est_longitude);
                } else if (msg.id == CAN_MSG_ID_GPS_VEL_DATA) {
                    sensorAvailability.imu.enabled_counter = 0;
                    sensorAvailability.imu.active_counter = 0;
                    CanMessageDecodeGpsVelData(&msg,
                            &tokimecDataStore.gpsDirection,
                            &tokimecDataStore.gpsSpeed,
                            &tokimecDataStore.magneticBearing,
                            &tokimecDataStore.status);
                }
            } else {
                pgn = Iso11783Decode(msg.id, NULL, NULL, NULL);
                switch (pgn) {
                case PGN_SYSTEM_TIME:
                { // From GPS
                    sensorAvailability.gps.enabled_counter = 0;
                    uint8_t rv = ParsePgn126992(msg.payload, NULL, NULL, &dateTimeDataStore.year, &dateTimeDataStore.month, &dateTimeDataStore.day, &dateTimeDataStore.hour, &dateTimeDataStore.min, &dateTimeDataStore.sec, &dateTimeDataStore.usecSinceEpoch);
                    // Check if all 6 parts of the datetime were successfully decoded before triggering an update
                    if ((rv & 0xFC) == 0xFC) {
                        sensorAvailability.gps.active_counter = 0;
                        dateTimeDataStore.newData = true;
                    }
                }
                break;
                case PGN_RUDDER:
                { // From the Rudder Controller
                    // Track rudder messages that are either rudder commands OR actual rudder angles. Since the Parse* function only
                    // stores valid data, we can just pass in both variables to be written to.
                    ParsePgn127245(msg.payload, NULL, NULL, &currentCommands.secondaryManualRudderCommand, &rudderSensorData.RudderAngle);
                }
                break;
                case PGN_BATTERY_STATUS:
                { // From the Power Node
                    sensorAvailability.power.enabled_counter = 0;
                    uint8_t rv = ParsePgn127508(msg.payload, NULL, NULL, &powerDataStore.voltage, &powerDataStore.current, &powerDataStore.temperature);
                    if ((rv & 0x0C) == 0xC) {
                        sensorAvailability.power.active_counter = 0;
                        powerDataStore.newData = true;
                    }
                }
                break;
                case PGN_SPEED: // From the DST800
                    sensorAvailability.dst800.enabled_counter = 0;
                    if (ParsePgn128259(msg.payload, NULL, &waterDataStore.speed)) {
                        sensorAvailability.dst800.active_counter = 0;
                        waterDataStore.newData = true;
                    }
                    break;
                case PGN_WATER_DEPTH:
                { // From the DST800
                    sensorAvailability.dst800.enabled_counter = 0;
                    // Only update the data in waterDataStore if an actual depth was returned.
                    uint8_t rv = ParsePgn128267(msg.payload, NULL, &waterDataStore.depth, NULL);
                    if ((rv & 0x02) == 0x02) {
                        sensorAvailability.dst800.active_counter = 0;
                        waterDataStore.newData = true;
                    }
                }
                break;
                case PGN_POSITION_RAP_UPD:
                { // From the GPS200
                    sensorAvailability.gps.enabled_counter = 0;
                    uint8_t rv = ParsePgn129025(msg.payload, &gpsNewDataStore.lat, &gpsNewDataStore.lon);

                    // Only do something if both latitude and longitude were parsed successfully.
                    if ((rv & 0x03) == 0x03) {
                        // If there was already position data in there, assume this is the start of a new clustering and clear out old data.
                        if (gpsNewDataStore.receivedMessages & GPSDATA_POSITION) {
                            gpsNewDataStore.receivedMessages = GPSDATA_POSITION;
                        }                            // Otherwise we...
                        else {
                            // Record that a position message was received.
                            gpsNewDataStore.receivedMessages |= GPSDATA_POSITION;

                            // And if it was the last message in this bundle, check that it's valid,
                            // and copy it over to the reading struct setting it as new data.
                            if (gpsNewDataStore.receivedMessages == GPSDATA_ALL) {
                                // Validity is checked by verifying that this had a valid 2D/3D fix,
                                // and lat/lon is not 0,
                                if ((gpsNewDataStore.mode == PGN_129539_MODE_2D || gpsNewDataStore.mode == PGN_129539_MODE_3D) &&
                                        (gpsNewDataStore.lat != 0 || gpsNewDataStore.lon != 0)) {

                                    // Copy the entire struct over and then overwrite the newData
                                    // field with a valid "true" value.
                                    memcpy(&gpsDataStore, &gpsNewDataStore, sizeof (gpsNewDataStore));
                                    gpsDataStore.newData = true;

                                    // Also set our GPS as receiving good data.
                                    sensorAvailability.gps.active_counter = 0;
                                }

                                // Regardless of whether this data was useful, we clear for next bundle.
                                gpsNewDataStore.receivedMessages = GPSDATA_NONE;
                            }
                        }
                    }
                }
                break;
                case PGN_COG_SOG_RAP_UPD:
                { // From the GPS200
                    sensorAvailability.gps.enabled_counter = 0;
                    uint8_t rv = ParsePgn129026(msg.payload, NULL, NULL, &gpsNewDataStore.cog, &gpsNewDataStore.sog);

                    // Only update if both course-over-ground and speed-over-ground were parsed successfully.
                    if ((rv & 0x0C) == 0x0C) {
                        // If there was already heading data in there, assume this is the start of a new clustering and clear out old data.
                        if (gpsNewDataStore.receivedMessages & GPSDATA_HEADING) {
                            gpsNewDataStore.receivedMessages = GPSDATA_HEADING;
                        }                            // Otherwise we...
                        else {
                            // Record that a position message was received.
                            gpsNewDataStore.receivedMessages |= GPSDATA_HEADING;

                            // And if it was the last message in this bundle, check that it's valid,
                            // and copy it over to the reading struct setting it as new data.
                            if (gpsNewDataStore.receivedMessages == GPSDATA_ALL) {
                                // Validity is checked by verifying that this had a valid 2D/3D fix,
                                // and lat/lon is not 0,
                                if ((gpsNewDataStore.mode == PGN_129539_MODE_2D || gpsNewDataStore.mode == PGN_129539_MODE_3D) &&
                                        (gpsNewDataStore.lat != 0 || gpsNewDataStore.lon != 0)) {

                                    // Copy the entire struct over and then overwrite the newData
                                    // field with a valid "true" value.
                                    memcpy(&gpsDataStore, &gpsNewDataStore, sizeof (gpsNewDataStore));
                                    gpsDataStore.newData = true;

                                    // Also set our GPS as receiving good data.
                                    sensorAvailability.gps.active_counter = 0;
                                }

                                // Regardless of whether this data was useful, we clear for next bundle.
                                gpsNewDataStore.receivedMessages = GPSDATA_NONE;
                            }
                        }
                    }
                }
                break;
                case PGN_GNSS_DOPS:
                { // From the GPS200
                    sensorAvailability.gps.enabled_counter = 0;
                    uint8_t rv = ParsePgn129539(msg.payload, NULL, NULL, &gpsNewDataStore.mode, &gpsNewDataStore.hdop, &gpsNewDataStore.vdop, NULL);

                    if ((rv & 0x1C) == 0x1C) {
                        // If there was already heading data in there, assume this is the start of a new clustering and clear out old data.
                        if (gpsNewDataStore.receivedMessages & GPSDATA_FIX) {
                            gpsNewDataStore.receivedMessages = GPSDATA_FIX;
                        }                            // Otherwise we...
                        else {
                            // Record that a position message was received.
                            gpsNewDataStore.receivedMessages |= GPSDATA_FIX;

                            // And if it was the last message in this bundle, check that it's valid,
                            // and copy it over to the reading struct setting it as new data.
                            if (gpsNewDataStore.receivedMessages == GPSDATA_ALL) {
                                // Validity is checked by verifying that this had a valid 2D/3D fix,
                                // and lat/lon is not 0,
                                if ((gpsNewDataStore.mode == PGN_129539_MODE_2D || gpsNewDataStore.mode == PGN_129539_MODE_3D) &&
                                        (gpsNewDataStore.lat != 0 || gpsNewDataStore.lon != 0)) {

                                    // Copy the entire struct over and then overwrite the newData
                                    // field with a valid "true" value.
                                    memcpy(&gpsDataStore, &gpsNewDataStore, sizeof (gpsNewDataStore));
                                    gpsDataStore.newData = true;
                                    // Also set our GPS as receiving good data.
                                    sensorAvailability.gps.active_counter = 0;
                                }

                                // Regardless of whether this data was useful, we clear for next bundle.
                                gpsNewDataStore.receivedMessages = GPSDATA_NONE;
                            }
                        }
                    }
                }
                break;
                case PGN_MAG_VARIATION: // From the GPS200
                    ParsePgn127258(msg.payload, NULL, NULL, NULL, &gpsDataStore.variation);
                    break;
                case PGN_WIND_DATA: // From the WSO100
                    sensorAvailability.wso100.enabled_counter = 0;
                    if (ParsePgn130306(msg.payload, NULL, &windDataStore.speed, &windDataStore.direction)) {
                        sensorAvailability.wso100.active_counter = 0;
                        windDataStore.newData = true;
                    }
                    break;
                case PGN_ENV_PARAMETERS: // From the DST800
                    sensorAvailability.dst800.enabled_counter = 0;
                    if (ParsePgn130310(msg.payload, NULL, &waterDataStore.temp, NULL, NULL)) {
                        // The DST800 is only considered active when a water depth is received
                        waterDataStore.newData = true;
                    }
                    break;
                case PGN_ENV_PARAMETERS2: // From the WSO100
                    sensorAvailability.wso100.enabled_counter = 0;
                    if (ParsePgn130311(msg.payload, NULL, NULL, NULL, &airDataStore.temp, &airDataStore.humidity, &airDataStore.pressure)) {
                        sensorAvailability.wso100.active_counter = 0;
                        airDataStore.newData = true;
                    }
                    break;
                }
            }

            ++messagesHandled;
        }
    } while (messagesLeft > 0);

    return messagesHandled;
}

/**
 * This function should be called at a constant rate (same units as SENSOR_TIMEOUT) and updates the
 * availability of any sensors and onboard nodes. This function is separated from the
 * `ProcessAllEcanMessages()` function because that function should be called as fast as possible,
 * while this one should be called at the base tick rate of the system.
 */
void UpdateSensorsAvailability(void)
{
    // Now if any nodes have timed out, reset their struct data since any data we have for them is
    // now invalid. Otherwise, keep incrementing their timeout counters. These are reset in
    // `ProcessAllEcanMessages()`.
    int i;
    for (i = 0; i < NUM_NODES; ++i) {
        // Be sure to not do this for the current node, as it won't ever receive CAN messages from
        // itself.
        if (i != nodeId - 1) {
            if (nodeStatusTimeoutCounters[i] < NODE_TIMEOUT) {
                ++nodeStatusTimeoutCounters[i];
            } else {
                nodeStatusDataStore[i].errors = UINT16_MAX;
                nodeStatusDataStore[i].load = UINT8_MAX;
                nodeStatusDataStore[i].status = UINT16_MAX;
                nodeStatusDataStore[i].temp = INT8_MAX;
                nodeStatusDataStore[i].voltage = UINT8_MAX;
            }
        }
    }

    // Now update the '.enabled' or '.active' status for every sensor. We keep timeout counters that
    // timeout a sensor after SENSOR_TIMEOUT amount of time, which depends on how often this function
    // is called.
    SENSOR_STATE_UPDATE(gps);
    SENSOR_STATE_UPDATE(imu);
    SENSOR_STATE_UPDATE(wso100);
    SENSOR_STATE_UPDATE(dst800);
    SENSOR_STATE_UPDATE(power);
    SENSOR_STATE_UPDATE(prop);
    SENSOR_STATE_UPDATE(rudder);
    SENSOR_STATE_UPDATE(rcNode);
    SENSOR_STATE_UPDATE(gyro);
    SENSOR_STATE_UPDATE(gps);
}
