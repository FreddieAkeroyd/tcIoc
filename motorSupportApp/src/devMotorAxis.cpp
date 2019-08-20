#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dbAccess.h>
#include <stdexcept>
#include <epicsThread.h>
#include <sstream>

#include "asynMotorController.h"
#include "asynMotorAxis.h"
#include <errlog.h>
#include <epicsExport.h>
#include "devMotor.h"

#ifndef ASYN_TRACE_INFO
#define ASYN_TRACE_INFO      0x0040
#endif

#ifndef MIN
#define MIN(x,y)  (((x) < (y)) ? (x) : (y))
#endif

/** 
  * Creates a new devMotorAxis object.
  *
  * Initializes the PV prefix for all the underlying pvs to communicate with.
  *
  * \param[in] pC Pointer to the devMotorController to which this axis belongs.
  * \param[in] axisNo Index number of this axis, range 1 to pC->numAxes_
  */
devMotorAxis::devMotorAxis(devMotorController *pC, int axisNo) 
    : asynMotorAxis(pC, axisNo), pC_(pC), axisNo(axisNo)
{
    printf("Axis %i created\n", axisNo);
	pvPrefix = pC_->pvPrefix + "AXES_" + std::to_string(axisNo + 1) + ":";
    pC_->wakeupPoller();
	setIntegerParam(pC_->motorStatusHasEncoder_, 1);
}

/** 
  * The external C function to create the axis that will be called by iocsh.
  *
  * Creates the motor axis object.
  *
  * \param[in] devMotorName The name of the motor port.
  * \param[in] axisNo Index number of this axis, range 1 to pC->numAxes_
  */
extern "C" int devMotorCreateAxis(const char *devMotorName, int axisNo) {
    devMotorController *pC;

    pC = (devMotorController*) findAsynPortDriver(devMotorName);
    if (!pC)
    {
        printf("Error port %s not found\n", devMotorName);
        return asynError;
    }
    pC->lock();
    new devMotorAxis(pC, axisNo);
    pC->unlock();
    return asynSuccess;
}

/**
  * Send a command to the PLC.
  * 
  * A command is sent in two steps, first the command code is entered and then execute is called.
  *
  * \param[in] command The command code to send.
  *
  * \return The status code for sending the command.
  */
asynStatus devMotorAxis::sendCommand(const int command) {
    int exec = 1;
    int status = putDb("ECOMMAND", &command);
    status |= putDb("BEXECUTE", &exec);
    return (asynStatus)status;
}

/** 
  * Move the axis to a position, either absolute or relative, called by the motor record.
  * 
  * \param[in] position in mm
  * \param[in] relative (0=absolute, otherwise relative)
  * \param[in] minimum velocity, mm/sec (currently unused)
  * \param[in] maximum velocity, mm/sec
  * \param[in] acceleration, seconds to maximum velocity (currently unused)
  *
  * \return The status code for the move.
  */
asynStatus devMotorAxis::move(double position, int relative, double minVelocity, double maxVelocity, double acceleration) {
	try {
		scaleValueFromMotorRecord(&position);
		scaleValueFromMotorRecord(&maxVelocity);

		int status = putDb("FVELOCITY", &maxVelocity);
		
		if (relative == 0) {
			status |= putDb("FPOSITION", &position);
			return (asynStatus)sendCommand(MOVE_ABS_COMMAND);
		} else {
			status |= putDb("FDISTANCE", &position);
			return (asynStatus)sendCommand(MOVE_RELATIVE_COMMAND);
		}
	}  catch (const std::runtime_error& e) {
		asynPrint(pC_->pasynUserSelf, ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER,
					"Failed to move to %f axis %i: %s\n", position, axisNo, e.what());
		return asynError;
	}
}

/** 
  * Home the motor to a known position, called by the motor record.
  * 
  * \param[in] minimum velocity, mm/sec (currently unused)
  * \param[in] maximum velocity, mm/sec (currently unused)
  * \param[in] acceleration, seconds to maximum velocity (currently unused)
  * \param[in] forwards (0=backwards, otherwise forwards) (currently unused)
  *
  * \return The status code for the home.
  */
asynStatus devMotorAxis::home(double minVelocity, double maxVelocity, double acceleration, int forwards) {
    try {
		return (asynStatus)sendCommand(HOME_COMMAND);
	}  catch (const std::runtime_error& e) {
		asynPrint(pC_->pasynUserSelf, ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER,
					"Failed to home axis %i: %s\n", axisNo, e.what());
		return asynError;
	}
}

/** 
  * Jog the the motor.
  *
  * \param[in] minimum velocity, mm/sec (currently unused)
  * \param[in] maximum velocity, mm/sec (positive or negative)
  * \param[in] acceleration, seconds to maximum velocity (currently unused)
  *
  * \return The status code for the move.
  */
asynStatus devMotorAxis::moveVelocity(double minVelocity, double maxVelocity, double acceleration) {
    try {
		scaleValueFromMotorRecord(&maxVelocity);
		
		int status = putDb("FVELOCITY", &maxVelocity);
		status |= sendCommand(MOVE_VELO_COMMAND);
		return (asynStatus)status;
    }  catch (const std::runtime_error& e) {
		asynPrint(pC_->pasynUserSelf, ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER,
					"Failed to move velocity axis %i: %s\n", axisNo, e.what());
		return asynError;
	}
}

