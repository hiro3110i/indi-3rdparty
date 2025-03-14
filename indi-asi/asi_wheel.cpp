/*
 ASI Filter Wheel INDI Driver

 Copyright (c) 2016 by Rumen G.Bogdanovski.
 All Rights Reserved.

 Contributors:
 Yang Zhou
 Hans Lambermont

 Code is based on SX Filter Wheel INDI Driver by Gerry Rozema
 Copyright(c) 2010 Gerry Rozema.
 All rights reserved.

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 of the License, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.

 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59
 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 The full GNU General Public License is included in this distribution in the
 file called LICENSE.
 */

#include "asi_wheel.h"

#include "config.h"

#include <string.h>
#include <unistd.h>
#include <deque>
#include <memory>

//#define SIMULATION

static class Loader
{
        std::deque<std::unique_ptr<ASIWHEEL>> wheels;
    public:
        Loader()
        {
#ifdef SIMULATION
            EFW_INFO info;
            info.ID = 1;
            strncpy(info.Name, "Simulated EFW8", 64);
            info.slotNum = 0;
            wheels.push_back(std::unique_ptr<ASIWHEEL>(new ASIWHEEL(info, info.Name)));
#else
            int num_wheels = EFWGetNum();

            if (num_wheels <= 0)
            {
                IDLog("No ASI EFW detected.");
                return;
            }
            int num_wheels_ok = 0;
            char *envDev = getenv("INDIDEV");
            for (int i = 0; i < num_wheels; i++)
            {
                int id;
                EFW_ERROR_CODE result = EFWGetID(i, &id);
                if (result != EFW_SUCCESS)
                {
                    IDLog("ERROR: ASI EFW %d EFWGetID error %d.", i + 1, result);
                    continue;
                }
                EFW_INFO info;
                result = EFWGetProperty(id, &info);
                if (result != EFW_SUCCESS && result != EFW_ERROR_CLOSED)   // TODO: remove the ERROR_CLOSED hack
                {
                    IDLog("ERROR: ASI EFW %d EFWGetProperty error %d.", i + 1, result);
                    continue;
                }
                std::string name = "ZWO " + std::string(info.Name);
                if (envDev && envDev[0])
                    name = envDev;

                // If we only have a single device connected
                // then favor the INDIDEV driver label over the auto-generated name above
                if (num_wheels > 1)
                    name += " " + std::to_string(i + 1);

                wheels.push_back(std::unique_ptr<ASIWHEEL>(new ASIWHEEL(info, name.c_str())));
                num_wheels_ok++;
            }
            IDLog("%d ZWO EFW attached out of %d detected.", num_wheels_ok, num_wheels);
#endif
        }
} loader;

ASIWHEEL::ASIWHEEL(const EFW_INFO &info, const char *name)
{
    fw_id              = info.ID;
    CurrentFilter      = 0;
    FilterSlotNP[0].setMin(0);
    FilterSlotNP[0].setMax(0);
    setDeviceName(name);
    setVersion(ASI_VERSION_MAJOR, ASI_VERSION_MINOR);
}

ASIWHEEL::~ASIWHEEL()
{
    Disconnect();
}

const char *ASIWHEEL::getDefaultName()
{
    return "ZWO EFW";
}

bool ASIWHEEL::Connect()
{
    if (isSimulation())
    {
        LOG_INFO("Simulation connected.");
        fw_id = 0;
        FilterSlotNP[0].setMin(1);
        FilterSlotNP[0].setMax(8);
    }
    else if (fw_id >= 0)
    {
        EFW_ERROR_CODE result = EFWOpen(fw_id);
        if (result != EFW_SUCCESS)
        {
            LOGF_ERROR("%s(): EFWOpen() = %d", __FUNCTION__, result);
            return false;
        }

        EFW_INFO info;
        result = EFWGetProperty(fw_id, &info);
        if (result != EFW_SUCCESS)
        {
            LOGF_ERROR("%s(): EFWGetProperty() = %d", __FUNCTION__, result);
            return false;
        }

        LOGF_INFO("Detected %d-position filter wheel.", info.slotNum);

        FilterSlotNP[0].setMin(1);
        FilterSlotNP[0].setMax(info.slotNum);

        // get current filter
        int current;
        result = EFWGetPosition(fw_id, &current);
        if (result != EFW_SUCCESS)
        {
            LOGF_ERROR("%s(): EFWGetPosition() = %d", __FUNCTION__, result);
            return false;
        }
        SelectFilter(current + 1);
        LOGF_DEBUG("%s(): current filter position %d", __FUNCTION__, CurrentFilter);
    }
    else
    {
        LOGF_INFO("%s(): no filter wheel known, fw_id = %d", __FUNCTION__, fw_id);
        return false;
    }
    return true;
}

