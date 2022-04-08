#include "pathStateClasses.hpp"


/***********************************************************************************************************************
 * FIXED WING
 **********************************************************************************************************************/


#if IS_FIXED_WING



/***********************************************************************************************************************
 * Static Member Variable Declarations
 **********************************************************************************************************************/
Telemetry_PIGO_t commsWithTelemetry::_incomingData;
_CruisingState_Telemetry_Return cruisingState::_returnToGround;
_WaypointManager_Data_In cruisingState::_inputdata;
_WaypointManager_Data_Out cruisingState::_outputdata;
SFOutput_t sensorFusion::_sfOutputData;
IMU_Data_t sensorFusion::_imudata;
CoordinatedTurnAttitudeManagerCommands_t coordinateTurnElevation::_rollandrudder;
AltitudeAirspeedCommands_t coordinateTurnElevation::_pitchandairspeed;
AttitudeData commsWithAttitude::_receivedData;

// Landing and Takeoff Variables (some stages DO NOT need waypoint inputs, it's not a mistake that some are missing)
// For landing and takeoff states that use waypoint manager functions, the variables below will hold the I/O that is needed for those functions to work

_LandingTakeoffInput takeoffRollStage::input;
_LandingTakeoffOutput takeoffRollStage::output;
_WaypointManager_Data_Out takeoffRollStage::waypointOutput;
_PathData * takeoffRollStage::currentLocation;
_PathData * takeoffRollStage::pathArray[1];
WaypointManager takeoffRollStage::takeoffPath;
_PathData takeoffRollStage::takeoffPoint;
_WaypointStatus takeoffRollStage::waypointStatus;

_LandingTakeoffInput takeoffClimbStage::input;
_LandingTakeoffOutput takeoffClimbStage::output;
_WaypointManager_Data_In takeoffClimbStage::waypointInput;
_WaypointManager_Data_Out takeoffClimbStage::waypointOutput;

_LandingTakeoffInput landingTransitionStage::input;
_LandingTakeoffOutput landingTransitionStage::output;
_WaypointManager_Data_In landingTransitionStage::waypointInput;
_WaypointManager_Data_Out landingTransitionStage::waypointOutput;
_WaypointStatus landingTransitionStage::waypointStatus;
_PathData * landingTransitionStage::currentLocation;
_PathData * landingTransitionStage::pathArray[3];
WaypointManager landingTransitionStage::landingPath;
_LandingPath landingTransitionStage::path;

_LandingTakeoffInput landingSlopeStage::input;
_LandingTakeoffOutput landingSlopeStage::output;
_WaypointManager_Data_In landingSlopeStage::waypointInput;
_WaypointManager_Data_Out landingSlopeStage::waypointOutput;

_LandingTakeoffInput landingFlareStage::input;
_LandingTakeoffOutput landingFlareStage::output;
_WaypointManager_Data_In landingFlareStage::waypointInput;
_WaypointManager_Data_Out landingFlareStage::waypointOutput;

_LandingTakeoffInput landingDecrabStage::input;
_LandingTakeoffOutput landingDecrabStage::output;
_WaypointManager_Data_Out landingDecrabStage::waypointOutput;

_LandingTakeoffInput landingTouchdownStage::input;
_LandingTakeoffOutput landingTouchdownStage::output;
_WaypointManager_Data_Out landingTouchdownStage::waypointOutput;


/***********************************************************************************************************************
 * Code
 **********************************************************************************************************************/

void commsWithAttitude::execute(pathManager* pathMgr)
{

    #if IS_FIXED_WING
    bool newDataAvailable = GetFromAMToPM(&_receivedData); // Gets attitude manager data
    #endif

    // Gets data used to populate CommandsForAM struct
    CoordinatedTurnAttitudeManagerCommands_t * turnCommands = coordinateTurnElevation::GetRollAndRudder();
    AltitudeAirspeedCommands_t * altCommands = coordinateTurnElevation::GetPitchAndAirspeed();

    CommandsForAM toSend {};
    toSend.roll = turnCommands->requiredRoll;
    toSend.pitch = altCommands->requiredPitch;
    toSend.rudderPercent = turnCommands->requiredRudderPosition;
    toSend.throttlePercent = altCommands->requiredThrottlePercent;

    SendFromPMToAM(&toSend); // Sends commands to attitude manager

    pathMgr->setState(commsWithTelemetry::getInstance());
}

pathManagerState& commsWithAttitude::getInstance()
{
    static commsWithAttitude singleton;
    return singleton;
}

void commsWithTelemetry::execute(pathManager* pathMgr)
{
    GetTelemetryCommands(&_incomingData);

    #ifndef SIMULATION
    setPitchPercent(_incomingData.gimbalPitch);
    setYawPercent(_incomingData.gimbalYaw);
    #endif

    if(pathMgr->isError)
    {
        pathMgr->setState(fatalFailureMode::getInstance());
    }
    else
    {
        pathMgr->setState(sensorFusion::getInstance());
    }
}

pathManagerState& commsWithTelemetry::getInstance()
{
    static commsWithTelemetry singleton;
    return singleton;
}

void sensorFusion::execute(pathManager* pathMgr)
{
    SFError_t error = SF_GetResult(&_sfOutputData); // Gets current Sensor fusion output struct
    _imudata = SF_GetRawIMU();

    if(pathMgr->isError)
    {
        pathMgr->setState(fatalFailureMode::getInstance());
    }
    else
    {
        pathMgr->setState(resetVariables::getInstance());
    }
}

pathManagerState& sensorFusion::getInstance()
{
    static sensorFusion singleton;
    return singleton;
}

