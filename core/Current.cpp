/*
 * Copyright (c) 2017, University of Kaiserslautern
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Omar Naji,
 *          Matthias Jung,
 *          Christian Weis,
 *          Kamal Haddad,
 *          Andre Lucas Chinazzo
 */



#include "Current.h"
#include <math.h>
#include <iostream>
#include <fstream>

void
Current::currentInitialize()
{
    // TODO: Find appropriate var. names and whether it should or not be an user input
    IDD2nPercentageIfNotDll = 0.6; // Fixed
    currentPerPageSizeSlope = 1.0*drs::milliamperes_page_per_kibibyte; // To be estimated
    SSAActiveTime = 1.5*drs::nanoseconds; // 1.5 tccd instead
    bitProCSL = 8 * drs::bits; // Fixed

    // Intermediate values added as variables for code cleanness
    nActiveSubarrays = 0;
    nLocalBitlines = 0;
    IDD0TotalCharge = 0*drs::nanocoulomb;
    effectiveTrc = 0*drs::nanosecond;
    IDD0ChargingCurrent = 0*si::amperes;
    IDD1TotalCharge = 0*drs::nanocoulomb;
    IDD1ChargingCurrent = 0*si::amperes;
    IDD4TotalCharge = 0*drs::nanocoulomb;
    ioTermRdCurrent = 0*drs::milliamperes;
    IDD4ChargingCurrent = 0*si::amperes;
    ioTermWrCurrent = 0*drs::milliamperes;
    refreshCharge = 0*drs::nanocoulomb;
    effectiveTrfc = 0*drs::nanosecond;
    IDD5ChargingCurrent = 0*si::amperes;

    // Main variables
    IDD0 = 0*drs::milliamperes;
    IDD1 = 0*drs::milliamperes;
    IDD4R = 0*drs::milliamperes;
    IDD4W = 0*drs::milliamperes;
    IDD2n = 0*drs::milliamperes;
    IDD3n = 0*drs::milliamperes;
    IDD5 = 0*drs::milliamperes;

    masterWordlineCharge = 0*drs::nanocoulomb;
    localWordlineCharge = 0*drs::nanocoulomb;
    localBitlineCharge = 0*drs::nanocoulomb;
    nLDQs = 0*drs::bits;
    SSACharge = 0*drs::nanocoulomb;
    nCSLs = 0;
    CSLCharge = 0*drs::nanocoulomb;
    masterDatalineCharge = 0*drs::nanocoulomb;
    DQWireCharge = 0*drs::nanocoulomb;
    readingCharge = 0*drs::nanocoulomb;

    includeIOTerminationCurrent = false;
}

// Background current calculation
void
Current::backgroundCurrentCalc()
{
    // Precharge background current (scaling with Freq)
    IDD2n = backgroundCurrentSlope * dramFreq
            + backgroundCurrentOffset;

    if ( !isDLL ) {
        IDD2n = IDD2nPercentageIfNotDll * IDD2n;
    }

    // Active background current:
    // IDD3n is equal to IDD2n + active-adder
    // Assumption : active-adder current = 4mA for 2K page
    // and changes linearly while changing page size
    IDD3n = IDD2n + currentPerPageSizeSlope * pageStorage;

}

void
Current::IDD0Calc()
{
    //   !!! CHECK !!!
    // Number of active subarrays
    //    [subarray / page] ???
    nActiveSubarrays = 1.0 * drs::bank * effectivePageStorage
            / ( subArrayRowStorage * 1.0*drs::subarray );

    if ( dramType == "DDR4" ) {
        vppPumpsEfficiency = 1;
    } else {
        vppPumpsEfficiency = 0.3;

    }

    // Charge of master wordline
    masterWordlineCharge = globalWordlineCapacitance * 1.0*drs::tile
                           *  vpp / vppPumpsEfficiency;

    // Charge of local wordline
    localWordlineCharge = localWordlineCapacitance * 1.0*drs::subarray
                          * vpp
                          * nActiveSubarrays
                          / vppPumpsEfficiency ;

    //charge of local bitline
    nLocalBitlines = SCALE_QUANTITY(pageStorage, drs::bit_per_page_unit)*drs::page/drs::bit;
    localBitlineCharge = localBitlineCapacitance * 1.0*drs::subarray
                         * vdd/2.0
                         * nLocalBitlines;

    rowAddrsLinesCharge =
            SCALE_QUANTITY(wireCapacitance, drs::nanofarad_per_millimeter_unit)
            * SCALE_QUANTITY(tileHeight, drs::millimeter_per_tile_unit)
            * nTilesPerBank * 1.0*drs::bank
            * nRowAddressLines
            * vdd;

    IDD0TotalCharge = ( masterWordlineCharge
                      + localWordlineCharge
                      + localBitlineCharge
                      + rowAddrsLinesCharge)
                            * 2.0 ; // !! Why times 2? Maybe number of tiles?

    // Clock cycles of trc in ns
    effectiveTrc = trc_clk * clkPeriod;
    // Current caused by charging and discharging of capas in mA
    IDD0ChargingCurrent = IDD0TotalCharge / effectiveTrc;

    IDD0 = IDD3n
           + SCALE_QUANTITY(IDD0ChargingCurrent, drs::milliampere_unit);

}