/** 
  * Stops the axis, called by motor record.
  * 
  * \param[in] acceleration The acceleration to stop the axis with (currently unused).
  *
  * \return The status code for the stop.
  */
asynStatus devMotorAxis::stop(double acceleration) {
    try {
		return (asynStatus)sendCommand(STOP_COMMAND);
	}  catch (const std::runtime_error& e) {
		asynPrint(pC_->pasynUserSelf, ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER,
					"Failed to stop axis %i: %s\n", axisNo, e.what());
		return asynError;
	}
}

/**
  * Gets the motor resolution inside the motor record.
  * 
  * \return The motor resolution or 1.0 if no resolution can be found.
  */
double devMotorAxis::getMotorResolution() {
    double mres = 1.0;
    if (pC_->getDoubleParam(axisNo_, pC_->motorResolution_, &mres) == asynSuccess) {
        return mres;
    } else {
        return 1.0;
    }
}

/**
  * Scales the value coming from the motor record towards the PLC.
  *
  * The PLC works in real world units the scaling will need to be but
  * the motor record works in steps, the resolution is the scaling.
  *
  * \param[out] value A pointer to the value that you wish to scale.
  */
void devMotorAxis::scaleValueFromMotorRecord(double* value) {
    *value *= getMotorResolution();
}

/**
  * Scales the value coming from the PLC towards the motor record.
  *
  * The PLC works in real world units the scaling will need to be but
  * the motor record works in steps, the resolution is the scaling.
  *
  * \param[out] value A pointer to the value that you wish to scale.
  */
void devMotorAxis::scaleValueToMotorRecord(double* value) {
    *value /= getMotorResolution();
}

/**
  * Get the direction which the motor is running in.
  *
  * The PLC has two flags, one for moving in a positive direction and one for negative,
  * this merges these two flags into a single direction integer.
  *
  * \param[out] direction The variable to put the direction into.
  */
void devMotorAxis::getDirection(int *direction) {
    int positiveDirection = 0;
    int negativeDirection = 0;
    getInteger("BPOSITIVEDIRECTION", &positiveDirection);
    getInteger("BNEGATIVEDIRECTION", &negativeDirection);
    if (positiveDirection && negativeDirection) {
        throw std::runtime_error(std::string("Axis is running in both directions"));
    } else if (positiveDirection) {
        *direction = 1;
    } else if (negativeDirection) {
        *direction = 0;
    }
}

/**
  * Pulls all relevant values out of the PLC.
  *
  * \param[out] axis_status The st_axis_status_type variable to put the information into.
  *
  * \return The status code for the polling.
  */
asynStatus devMotorAxis::pollAll(st_axis_status_type *axis_status) { 
	getInteger("BENABLE", &axis_status->bEnable);
	getInteger("BEXECUTE", &axis_status->bExecute);
	getDouble("FVELOCITY", &axis_status->fVelocity);
	getDouble("FPOSITION", &axis_status->fPosition);
	getInteger("FWLIMIT_" + std::to_string(axisNo + 1), &axis_status->bLimitFwd, &pC_->pvPrefix); 
	getInteger("BWLIMIT_" + std::to_string(axisNo + 1), &axis_status->bLimitBwd, &pC_->pvPrefix);
	getInteger("BERROR", &axis_status->bError);
	getDouble("FACTPOSITION", &axis_status->fActPosition);
	getDouble("FACTVELOCITY", &axis_status->fActVelocity);
	getInteger("BCALIBRATED", &axis_status->bHomed);
	getInteger("BMOVING", &axis_status->bMoving);
	getDirection(&axis_status->bDirection);
    return asynSuccess;
}

/**
  * Puts a value into a PV.
  *
  * \param[in] pvSuffix The name of the PV to put into (without the PV prefix)
  * \param[in] value The value to put into the PV. This is a void pointer as this method will work for both ints and doubles.
  * 
  * \return The status code from doing the put.
  */
asynStatus devMotorAxis::putDb(std::string pvSuffix, const void *value) {
    DBADDR addr;
	std::string fullPV = pvPrefix + pvSuffix;
    if (dbNameToAddr(fullPV.c_str(), &addr)) {
		throw std::runtime_error("PV not found: " + fullPV);
        return asynError;
    }

    return (asynStatus) dbPutField(&addr, addr.dbr_field_type, value, 1);
}

/**
  * Get a value from a PV.
  *
  * \param[in] pvSuffix The name of the PV to get from (without the PV prefix)
  * \param[out] addr The DBADDR structure to store metadata about the PV
  * \param[out] pbuffer The buffer to store the value retrieved from the PV
  * \param[in] prefix The prefix to use for the PV, if not specified then will default to the one setup in the constructor
  */