void resetVariables::execute(pathManager* pathMgr)
{
    //resetting the variables for passby
    if(pathMgr->isError)
    {
        pathMgr->setState(fatalFailureMode::getInstance());
    }

    //if begin landing or begin takeoff is true, this state will bypass the enum and start the plane at roll/transition
    if(commsWithTelemetry::GetTelemetryIncomingData()->beginLanding)
    {
        resetPassby(&landingTransitionStage::getControlOutput()->controlDetails);
        pathMgr->setState(landingTransitionStage::getInstance());
        return;
    }
    if(commsWithTelemetry::GetTelemetryIncomingData()->beginTakeoff)
    {
        resetPassby(&takeoffRollStage::getControlOutput()->controlDetails);
        pathMgr->setState(takeoffRollStage::getInstance());
        return;
    }

    //if the enums for landing state, set to each landing state
    switch(pathMgr->stage){
        case TRANSITION:
            resetPassby(&landingTransitionStage::getControlOutput()->controlDetails);
            pathMgr->setState(landingTransitionStage::getInstance());
            break;
        case SLOPE:
            resetPassby(&landingSlopeStage::getControlOutput()->controlDetails);
            pathMgr->setState(landingSlopeStage::getInstance());
            break;
        case FLARE:
            resetPassby(&landingFlareStage::getControlOutput()->controlDetails);
            pathMgr->setState(landingFlareStage::getInstance());
            break;
        case DECRAB:
            resetPassby(&landingDecrabStage::getControlOutput()->controlDetails);
            pathMgr->setState(landingDecrabStage::getInstance());
            break;
        case TOUCHDOWN:
            resetPassby(&landingTouchdownStage::getControlOutput()->controlDetails);
            pathMgr->setState(landingTouchdownStage::getInstance());
            break;
        case CRUISING:
            //no control details need to be reset for cruising state
            pathMgr->setState(cruisingState::getInstance());
            break;
        case ROLL:
            resetPassby(&takeoffRollStage::getControlOutput()->controlDetails);
            pathMgr->setState(takeoffRollStage::getInstance());
            break;
        case CLIMB:
            resetPassby(&takeoffClimbStage::getControlOutput()->controlDetails);
            pathMgr->setState(takeoffClimbStage::getInstance());
            break;
        default:
            pathMgr->setState(cruisingState::getInstance());
    }
}

pathManagerState& resetVariables::getInstance()
{
    static resetVariables singleton;
    return singleton;
}

void cruisingState::execute(pathManager* pathMgr)
{

    Telemetry_PIGO_t * telemetryData = commsWithTelemetry::GetTelemetryIncomingData(); // Get struct from telemetry state with all of the commands and values.
    SFOutput_t * sensFusionOutput = sensorFusion::GetSFOutput(); // Get sensor fusion data

    // Set waypoint manager input struct
    _inputdata.track = sensFusionOutput->track; // Gets track
    _inputdata.longitude = sensFusionOutput->longitude;
    _inputdata.latitude = sensFusionOutput->latitude;
    _inputdata.altitude = sensFusionOutput->altitude;

    // Call module functions
    _ModifyFlightPathErrorCode editError = editFlightPath(telemetryData, cruisingStateManager, waypointIDArray); // Edit flight path if applicable
    _GetNextDirectionsErrorCode pathError = pathFollow(telemetryData, cruisingStateManager, _inputdata, &_outputdata, goingHome, inHold); // Get next direction or modify flight behaviour pattern
    setReturnValues(&_returnToGround, cruisingStateManager, editError, pathError); // Set error codes


    if(pathMgr->isError)
    {
        pathMgr->setState(fatalFailureMode::getInstance());
    }
    else
    {
        pathMgr->setState(coordinateTurnElevation::getInstance());
    }
}

pathManagerState& cruisingState::getInstance()
{
    static cruisingState singleton;
    return singleton;
}