void
Current::IDD1Calc()
{

    nLDQs = interface * prefetch;

    // Charges of SSA
    SSACharge = nLDQs
                * SCALE_QUANTITY(Issa, drs::ampere_per_bit_unit)
                * SSAActiveTime;

    // nCSLs includes CSLEN and !CSLEN lines (+ 2),
    //  and we consider they have the same capacitance as CSLs.
    nCSLs = nSubArraysPerArrayBlock * nHorizontalTiles * 1.0*drs::bank/drs::subarrays + 2.0;
    CSLCharge = CSLCapacitance * 1.0*drs::bank
                * vdd
                * nCSLs;

    // Charge of global Dataline
    masterDatalineCharge = globalDatalineCapacitance / drs::bit
                           * 1.0*drs::bank
                           * vdd
                           * interface
                           * prefetch;
    // Charges for Dataqueue // 1 Read is done for interface x prefetch
    DQWireCharge = DQWireCapacitance
                   * vdd
                   * interface
                   * prefetch
                    / drs::bit; // TODO: remove the work around

    // read charges in pC
    readingCharge = SSACharge
                    + 2.0*CSLCharge // !! Why times 2? Maybe number of tiles?
                    + 2.0*masterDatalineCharge // !! Why times 2? Maybe number of tiles?
                    + DQWireCharge;
    IDD1TotalCharge = (masterWordlineCharge
                       + localWordlineCharge
                       + localBitlineCharge
                       + readingCharge)
                            * 2.0; // !! Why times 2? Maybe number of tiles?

    IDD1ChargingCurrent = IDD1TotalCharge / effectiveTrc;

    IDD1 = IDD3n
           + SCALE_QUANTITY(IDD1ChargingCurrent, drs::milliampere_unit);

}

void
Current::IDD4RCalc()
{

    colAddrsLinesCharge =
            SCALE_QUANTITY(wireCapacitance, drs::nanofarad_per_millimeter_unit)
            * SCALE_QUANTITY(bankWidth, drs::millimeter_per_bank_unit)
            * 1.0*drs::bank
            * nColumnAddressLines
            * vdd;

    IDD4TotalCharge = readingCharge + colAddrsLinesCharge;
    if (includeIOTerminationCurrent) {
      // Scale the IDD_OCD_RCV with frequency linearly
      IddOcdRcv = IddOcdRcvSlope * dramFreq / drs::bit;

      // Case for [LP]DDRX ( x4, x8, x16 or x32 per channel )
      //  Must have a DQS/!DQS pair for each 8 bits of data lines
      if ( interface <= 32 * drs::bits) {
       ioTermRdCurrent = ( interface + ceil(interface / 8.0) * 2.0 )
                         * SCALE_QUANTITY(IddOcdRcv, drs::milliampere_per_bit_unit);
      }
      // Case for WIDEIO2 ( x64 per channel )
      //  Must have a DQS/!DQS pair for each 16 bits of data lines
      else if ( interface <= 64 * drs::bits) {
       ioTermRdCurrent = ( interface + ceil(interface / 16.0) * 2.0 )
                         * SCALE_QUANTITY(IddOcdRcv, drs::milliampere_per_bit_unit);
      }
      // Case for HBM ( x128 per channel )
      //  Must have a DQS/!DQS pair for each 32 bits of data lines
      else if ( interface <= 128 * drs::bits) {
       ioTermRdCurrent = ( interface + ceil(interface / 32.0) * 2.0 )
                         * SCALE_QUANTITY(IddOcdRcv, drs::milliampere_per_bit_unit);
      }
      else {
        std::string exceptionMsgThrown("[ERROR] ");
        exceptionMsgThrown.append("Custom interface size ");
        exceptionMsgThrown.append("(greater than 128 bits or ");
        exceptionMsgThrown.append("not a power of two) ");
        exceptionMsgThrown.append("model is not yet implemented!");
        throw exceptionMsgThrown;
      }
    }
    else {
       ioTermRdCurrent = 0*drs::milliamperes;
    }

    IDD4ChargingCurrent = IDD4TotalCharge
                          * SCALE_QUANTITY(dramCoreFreq, drs::gigahertz_clock_unit)
                          / (1.0*drs::clock);

    IDD4R = IDD3n
            + ioTermRdCurrent
            + SCALE_QUANTITY(IDD4ChargingCurrent, drs::milliampere_unit);

}

void
Current::IDD4WCalc()
{
    // Termination current for writting must include DQS/!DQS pairs
    //  as well as DM lines charges (one for each 8 bits of data lines).
    if (includeIOTerminationCurrent) {
      ioTermWrCurrent = ioTermRdCurrent
                        + ceil(interface / 8.0)
                          * SCALE_QUANTITY(IddOcdRcv, drs::milliampere_per_bit_unit);
    }
    else {
        ioTermWrCurrent = 0*drs::milliamperes;
    }

    IDD4W = IDD3n
            + ioTermWrCurrent
            + SCALE_QUANTITY(IDD4ChargingCurrent, drs::milliampere_unit);

}

void
Current::IDD5Calc()
{
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! //
    // !!!! TODO: DISCUSS THESE VALUES WITH CHRISTIAN IN FURTHER MEETINGS !!!! //
    // Charges for refresh
    // Charges of IDD0 x 2 ( 2 rows pro command)x # of banksrefreshed pro
    // x # of rows pro command
    refreshCharge = IDD0TotalCharge
                    * nRowsRefreshedPerARCmd;

    // Refresh current
    effectiveTrfc = trfc_clk * clkPeriod;

    IDD5ChargingCurrent = refreshCharge / effectiveTrfc;

    IDD5 = IDD3n
           + SCALE_QUANTITY(IDD5ChargingCurrent, drs::milliampere_unit);

}

void
Current::currentCompute()
{
    backgroundCurrentCalc();
    IDD0Calc();
    IDD1Calc();
    IDD4RCalc();
    IDD4WCalc();
    IDD5Calc();
}