bool ASIWHEEL::Disconnect()
{
    if (isSimulation())
    {
        LOG_INFO("Simulation disconnected.");
    }
    else if (fw_id >= 0)
    {
        EFW_ERROR_CODE result = EFWClose(fw_id);
        if (result != EFW_SUCCESS)
        {
            LOGF_ERROR("%s(): EFWClose() = %d", __FUNCTION__, result);
            return false;
        }
    }
    else
    {
        LOGF_INFO("%s(): no filter wheel known, fw_id = %d", __FUNCTION__, fw_id);
        return false;
    }
    // NOTE: do not unset fw_id here, otherwise we cannot reconnect without reloading the driver
    return true;
}

bool ASIWHEEL::initProperties()
{
    INDI::FilterWheel::initProperties();

    // Unidirectional motion
    IUFillSwitch(&UniDirectionalS[INDI_ENABLED], "INDI_ENABLED", "Enable", ISS_OFF);
    IUFillSwitch(&UniDirectionalS[INDI_DISABLED], "INDI_DISABLED", "Disable", ISS_ON);
    IUFillSwitchVector(&UniDirectionalSP, UniDirectionalS, 2, getDeviceName(), "FILTER_UNIDIRECTIONAL_MOTION", "Uni Direction",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&CalibrateS[0], "CALIBRATE", "Calibrate", ISS_OFF);
    IUFillSwitchVector(&CalibrateSP, CalibrateS, 1, getDeviceName(), "FILTER_CALIBRATION", "Calibrate",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    addAuxControls();
    setDefaultPollingPeriod(250);
    return true;
}

bool ASIWHEEL::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
    {
        bool isUniDirection = false;
        if (!isSimulation() && EFWGetDirection(fw_id, &isUniDirection) == EFW_SUCCESS)
        {
            UniDirectionalS[INDI_ENABLED].s = isUniDirection ? ISS_ON : ISS_OFF;
            UniDirectionalS[INDI_DISABLED].s = isUniDirection ? ISS_OFF : ISS_ON;
        }
        defineProperty(&UniDirectionalSP);
        defineProperty(&CalibrateSP);
    }
    else
    {
        deleteProperty(UniDirectionalSP.name);
        deleteProperty(CalibrateSP.name);
    }

    return true;
}

bool ASIWHEEL::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if (!strcmp(name, UniDirectionalSP.name))
        {
            EFW_ERROR_CODE rc = EFWSetDirection(fw_id, !strcmp(IUFindOnSwitchName(states, names, n),
                                                UniDirectionalS[INDI_ENABLED].name));
            if (rc == EFW_SUCCESS)
            {
                IUUpdateSwitch(&UniDirectionalSP, states, names, n);
                UniDirectionalSP.s = IPS_OK;
            }
            else
            {
                LOGF_ERROR("%s(): EFWSetDirection = %d", __FUNCTION__, rc);
                UniDirectionalSP.s = IPS_ALERT;
            }
            IDSetSwitch(&UniDirectionalSP, nullptr);
            return true;
        }
        if (!strcmp(name, CalibrateSP.name))
        {
            CalibrateS[0].s = ISS_OFF;

            if (isSimulation())
            {
                return true;
            }

            CalibrateSP.s   = IPS_BUSY;
            IDSetSwitch(&CalibrateSP, nullptr);

            // make the set filter number busy
            FilterSlotNP.setState(IPS_BUSY);
            FilterSlotNP.apply();

            LOGF_DEBUG("Calibrating EFW %d", fw_id);
            EFW_ERROR_CODE rc = EFWCalibrate(fw_id);

            if (rc == EFW_SUCCESS)
            {
                IEAddTimer(getCurrentPollingPeriod(), ASIWHEEL::TimerHelperCalibrate, this);
                return true;
            }
            else
            {
                LOGF_ERROR("%(): EFWCalibrate = %d", __FUNCTION__, rc);
                CalibrateSP.s = IPS_ALERT;
                IDSetSwitch(&CalibrateSP, nullptr);

                // reset filter slot state
                FilterSlotNP.setState(IPS_OK);
                FilterSlotNP.apply();
                return false;
            }
        }
    }

    return INDI::FilterWheel::ISNewSwitch(dev, name, states, names, n);
}