void coordinateTurnElevation::execute(pathManager* pathMgr)
{

    SFOutput_t * sensFusionOutput = sensorFusion::GetSFOutput(); // Get sensor fusion data
    IMU_Data_t * imudata = sensorFusion::GetIMUData(); // Gets raw IMU data
    _WaypointManager_Data_Out * waypointOutput = cruisingState::GetOutputData(); // Get output data from waypoint manager

    CoordinatedTurnInput_t turnInput {};
    AltitudeAirspeedInput_t altAirspeedInput {};
    
    _LandingTakeoffOutput outputHolder; //had a bug with initializing the passby details, defining the pointer like this fixed it
    _LandingTakeoffOutput * landingTakeoffOutput = &outputHolder;
    //get elevation and turning data
    //loading in commands data from each of the states
    switch(pathMgr->stage){
        case ROLL:
            landingTakeoffOutput = takeoffRollStage::getControlOutput();
            break;
        case CLIMB:
            landingTakeoffOutput = takeoffClimbStage::getControlOutput();
            break;
        case TRANSITION:
            landingTakeoffOutput = landingTransitionStage::getControlOutput();
            break;
        case SLOPE:
            landingTakeoffOutput = landingSlopeStage::getControlOutput();
            break;
        case FLARE:
            landingTakeoffOutput = landingFlareStage::getControlOutput();
            break;
        case DECRAB:
            landingTakeoffOutput = landingDecrabStage::getControlOutput();
            break;
        case TOUCHDOWN:
            landingTakeoffOutput = landingTouchdownStage::getControlOutput();
            break;
        case CRUISING:
            turnInput.currentHeadingTrack = sensFusionOutput->track; // Gets track;
            turnInput.desiredHeadingTrack = waypointOutput->desiredTrack;
            turnInput.accY = imudata->accy;

            // Set up the compute altitude and airspeed function input
            altAirspeedInput.currentAltitude = sensFusionOutput->altitude;
            altAirspeedInput.desiredAltitude = waypointOutput->desiredAltitude;
            altAirspeedInput.currentAirspeed = sensFusionOutput->airspeed;
            altAirspeedInput.desiredAirspeed = waypointOutput->desiredAirspeed;
            break;
        default:
            //should default to cruising

            turnInput.currentHeadingTrack = sensFusionOutput->track; // Gets track;
            turnInput.desiredHeadingTrack = waypointOutput->desiredTrack;
            turnInput.accY = imudata->accy;

            // Set up the compute altitude and airspeed function input
            altAirspeedInput.currentAltitude = sensFusionOutput->altitude;
            altAirspeedInput.desiredAltitude = waypointOutput->desiredAltitude;
            altAirspeedInput.currentAirspeed = sensFusionOutput->airspeed;
            altAirspeedInput.desiredAirspeed = waypointOutput->desiredAirspeed;

    }

    if(pathMgr->stage!=CRUISING)
    {
        LandingTakeoffManager::translateLTSFCommandsToCoordTurns(*landingTakeoffOutput, *sensFusionOutput, *imudata, turnInput, altAirspeedInput);
    }

    // Call module functions
    AutoSteer_ComputeCoordinatedTurn(&turnInput, &_rollandrudder);
    AutoSteer_ComputeAltitudeAndAirspeed(&altAirspeedInput, &_pitchandairspeed);

    //passby functionality
    
    if(landingTakeoffOutput->controlDetails.pitchPassby)
    {
        _pitchandairspeed.requiredPitch = landingTakeoffOutput->controlDetails.pitchPercent;
    }

    if(landingTakeoffOutput->controlDetails.throttlePassby)
    {
        _pitchandairspeed.requiredThrottlePercent = landingTakeoffOutput->controlDetails.throttlePercent;
    }

    if(landingTakeoffOutput->controlDetails.rollPassby)
    {
        _rollandrudder.requiredRoll = landingTakeoffOutput->controlDetails.rollPercent;
    }

    if(landingTakeoffOutput->controlDetails.rudderPassby)
    {
        _rollandrudder.requiredRudderPosition = landingTakeoffOutput->controlDetails.rudderPercent;
    }
    
    if(pathMgr->isError)
    {
        pathMgr->setState(fatalFailureMode::getInstance());
    }
    else
    {
        pathMgr->setState(commsWithAttitude::getInstance());
    }
}

pathManagerState& coordinateTurnElevation::getInstance()
{
    static coordinateTurnElevation singleton;
    return singleton;
}

void fatalFailureMode::execute(pathManager* pathMgr)
{
    pathMgr->setState(fatalFailureMode::getInstance());
}

pathManagerState& fatalFailureMode::getInstance()
{
    static fatalFailureMode singleton;
    return singleton;
}

/****************************************************************************************************
LANDING STATE FUNCTIONS
****************************************************************************************************/

void landingTransitionStage::execute(pathManager* pathMgr)
{
    pathMgr->stage = TRANSITION;
    //load in sensor fusion data and telemtry data into input structure
    input.telemetryData = commsWithTelemetry::GetTelemetryIncomingData();
    input.sensorOutput = sensorFusion::GetSFOutput();

    //making sure landing points are only made once
    if(!pathMgr->madeLandingPoints)
    {
        //requires data structure that dhruv wants to use
        path = LandingTakeoffManager::createSlopeWaypoints(*input.telemetryData, input.sensorOutput->altitude);

        //creating waypoints

        pathArray[0] = landingPath.initialize_waypoint(input.sensorOutput->longitude, input.sensorOutput->latitude, input.sensorOutput->altitude, PATH_FOLLOW);
        pathArray[1] = landingPath.initialize_waypoint(path.intersectionPoint.longitude, path.intersectionPoint.latitude, path.intersectionPoint.altitude, PATH_FOLLOW);
        pathArray[2] = landingPath.initialize_waypoint(path.aimingPoint.longitude, path.aimingPoint.latitude, path.aimingPoint.altitude, PATH_FOLLOW);
        pathArray[3] = landingPath.initialize_waypoint(path.stoppingPoint.longitude, path.stoppingPoint.latitude, path.stoppingPoint.altitude, PATH_FOLLOW);
        currentLocation = landingPath.initialize_waypoint(input.sensorOutput->longitude, input.sensorOutput->latitude, input.sensorOutput->altitude, HOLD_WAYPOINT, 20); 

        //initializing flight path
        waypointStatus = landingPath.initialize_flight_path(pathArray, 4, currentLocation);

        //set made madelandingPoints to true
        pathMgr->madeLandingPoints = true;
    }

    //translate sensor data to waypoint data in struct
    waypointInput.latitude = input.sensorOutput->latitude;
    waypointInput.longitude = input.sensorOutput->longitude;
    waypointInput.altitude = input.sensorOutput->altitude;
    waypointInput.track = input.sensorOutput->track;

    //follow the landing waypoints
    waypointStatus = landingPath.get_next_directions(waypointInput, &waypointOutput);

    //translate waypoint commands into landing output structure
    output = LandingTakeoffManager::translateWaypointCommands(waypointOutput);

    //maintaining altitude
    output.desiredAirspeed = CRUISING_AIRSPEED;

    //calculating the difference in heading to detect if finished turning (2 differences possible)
    double differenceInHeading1 = input.telemetryData->stoppingDirectionHeading - input.sensorOutput->track;
    double differenceInHeading2 = input.sensorOutput->track - input.telemetryData->stoppingDirectionHeading;

    //making sure both headings are positive
    if(differenceInHeading1 < 0){differenceInHeading1 += 360;}
    if(differenceInHeading2 < 0){differenceInHeading2 += 360;}

    //if the smaller heading is less than 30 degrees, set stage to slope
    if((fabs(differenceInHeading1) <= 80) || (fabs(differenceInHeading2) <= 80))
    {
        pathMgr->stage = SLOPE;
    }

    if(landingTransitionStage::waypointStatus != WAYPOINT_SUCCESS)
    {
        pathMgr->isError = true;
    }

    if(pathMgr->isError)
    {
        pathMgr->setState(fatalFailureMode::getInstance());
    }
    else
    {
        pathMgr->setState(coordinateTurnElevation::getInstance());
    }
}

