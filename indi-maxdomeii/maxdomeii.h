/*
    MaxDome II
    Copyright (C) 2009 Ferran Casarramona (ferran.casarramona@gmail.com)

    Migrated to INDI::Dome by Jasem Mutlaq (2014)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#pragma once

#include <indidome.h>
#include "maxdomeiidriver.h"

#define MD_AZIMUTH_IDLE   0
#define MD_AZIMUTH_MOVING 1
#define MD_AZIMUTH_HOMING 2

class MaxDomeII : public INDI::Dome
{
  public:
    MaxDomeII();
    ~MaxDomeII();

    virtual const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool saveConfigItems(FILE *fp) override;

    virtual bool Disconnect() override;
    virtual bool Handshake() override;

    virtual void TimerHit() override;

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    virtual IPState MoveAbs(double az) override;
    virtual IPState Move(DomeDirection dir, DomeMotionCommand operation) override;

    //virtual IPState Home();
    virtual bool Abort() override;

  protected:
    // Parking
    IPState ConfigureShutterOperation(int nMDBOS, double ShutterOperationAzimuth);
    virtual IPState Park() override;
    virtual IPState UnPark() override;
    virtual bool SetCurrentPark() override;
    virtual bool SetDefaultPark() override;
    virtual IPState ControlShutter(ShutterOperation operation) override;

    /*******************************************************/
    /* Misc routines
 ********************************************************/
    int AzimuthDistance(int nPos1, int nPos2);
    double TicksToAzimuth(int nTicks);
    int AzimuthToTicks(double nAzimuth);
    int handle_driver_error(int *error, int *nRetry); // Handles errors returned by driver

    INDI::PropertySwitch ShutterModeSP {2};
    enum
    {
        FULL,
        UPPER
    };

    INDI::PropertyNumber ShutterOperationAzimuthNP {1};

    INDI::PropertySwitch ShutterConflictSP {2};
    enum
    {
      MOVE,
      NO_MOVE
    };

    INDI::PropertySwitch HomeSP {1};

    INDI::PropertyNumber TicksPerTurnNP {1};

    INDI::PropertyNumber WatchDogNP {1};

    INDI::PropertyNumber HomeAzimuthNP {1};

    INumber HomePosRN[1];
    INumberVectorProperty HomePosRNP;

  private:
    int nTicksPerTurn;           // Number of ticks per turn of azimuth dome
    unsigned nCurrentTicks;      // Position as reported by the MaxDome II
    int nMoveDomeBeforeOperateShutter; // 0 no move
    double nShutterOperationPosition;        // Go to this position before operate the shutter. Controlled by the MaxDomeII firmware, in conjuntion to nMoveDomeBeforeOperateShutter
    double nHomeAzimuth;         // Azimuth of home position
    int nHomeTicks;              // Ticks from 0 azimuth to home
    int nTimeSinceShutterStart;  // Timer since shutter movement has started, in order to check timeouts
    int nTimeSinceAzimuthStart;  // Timer since azimuth movement has started, in order to check timeouts
    int nTargetAzimuth;
    int nTimeSinceLastCommunication; // Used by Watch Dog

    double prev_az, prev_alt;

    bool SetupParms();

    MaxDomeIIDriver driver;
};