void devMotorAxis::getPVValue(std::string& pvSuffix, DBADDR* addr, long* pbuffer, const std::string* prefix) {
    long options = 0;
    long no_elements;
    
	if (!prefix) {
		prefix = &pvPrefix;
	}

	std::string fullPV = *prefix + pvSuffix;
    if (dbNameToAddr(fullPV.c_str(), addr)) {
        throw std::runtime_error("PV not found: " + fullPV);
    }
      
    no_elements = MIN(addr->no_elements, PV_BUFFER_LEN/addr->field_size);
    if (dbGet(addr, addr->dbr_field_type, pbuffer, &options, &no_elements, NULL) != 0) {
        throw std::runtime_error("Could not get value from PV: " + fullPV);
    }

    epicsEnum16 status = addr->precord->stat;
    epicsEnum16 severity = addr->precord->sevr;
	
	if (status) {
		std::ostringstream os;
		os << "PV " + fullPV + " in alarm with status " << status << " and severity " << severity;
		throw std::runtime_error(os.str());
	}
}

/**
  * Get a double value from a PV.
  *
  * \param[in] pvSuffix The name of the PV to get from (without the PV prefix)
  * \param[out] pvalue The object to store the value retrieved from the PV
  */
void devMotorAxis::getDouble(std::string pvSuffix, epicsFloat64* pvalue) {
    long buffer[PV_BUFFER_LEN];
	long *pbuffer=&buffer[0];
    DBADDR addr;

    getPVValue(pvSuffix, &addr, pbuffer);
    
    if (addr.dbr_field_type == DBR_DOUBLE) {
        *pvalue = (*(epicsFloat64 *) pbuffer);
    }
}

/**
  * Get a integer value from a PV.
  *
  * \param[in] pvSuffix The name of the PV to get from (without the PV prefix)
  * \param[out] pvalue The object to store the value retrieved from the PV
  * \param[in] prefix The prefix to use for the PV, if not specified then will default to the one setup in the constructor
  */
void devMotorAxis::getInteger(std::string pvSuffix, epicsInt32* pvalue, const std::string* prefix) {
    long buffer[PV_BUFFER_LEN];
    long *pbuffer=&buffer[0];
    DBADDR addr;
    
    getPVValue(pvSuffix, &addr, pbuffer, prefix);
    
    if (addr.dbr_field_type == DBR_LONG) {
        *pvalue = (*(epicsInt32 *) pbuffer);
    } else if (addr.dbr_field_type == DBR_ENUM) {
        *pvalue = (*(epicsEnum16 *) pbuffer);
    }
}


/** 
  * Poll called by the motor record to get information about the axis.
  *
  * This function reads the motor position, the limit status, the moving status,
  * and the drive power-on status. It calls setIntegerParam() and setDoubleParam() for each item that it polls,
  * and then calls callParamCallbacks() at the end to update the PV.
  *
  * \param[out] moving A flag that is set indicating that the axis is moving (true) or done (false).
  *
  * \return The status code from doing the poll.
  */
asynStatus devMotorAxis::poll(bool *moving) {
    asynStatus comStatus = asynSuccess;
    int nowMoving = 0;
	st_axis_status_type st_axis_status;
	memset(&st_axis_status, 0, sizeof(st_axis_status));
	
	// The comms error neeeds to be set to 0 to start to force the new error on init
	setIntegerParam(pC_->motorStatusCommsError_, 0);

	try {		
		// Go and get all the values from the device
		pollAll(&st_axis_status);

	} catch (const std::runtime_error& e) {
		int mask = previousError == e.what() ? ASYN_TRACEIO_DRIVER : ASYN_TRACE_ERROR | ASYN_TRACEIO_DRIVER;
		asynPrint(pC_->pasynUserSelf, mask, "Failed to poll axis %i: %s\n", axisNo, e.what());
		previousError = e.what();
		comStatus = asynError;
    }

	// Set the MSTA bits
	setIntegerParam(pC_->motorStatusHomed_, st_axis_status.bHomed);
	setIntegerParam(pC_->motorStatusProblem_, st_axis_status.bError);
	setIntegerParam(pC_->motorStatusLowLimit_, !st_axis_status.bLimitBwd);
	setIntegerParam(pC_->motorStatusHighLimit_, !st_axis_status.bLimitFwd);
	setIntegerParam(pC_->motorStatusPowerOn_, st_axis_status.bEnable);
	setIntegerParam(pC_->motorStatusAtHome_, 0);
	setIntegerParam(pC_->motorStatusDirection_, st_axis_status.bDirection);
	
	// Get the actual position
	scaleValueToMotorRecord(&st_axis_status.fActPosition);
	setDoubleParam(pC_->motorPosition_, st_axis_status.fActPosition);
	setDoubleParam(pC_->motorEncoderPosition_, st_axis_status.fActPosition);
	
	// Calculate if moving and set appropriate bits
	nowMoving = st_axis_status.bMoving;
	setIntegerParam(pC_->motorStatusMoving_, nowMoving);
	setIntegerParam(pC_->motorStatusDone_, !nowMoving);
	*moving = nowMoving ? true : false;
	
	setIntegerParam(pC_->motorStatusCommsError_, comStatus ? 1 : 0);
	
    callParamCallbacks();
    
    return asynSuccess;
}