pathManagerState& landingTransitionStage::getInstance()
{
    static landingTransitionStage singleton;
    return singleton;
}

void landingSlopeStage::execute(pathManager* pathMgr)
{
    //load in sensor fusion data and telemtry data into input structure
    input.telemetryData = commsWithTelemetry::GetTelemetryIncomingData();
    input.sensorOutput = sensorFusion::GetSFOutput();

    if(input.sensorOutput->altitude <= (FLARE_ALTITUDE + input.telemetryData->stoppingAltitude)) //if less than flare altitude
    {
        pathMgr->stage = FLARE;

    }
    else
    {
        //setting sensorFusion input to waypoint data in
        waypointInput.latitude = input.sensorOutput->latitude;
        waypointInput.longitude = input.sensorOutput->longitude;
        waypointInput.altitude = input.sensorOutput->altitude;
        waypointInput.track = input.sensorOutput->track;

        //aligning horizontal position using waypointManager get_next_directions
        landingTransitionStage::waypointStatus = landingTransitionStage::landingPath.get_next_directions(waypointInput, &waypointOutput);

        //translate waypoint ouput data into landing data
        output = LandingTakeoffManager::translateWaypointCommands(waypointOutput);

        //retrieving desired altitude for slope state and setting it
        output.desiredAltitude = LandingTakeoffManager::changingAltitude(*input.sensorOutput, landingTransitionStage::path.aimingPoint, landingTransitionStage::path.intersectionPoint, landingTransitionStage::path.stoppingPoint);

        //retrieving desired speed for approach speed and setting it
        output.desiredAirspeed = LandingTakeoffManager::approachSpeed(&pathMgr->isPackage);
    }

    if(landingTransitionStage::waypointStatus != WAYPOINT_SUCCESS)
    {
        pathMgr->isError = true;
    }

    if(pathMgr->isError)
    {
        pathMgr->setState(fatalFailureMode::getInstance());
    }
    else
    {
        pathMgr->setState(coordinateTurnElevation::getInstance());
    }
}

pathManagerState& landingSlopeStage::getInstance()
{
    static landingSlopeStage singleton;
    return singleton;
}

void landingFlareStage::execute(pathManager* pathMgr)
{
    //load in sensor fusion data and telemtry data into input structure
    input.telemetryData = commsWithTelemetry::GetTelemetryIncomingData();
    input.sensorOutput = sensorFusion::GetSFOutput();

    if(input.sensorOutput->altitude <= (DECRAB_ALTITUDE + input.telemetryData->stoppingAltitude)) //altitude is below 70 cm
    {
        pathMgr->stage = DECRAB;
    }
    else
    {
        //setting sensorFusion input to waypoint data in
        waypointInput.latitude = input.sensorOutput->latitude;
        waypointInput.longitude = input.sensorOutput->longitude;
        waypointInput.altitude = input.sensorOutput->altitude;
        waypointInput.track = input.sensorOutput->track;

        //maintaining horizontal position
        landingTransitionStage::waypointStatus = landingTransitionStage::landingPath.get_next_directions(waypointInput, &waypointOutput);

        //translating waypoint data into landing data
        output = LandingTakeoffManager::translateWaypointCommands(waypointOutput);

        //throttle off
        output.controlDetails.throttlePassby = true;
        output.controlDetails.throttlePercent = 0;

        //maintaing speed for flare attitude
        output.desiredAirspeed = LandingTakeoffManager::slowFlightSpeed(pathMgr->isPackage);
    }

    if(landingTransitionStage::waypointStatus != WAYPOINT_SUCCESS)
    {
        pathMgr->isError = true;
    }

    if(pathMgr->isError)
    {
        pathMgr->setState(fatalFailureMode::getInstance());
    }
    else
    {
        pathMgr->setState(coordinateTurnElevation::getInstance());
    }
}

pathManagerState& landingFlareStage::getInstance()
{
    static landingFlareStage singleton;
    return singleton;
}

void landingDecrabStage::execute(pathManager* pathMgr)
{
    //load in sensor fusion data and telemtry data into input structure
    input.telemetryData = commsWithTelemetry::GetTelemetryIncomingData();
    input.sensorOutput = sensorFusion::GetSFOutput();

    if(input.sensorOutput->altitude <= (TOUCHDOWN_ALTITUDE + input.telemetryData->stoppingAltitude)) //altitude is 5 cm or less/ultrasonic sensor sensed 5cm or less
    {
        pathMgr->stage = TOUCHDOWN;
    }
    else
    {
        //align heading with landing direction
        output.desiredHeading = input.telemetryData->stoppingDirectionHeading;
        output.useHeading = true;

        //retrieving desired slow flight speed
        output.desiredAirspeed = LandingTakeoffManager::slowFlightSpeed(pathMgr->isPackage);

        //throttle off
        output.controlDetails.throttlePassby = true;
        output.controlDetails.throttlePercent = 0;
    }

    if(pathMgr->isError)
    {
        pathMgr->setState(fatalFailureMode::getInstance());
    }
    else
    {
        pathMgr->setState(coordinateTurnElevation::getInstance());
    }
}

pathManagerState& landingDecrabStage::getInstance()
{
    static landingDecrabStage singleton;
    return singleton;
}