int ASIWHEEL::QueryFilter()
{
    if (isSimulation())
        return CurrentFilter;

    if (fw_id >= 0)
    {
        EFW_ERROR_CODE result;
        result = EFWGetPosition(fw_id, &CurrentFilter);
        if (result != EFW_SUCCESS)
        {
            LOGF_ERROR("%s(): EFWGetPosition() = %d", __FUNCTION__, result);
            return 0;
        }
        CurrentFilter++;
    }
    else
    {
        LOGF_INFO("%s(): no filter wheel known, fw_id = %d", __FUNCTION__, fw_id);
        return 0;
    }

    return CurrentFilter;
}

bool ASIWHEEL::SelectFilter(int f)
{
    TargetFilter = f;
    if (isSimulation())
    {
        CurrentFilter = TargetFilter;
        return true;
    }

    if (fw_id >= 0)
    {
        EFW_ERROR_CODE result;
        result = EFWSetPosition(fw_id, f - 1);
        if (result == EFW_SUCCESS)
        {
            SetTimer(getCurrentPollingPeriod());
            do
            {
                result = EFWGetPosition(fw_id, &CurrentFilter);
                CurrentFilter++;
                usleep(getCurrentPollingPeriod() * 1000);
            }
            while (result == EFW_SUCCESS && CurrentFilter != TargetFilter);
            if (result != EFW_SUCCESS)
            {
                LOGF_ERROR("%s(): EFWSetPosition() = %d", __FUNCTION__, result);
                return false;
            }
        }
        else
        {
            LOGF_ERROR("%s(): EFWSetPosition() = %d", __FUNCTION__, result);
            return false;
        }
    }
    else
    {
        LOGF_INFO("%s(): no filter wheel known, fw_id = %d", __FUNCTION__, fw_id);
        return false;
    }
    return true;
}

void ASIWHEEL::TimerHit()
{
    QueryFilter();

    if (CurrentFilter != TargetFilter)
    {
        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        SelectFilterDone(CurrentFilter);
    }
}

bool ASIWHEEL::saveConfigItems(FILE *fp)
{
    INDI::FilterWheel::saveConfigItems(fp);
    IUSaveConfigSwitch(fp, &UniDirectionalSP);
    return true;
}

void ASIWHEEL::TimerHelperCalibrate(void *context)
{
    static_cast<ASIWHEEL*>(context)->TimerCalibrate();
}

void ASIWHEEL::TimerCalibrate()
{
    // check current state of calibration
    int position;
    EFW_ERROR_CODE rc = EFWGetPosition(fw_id, &position);

    if (rc == EFW_SUCCESS)
    {
        if (position == EFW_IS_MOVING)
        {
            // while filterwheel is moving we're still calibrating
            IEAddTimer(getCurrentPollingPeriod(), ASIWHEEL::TimerHelperCalibrate, this);
            return;
        }
        LOGF_DEBUG("Successfully calibrated EFW %d", fw_id);
        CalibrateSP.s   = IPS_OK;
        IDSetSwitch(&CalibrateSP, nullptr);
    }
    else
    {
        LOGF_ERROR("%(): EFWCalibrate = %d", __FUNCTION__, rc);
        CalibrateSP.s = IPS_ALERT;
        IDSetSwitch(&CalibrateSP, nullptr);
    }

    FilterSlotNP.setState(IPS_OK);
    FilterSlotNP.apply();
    return;
}