void landingTouchdownStage::execute(pathManager* pathMgr)
{
    //load in sensor fusion data and telemtry data into input structure
    input.telemetryData = commsWithTelemetry::GetTelemetryIncomingData();
    input.sensorOutput = sensorFusion::GetSFOutput();

    //throttle off
    output.controlDetails.throttlePassby = true;
    output.controlDetails.throttlePercent = 0;

    //aligning heading
    output.desiredHeading = input.telemetryData->stoppingDirectionHeading;
    output.useHeading = true;

    //ensure made landing points is reset
    pathMgr->madeLandingPoints = false;

    if(pathMgr->isError)
    {
        pathMgr->setState(fatalFailureMode::getInstance());
    }
    else
    {
        pathMgr->setState(coordinateTurnElevation::getInstance());
    }
}

pathManagerState& landingTouchdownStage::getInstance()
{
    static landingTouchdownStage singleton;
    return singleton;
}


/****************************************************************************************************
TAKEOFF STATE FUNCTIONS
****************************************************************************************************/

void takeoffRollStage::execute(pathManager* pathMgr)
{
    pathMgr->stage = ROLL;
    //load in sensor fusion data and telemtry data into input structure
    input.telemetryData = commsWithTelemetry::GetTelemetryIncomingData();
    input.sensorOutput = sensorFusion::GetSFOutput();

    //max throttle
    output.controlDetails.throttlePassby = true;
    output.controlDetails.throttlePercent = 100;

    if(!pathMgr->madeTakeoffPoints)
    {
        takeoffPoint = LandingTakeoffManager::createTakeoffWaypoint(input.sensorOutput->latitude,input.sensorOutput->longitude, input.sensorOutput->altitude, input.telemetryData->takeoffDirectionHeading);
        pathArray[0] = takeoffPath.initialize_waypoint(takeoffPoint.longitude, takeoffPoint.latitude, takeoffPoint.altitude, PATH_FOLLOW);

        //10 meters is added to the altitude of the currentLocation waypoint so that is is not ground level
        currentLocation = takeoffPath.initialize_waypoint(input.sensorOutput->longitude, input.sensorOutput->latitude, (input.sensorOutput->altitude + 10), HOLD_WAYPOINT);
        waypointStatus = takeoffPath.initialize_flight_path(pathArray, 1, currentLocation);
        pathMgr->madeTakeoffPoints = true;
    }

    if(input.sensorOutput->airspeed > (LandingTakeoffManager::desiredRotationSpeed(pathMgr->isPackage)))
    {
        pathMgr->stage = CLIMB;
    }

    if(takeoffRollStage::waypointStatus != WAYPOINT_SUCCESS)
    {
        pathMgr->isError = true;
    }

    if(pathMgr->isError)
    {
        pathMgr->setState(fatalFailureMode::getInstance());
    }
    else
    {
        pathMgr->setState(coordinateTurnElevation::getInstance());
    }
}

pathManagerState& takeoffRollStage::getInstance()
{
    static takeoffRollStage singleton;
    return singleton;
}

void takeoffClimbStage::execute(pathManager* pathMgr)
{
    //load in sensor fusion data and telemtry data into input structure
    input.telemetryData = commsWithTelemetry::GetTelemetryIncomingData();
    input.sensorOutput = sensorFusion::GetSFOutput();

    if(input.sensorOutput->altitude > (takeoffRollStage::takeoffPoint.altitude + EXIT_TAKEOFF_ALTITUDE))
    {
        pathMgr->stage = CRUISING;
    }
    else
    {
        //setting sensorFusion input to waypoint data in
        waypointInput.latitude = input.sensorOutput->latitude;
        waypointInput.longitude = input.sensorOutput->longitude;
        waypointInput.altitude = input.sensorOutput->altitude;
        waypointInput.track = input.sensorOutput->track;

        takeoffRollStage::waypointStatus = takeoffRollStage::takeoffPath.get_next_directions(waypointInput, &waypointOutput);
        output = LandingTakeoffManager::translateWaypointCommands(waypointOutput);

        output.desiredAirspeed = LandingTakeoffManager::desiredClimbSpeed(pathMgr->isPackage);
        
        //pitching up
        output.controlDetails.pitchPassby = true;
        output.controlDetails.pitchPercent =  0.3;
    }

    //ensuring made takeoff points is reset
    pathMgr->madeTakeoffPoints = false;

    if(takeoffRollStage::waypointStatus != WAYPOINT_SUCCESS)
    {
        pathMgr->isError = true;
    }

    if(pathMgr->isError)
    {
        pathMgr->setState(fatalFailureMode::getInstance());
    }
    else
    {
        pathMgr->setState(coordinateTurnElevation::getInstance());
    }
}

pathManagerState& takeoffClimbStage::getInstance()
{
    static takeoffClimbStage singleton;
    return singleton;
}



#else 

/****************************************************************************************************
DRONE CODE
****************************************************************************************************/

/***********************************************************************************************************************
 * Static Member Variable Declarations
 **********************************************************************************************************************/


fijo commsWithTelemetry::_incomingData;
_CruisingState_Telemetry_Return cruisingState::_returnToGround;
_WaypointManager_Data_In cruisingState::_inputdata;
_WaypointManager_Data_Out cruisingState::_outputdata;
SFOutput_t sensorFusion::_sfOutputData;
IMU_Data_t sensorFusion::_imudata;
// CoordinatedTurnAttitudeManagerCommands_t coordinateTurnElevation::_rollandrudder;
// AltitudeAirspeedCommands_t coordinateTurnElevation::_pitchandairspeed;

// No longer reciving data from AM 
//AttitudeData commsWithAttitude::_receivedData;

// Landing and Takeoff Variables (some stages DO NOT need waypoint inputs, it's not a mistake that some are missing)
// For landing and takeoff states that use waypoint manager functions, the variables below will hold the I/O that is needed for those functions to work

// Preflight
_LandingTakeoffInput preflightStage::input;
_LandingTakeoffOutput preflightStage::output;
_WaypointManager_Data_Out preflightStage::waypointOutput;
_PathData * preflightStage::currentLocation;
_PathData * preflightStage::targetWaypoint;
WaypointManager preflightStage::preflightPath;
_WaypointStatus preflightStage::waypointStatus;

// Takeoff
_LandingTakeoffInput takeoffStage::input;
_LandingTakeoffOutput takeoffStage::output;
_PathData * takeoffStage::currentLocation;
_PathData * takeoffStage::targetWaypoint;
WaypointManager takeOffPath;
_WaypointManager_Data_In takeoffStage::waypointInput;
_WaypointManager_Data_Out takeoffStage::waypointOutput;
 _WaypointStatus takeoffStage::waypointStatus;



//Landing
_LandingTakeoffInput landingStage::input;
_LandingTakeoffOutput landingStage::output;
_WaypointManager_Data_In landingStage::waypointInput;
_WaypointManager_Data_Out landingStage::waypointOutput;
_WaypointStatus landingStage::waypointStatus;
_PathData * landingStage::currentLocation;
_PathData * landingStage::targetWaypoint;
WaypointManager landingStage::landingPath;
_LandingPath landingStage::path;

#define ON_GROUND_ALT 2 //minimum on ground altitude
constexpr int CRUISING_AIRSPEED {15};




/***********************************************************************************************************************
 * Code
 **********************************************************************************************************************/

void commsWithAttitude::execute(pathManager* pathMgr)
{


    // CoordinatedTurnAttitudeManagerCommands_t * turnCommands = coordinateTurnElevation::GetRollAndRudder();
    // AltitudeAirspeedCommands_t * altCommands = coordinateTurnElevation::GetPitchAndAirspeed();

    _WaypointManager_Data_Out * waypointOutput {}; 
    
    //deciding which stage we get output data from 
    switch(pathMgr->stage){
        case LANDING:
            waypointOutput = landingStage::GetOutputData();
            break;
        case CRUISING:
            waypointOutput = cruisingState::GetOutputData();
            break;
        case PREFLIGHT:
            waypointOutput = preflightStage::GetOutputData();
            break;
        case TAKEOFF:
           waypointOutput = takeoffStage::GetOutputData();
            break;
        default:
            waypointOutput = cruisingState::GetOutputData();
    }


    CommandsForAM toSend {};

    toSend.desiredX = waypointOutput->distanceX;
    toSend.desiredY = waypointOutput->distanceY;
    toSend.desiredZ = waypointOutput->distanceZ;
    toSend.rotation = waypointOutput->rotation; 
    toSend.airspeed = waypointOutput->desiredAirspeed; 


    SendFromPMToAM(&toSend); // Sends commands to attitude manager

    pathMgr->setState(commsWithTelemetry::getInstance());
}

pathManagerState& commsWithAttitude::getInstance()
{
    static commsWithAttitude singleton;
    return singleton;
}

void commsWithTelemetry::execute(pathManager* pathMgr)
{
    GetTelemetryCommands(&_incomingData);

    // should there be a replacement for this?
    // #ifndef SIMULATION
    // setPitchPercent(_incomingData.gimbalPitch);
    // setYawPercent(_incomingData.gimbalYaw);
    // #endif

    if(pathMgr->isError)
    {
        pathMgr->setState(fatalFailureMode::getInstance());
    }
    else
    {
        pathMgr->setState(sensorFusion::getInstance());
    }
}

pathManagerState& commsWithTelemetry::getInstance()
{
    static commsWithTelemetry singleton;
    return singleton;
}

void sensorFusion::execute(pathManager* pathMgr)
{
    SFError_t error = SF_GetResult(&_sfOutputData); // Gets current Sensor fusion output struct
    _imudata = SF_GetRawIMU();

    if(pathMgr->isError)
    {
        pathMgr->setState(fatalFailureMode::getInstance());
    }
    else
    {
        pathMgr->setState(resetVariables::getInstance());
    }
}

pathManagerState& sensorFusion::getInstance()
{
    static sensorFusion singleton;
    return singleton;
}

void resetVariables::execute(pathManager* pathMgr)
{
    //resetting the variables for passby
    if(pathMgr->isError)
    {
        pathMgr->setState(fatalFailureMode::getInstance());
    }

    //if takeOff command is false, start landing
    if(!commsWithTelemetry::GetTelemetryIncomingData()->takeoffCommand)
    {
        pathMgr->stage = LANDING;
    
    }

    //if takeOff command is true and we are still on the ground, start preTakeOff 
    if(commsWithTelemetry::GetTelemetryIncomingData()->takeoffCommand && sensorFusion::GetSFOutput()->altitude < ON_GROUND_ALT)
    {
        pathMgr->stage = PREFLIGHT;

    }

    //checking of 
    if (pathMgr->stage == PREFLIGHT && preflightStage::getPreFlightCompleteStatus){
        pathMgr->stage = TAKEOFF; 

    }

     // If we are above the ground, reached a certain height, and takeOff is still true, begin cruising 
    if(sensorFusion::GetSFOutput()->altitude > EXIT_TAKEOFF_ALTITUDE && commsWithTelemetry::GetTelemetryIncomingData()->takeoffCommand)
    {
        pathMgr->stage = CRUISING;

    }
 

    //I have deleted reset passby 
    switch(pathMgr->stage){
        case LANDING:
            pathMgr->setState(landingStage::getInstance());
            break;
        case CRUISING:
            //no control details need to be reset for cruising state
            pathMgr->setState(cruisingState::getInstance());
            break;
        case PREFLIGHT:
            pathMgr->setState(preflightStage::getInstance());
            break;
        case TAKEOFF:
            pathMgr->setState(takeoffStage::getInstance());
            break;
        default:
            pathMgr->setState(cruisingState::getInstance());
    }
}

pathManagerState& resetVariables::getInstance()
{
    static resetVariables singleton;
    return singleton;
}

void cruisingState::execute(pathManager* pathMgr)
{

    // telementryData is where we want to go, target waypoint
    fijo * telemetryData = commsWithTelemetry::GetTelemetryIncomingData(); // Get struct from telemetry state with all of the commands and values.
    SFOutput_t * sensFusionOutput = sensorFusion::GetSFOutput(); // Get sensor fusion data

    // Set waypoint manager input struct 
    // input data is the current position, should be middle waypoint
    _inputdata.track = sensFusionOutput->track; // Gets track
    _inputdata.longitude = sensFusionOutput->longitude; // setting current waypoint longitude to SF longitude 
    _inputdata.latitude = sensFusionOutput->latitude;
    _inputdata.altitude = sensFusionOutput->altitude;

    // Call module functions - These will be changed later 

    //initializing current and target waypoint, setting the relationship of their pointers 
    _ModifyFlightPathErrorCode editError = editFlightPath(cruisingStateManager, telemetryData, this->_inputdata); // Edit flight path if applicable
    
    
    _GetNextDirectionsErrorCode pathError = pathFollow(cruisingStateManager, telemetryData, _inputdata, &_outputdata); // Get next direction or modify flight behaviour pattern
    setReturnValues(&_returnToGround, cruisingStateManager, editError, pathError); // Set error codes

    if(pathMgr->isError)
    {
        pathMgr->setState(fatalFailureMode::getInstance());
    }
    else
    {
        // pathMgr->setState(coordinateTurnElevation::getInstance());
         pathMgr->setState(commsWithAttitude::getInstance()); 
    }
}

pathManagerState& cruisingState::getInstance()
{
    static cruisingState singleton;
    return singleton;
}


//KILL MOST OF THIS WITH FIRE -> DONT NEED THIS 
// void coordinateTurnElevation::execute(pathManager* pathMgr)
// {

//     SFOutput_t * sensFusionOutput = sensorFusion::GetSFOutput(); // Get sensor fusion data
//     IMU_Data_t * imudata = sensorFusion::GetIMUData(); // Gets raw IMU data
//     _WaypointManager_Data_Out * waypointOutput = cruisingState::GetOutputData(); // Get output data from waypoint manager

//     CoordinatedTurnInput_t turnInput {};
//     AltitudeAirspeedInput_t altAirspeedInput {};
    
//     _LandingTakeoffOutput outputHolder; //had a bug with initializing the passby details, defining the pointer like this fixed it
//     _LandingTakeoffOutput * landingTakeoffOutput = &outputHolder;
//     //get elevation and turning data
//     //loading in commands data from each of the states
//     switch(pathMgr->stage){
//         case PREFLIGHT:
//             landingTakeoffOutput = preflightStage::getControlOutput();
//             break;
//         case LANDING:
//             landingTakeoffOutput = landingStage::getControlOutput();
//             break;
//         case TAKEOFF:
//             landingTakeoffOutput = takeoffStage::getControlOutput();
//             break;
//         case CRUISING:
//             turnInput.currentHeadingTrack = sensFusionOutput->track; // Gets track;
//             turnInput.desiredHeadingTrack = waypointOutput->desiredTrack;
//             turnInput.accY = imudata->accy;

//             // Set up the compute altitude and airspeed function input
//             altAirspeedInput.currentAltitude = sensFusionOutput->altitude;
//             altAirspeedInput.desiredAltitude = waypointOutput->desiredAltitude;
//             altAirspeedInput.currentAirspeed = sensFusionOutput->airspeed;
//             altAirspeedInput.desiredAirspeed = waypointOutput->desiredAirspeed;
//             break;
//         default:
//             //should default to cruising

//             turnInput.currentHeadingTrack = sensFusionOutput->track; // Gets track;
//             turnInput.desiredHeadingTrack = waypointOutput->desiredTrack;
//             turnInput.accY = imudata->accy;

//             // Set up the compute altitude and airspeed function input
//             altAirspeedInput.currentAltitude = sensFusionOutput->altitude;
//             altAirspeedInput.desiredAltitude = waypointOutput->desiredAltitude;
//             altAirspeedInput.currentAirspeed = sensFusionOutput->airspeed;
//             altAirspeedInput.desiredAirspeed = waypointOutput->desiredAirspeed;

//     }

//     if(pathMgr->stage!=CRUISING)
//     {
//         LandingTakeoffManager::translateLTSFCommandsToCoordTurns(*landingTakeoffOutput, *sensFusionOutput, *imudata, turnInput, altAirspeedInput);
//     }

//     // Call module functions
//     AutoSteer_ComputeCoordinatedTurn(&turnInput, &_rollandrudder);
//     AutoSteer_ComputeAltitudeAndAirspeed(&altAirspeedInput, &_pitchandairspeed);

//     //passby functionality
    
//     if(landingTakeoffOutput->controlDetails.pitchPassby)
//     {
//         _pitchandairspeed.requiredPitch = landingTakeoffOutput->controlDetails.pitchPercent;
//     }

//     if(landingTakeoffOutput->controlDetails.throttlePassby)
//     {
//         _pitchandairspeed.requiredThrottlePercent = landingTakeoffOutput->controlDetails.throttlePercent;
//     }

//     if(landingTakeoffOutput->controlDetails.rollPassby)
//     {
//         _rollandrudder.requiredRoll = landingTakeoffOutput->controlDetails.rollPercent;
//     }

//     if(landingTakeoffOutput->controlDetails.rudderPassby)
//     {
//         _rollandrudder.requiredRudderPosition = landingTakeoffOutput->controlDetails.rudderPercent;
//     }
    
//     if(pathMgr->isError)
//     {
//         pathMgr->setState(fatalFailureMode::getInstance());
//     }
//     else
//     {
//         pathMgr->setState(commsWithAttitude::getInstance());
//     }
// }

// pathManagerState& coordinateTurnElevation::getInstance()
// {
//     static coordinateTurnElevation singleton;
//     return singleton;
// }

void fatalFailureMode::execute(pathManager* pathMgr)
{
    pathMgr->setState(fatalFailureMode::getInstance());
}

pathManagerState& fatalFailureMode::getInstance()
{
    static fatalFailureMode singleton;
    return singleton;
}

/****************************************************************************************************
LANDING STATE FUNCTIONS
****************************************************************************************************/

void landingStage::execute(pathManager* pathMgr)
{
    pathMgr->stage = LANDING;
    //load in sensor fusion data and telemtry data into input structure
    input.telemetryData = commsWithTelemetry::GetTelemetryIncomingData();
    input.sensorOutput = sensorFusion::GetSFOutput();

    //Think I need to get rid of this if statement as we should continuously create target and currentLocation waypoints
    // if(!pathMgr->madeLandingPoints)
    // {
        //Getting current location from sensor fusion 
        currentLocation = landingPath.initialize_waypoint(input.sensorOutput->longitude, input.sensorOutput->latitude, input.sensorOutput->altitude, LANDING_WAYPOINT); 

        // Gordon this is for you! - targetWaypoint Must have long, lat, alt, and waypointType = LANDING
        // targetWaypoint = someFancyLandingTakeoffMath

        //initializing flight path - sets the relationship between current location's and target's pointers
        waypointStatus = landingPath.initialize_flight_path(targetWaypoint, currentLocation);

        // pathMgr->madeLandingPoints = true;
    // }

    //we use this instead of the currentLocationWaypoint for get_next_directions for reasons I hate 
    waypointInput.latitude = input.sensorOutput->latitude;
    waypointInput.longitude = input.sensorOutput->longitude;
    waypointInput.altitude = input.sensorOutput->altitude;
    waypointInput.track = input.sensorOutput->track;

    //going from current to target, 
    waypointStatus = landingPath.get_next_directions(waypointInput, &waypointOutput);

    //maintaining altitude
    waypointOutput.desiredAirspeed = CRUISING_AIRSPEED;


    if(landingStage::waypointStatus != WAYPOINT_SUCCESS)
    {
        pathMgr->isError = true;
    }

    if(pathMgr->isError)
    {
        pathMgr->setState(fatalFailureMode::getInstance());
    }
    else
    {
        pathMgr->setState(commsWithAttitude::getInstance());
    }
}

pathManagerState& landingStage::getInstance()
{
    static landingStage singleton;
    return singleton;
}


/****************************************************************************************************
TAKEOFF STATE FUNCTIONS
****************************************************************************************************/

//create a count to 1000 -> then proceed to takeoff. 
void preflightStage::execute(pathManager* pathMgr)
{
    //load in sensor fusion data and telemtry data into input structure
    input.telemetryData = commsWithTelemetry::GetTelemetryIncomingData();
    input.sensorOutput = sensorFusion::GetSFOutput();

    
    // This is the part where we ~wait~ before going to takeoff...not entirely sure if I'm doing this right 
    // I'll make this a const later once I know if this actually works 

    if (cycleCount < 1000){
        cycleCount++; 
        preFlightComplete = false;
    } else { 
        preFlightComplete = true; 
        cycleCount = 0; 
    }


    if(preflightStage::waypointStatus != WAYPOINT_SUCCESS)
    {
        pathMgr->isError = true;
    }

    if(pathMgr->isError)
    {
        pathMgr->setState(fatalFailureMode::getInstance());
    }
    else
    {
        // pathMgr->setState(coordinateTurnElevation::getInstance());
        pathMgr->setState(commsWithAttitude::getInstance());
    }
}

pathManagerState& preflightStage::getInstance()
{
    static preflightStage singleton;
    return singleton;
}

void takeoffStage::execute(pathManager* pathMgr)
{
    //load in sensor fusion data and telemtry data into input structure
    input.telemetryData = commsWithTelemetry::GetTelemetryIncomingData();
    input.sensorOutput = sensorFusion::GetSFOutput();

        // we use this instead of the currentLocationWaypoint for get_next_directions for reasons I hate 
        waypointInput.latitude = input.sensorOutput->latitude;
        waypointInput.longitude = input.sensorOutput->longitude;
        waypointInput.altitude = input.sensorOutput->altitude;
        waypointInput.track = input.sensorOutput->track;

         //Getting current location from sensor fusion 
        currentLocation = takeOffPath.initialize_waypoint(input.sensorOutput->longitude, input.sensorOutput->latitude, input.sensorOutput->altitude, LANDING_WAYPOINT); 

        // Gordon this is for you! - targetWaypoint Must have long, lat, alt, and waypointType = TAKEOFF
        // targetWaypoint = someFancyLandingTakeoffMath

        //initializing flight path - sets the relationship between current location's and target's pointers
        waypointStatus = takeOffPath.initialize_flight_path(targetWaypoint, currentLocation);

        // Going from current to target
        waypointStatus = takeOffPath.get_next_directions(waypointInput, &waypointOutput);

        //maintaining altitude
        waypointOutput.desiredAirspeed = CRUISING_AIRSPEED;        
        

    if(waypointStatus != WAYPOINT_SUCCESS)
    {
        pathMgr->isError = true;
    }

    if(pathMgr->isError)
    {
        pathMgr->setState(fatalFailureMode::getInstance());
    }
    else
    {
        pathMgr->setState(commsWithAttitude::getInstance());
    }
}

pathManagerState& takeoffStage::getInstance()
{
    static takeoffStage singleton;
    return singleton;
}

#endif
