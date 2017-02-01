/*
 * Copyright (c) 2015, University of Kaiserslautern
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
 *    documentation and/or other materials provided with the distributio
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permissio
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
 * Authors: Omar Naji, Matthias Jung, Christian Weis
 */

#include "Timing.h"
#include <math.h>
#include <iostream>
#include <fstream>

double
Timing::timeToPercentage(double percentage)
{
    return -log(1.0 - percentage/100.0);
}

bool
Timing::calctrcd()
{
    // Trcd is divided as following:
    // the first part is for wordline driver delay estimated as 2 ns
    // the second part is the delay throught the local wordline and
    // local bitline and delay coming from cell itself
    // the 3rd part is the delay of the ssa and is estimated to 2 ns
    // calculating tau for wl
    // calculating wordline total resistance ( value in Ohm )
    // local wordline resistance equals resistance of local wordline driver+
    // resistance of cells in wordline direction

    // Calculating tau for cell celltau ( in ns )
    celltau = timeToPercentage(90)
              * SCALE_QUANTITY(capacitancePerCell, drs::nanofarad_per_cell_unit)
              * resistancePerCell
              * drs::cell * drs::cell;

    localWordlineResistance = LWLDriverResistance
                              + (cellsPerLWL *  resistancePerWLCell);

    // Calculating wordline total capacitance ( value in ff)
    localWordlineCapacitance = cellsPerLWL
            * SCALE_QUANTITY(capacitancePerWLCell, drs::nanofarad_per_cell_unit);

    // Calculating wltau( in ns )
    wltau = timeToPercentage(90)
            * localWordlineCapacitance
            * localWordlineResistance
            * drs::subarray * drs::subarray;

    // Calculating bitline total resistance
    blr = cellsPerLBL * resistancePerBLCell;

    // Calculating bitline total capacitance
    blc = cellsPerLBL
          * SCALE_QUANTITY(capacitancePerBLCell, drs::nanofarad_per_cell_unit);

    // Calculating the bltau( in ns )
    // for timing of bitline, the voltage should reach 90% of the value
    // or else the BLSA can kipp to the wrong value => tau*2.3
    bltau = timeToPercentage(90)
            * blr * drs::subarray
            * blc * drs::subarray;

    //calculating GWL decoder + wiring delay
    //calculating global wordline total capa in fF
    GWDR = wireResistance
           * SCALE_QUANTITY(tileWidth, drs::millimeter_per_tile_unit);

    GWDC = SCALE_QUANTITY(wireCapacitance, drs::nanofarad_per_millimeter_unit)
           * SCALE_QUANTITY(tileWidth, drs::millimeter_per_tile_unit);

    // Calculating delay through global wordline driver and wiring
//   [ns]   [ns]              [ohm]         [fF]
    tGWLD = driverOffset +
            timeToPercentage(90) * GWLDriverResistance * GWDC * drs::tile +
            timeToPercentage(63) * GWDR * drs::tile * GWDC * drs::tile;
    
    // Calculating trcd
//  [ns]    [ns]    [ns]     [ns]     [ns]    [ns]
    trcd = tGWLD + wltau + bltau + celltau + SSADelay ;

    return true;
}

//bool
//Timing::calctras()
//{
//    // tras is divided into 4 parts: trcd + tCAS + time for precharging
//    // SSA(~2ns) + time needed to restore the bits into the cells
//    // (tau bitline)
//    //
//    // Calculating tCAS:
//    // tCAS has delay from CSL driver + Wire ,Local Dataline driver
//    // + wire (~1 ns ), Global Data line driver + wire, Delay from
//    // SSA to Dataqueue(DQ)
//    // delay through CSL
//    // CSL wire capacitane in fF + 8 fF for load
////    [fF]      [um/bank]     [fF/mm]       [um -> mm] !Add [bank]!
//    CSLcapa = Bankheight * wireCapacitance * (1/pow(10.0,3.0))
////      [fF]
//        + 8;
//    //tau CSL in ns
////        [ns]   [ns]                [ohm]               [fF]
//    float tCSL = 0.6 +  (2.2 * (CSLDriverResistance * CSLcapa)
////           [ohm/mm]        [fF/mm]
//     + (wireResistance * wireCapacitance
////          [um/bank]      [um/bank]       [um -> mm]^2
//        * Bankheight * Bankheight)*(1/pow(10.0,6.0)))
////              [fs -> ns]     !Add [bank]^2!
//            *(1/pow(10.0,6.0));

//    //through global data line(voltage on dataline should reach 90%
////    [fF]      [um/bank]     [fF/mm]        [um -> mm] !Add [bank]!
//    GDLcapa = Bankheight * wireCapacitance * (1/pow(10.0,3.0));

////        [ns]    [ns]                [ohm]          [fF]
//    float tGDL =  0.6 +  (2.2 * (GDLDriverResistance * GDLcapa)
////          [ohm/mm]         [fF/mm]
//     + (wireResistance * wireCapacitance
////          [um/bank]      [um/bank]       [um -> mm]^2
//         * Bankheight * Bankheight)*(1/pow(10.0,6.0)))
////               [fs -> ns]     !Add [bank]^2!
//            *(1/pow(10.0,6.0));

//    // Delay from SSA to DQ//assume wire length = 5mm
//    // the length of the DQ wire depends on the width of the bank
////          [mm]
//    int dqwirelength;
    
//    // Factor which defines the rowbuffer*subarray2rowbuffer relation
//    float bankwidthfactor;
////   [Kbyte??]       [KByte]           [sa_row/ma_row] !CHECK!
//    bankwidthfactor = pageSize * subArrayRatioToPage;

//    // Currently we assume 5 mm taken from a DDR3 ( 8 banks config)
//    // when the number of banks increases this distance from bank to
//    // DQ should increas. We do not model this yet due to lack of input.
//    // This should be changed in the future.
//    if (ThreeD == "ON") {
////          [mm]      [mm]
//        dqwirelength = 1;
//    } else {
//        if (bankwidthfactor == 1) {
//            dqwirelength = 3;
//        } else if (bankwidthfactor == 2) {
//            dqwirelength = 5;
//        } else if(bankwidthfactor == 4) {
//            dqwirelength = 7;
//        } else if(bankwidthfactor == 8) {
//            dqwirelength = 9;
//        } else if(bankwidthfactor == 0.5) {
//            dqwirelength = 2;
//        } else {
//            dqwirelength = 5;
//            std::cout<<"WARNING: YOUR PAGESIZE*Subarray2rowbuffer SEEMS to be too big!!"<<"\n";
//        }
//    }
////   [fF]        [mm]        [fF/mm]
//    DQcapa = dqwirelength * wireCapacitance;
////       [ns]  [ns]                [ohm]         [fF]
//    float tDQ = 0.6 + (2.2 * (DQDriverResistance * DQcapa)
////          [ohm/mm]          [fF/mm]       [mm]           [mm]
//     + (wireResistance * wireCapacitance * dqwirelength * dqwirelength))
////               [fs -> ns]
//            *(1/pow(10.0,6.0));
    
//    // Calculating tcl:
//    // command decoding latency (consider for now 2ns) +
//    // ( readout-sensing of SSA ) + tDQ + 1 ns (interface delay)
//// [ns]  [ns] [ns] [ns]  [ns] [ns] [ns]  [ns]
//    tcl = 2 + tCSL + 1 + tGDL + 2 + tDQ + 1;
    
//    // Calculating trtp:
//    // read to precharge
//    // trtp is tCSL + tGDL + delay for sense amps.
//    // This is the point in time where precharging is allowed
//    // std::cout<<"bltau"<<bltau<<std::endl;
//    // std::cout<<"tCSL"<<tCSL<<std::endl;
//    // remove the command decoding latency
////  [ns]  [ns] [ns] [ns] [ns] [ns]
//    trtp = tcl - 2 - tDQ - 1 - 1;
    
//    // Calculating tccd:
//    // tccd ( column to column delay) is equal time to select another CSL
//    //  + precharging secondary SSA + Global Dataline delay...
//    // - 0.5(delay of CSL driver isnot in the critical path)
////  [ns]   [ns] [ns]  [ns]   [ns]
//    tccd = tCSL + 1 + tGDL - 0.5;
    
//    // Calculating tras:
////  [ns]   [ns]  [ns]    [ns]   [ns] [ns]
//    tras = trcd + tcl + bltau - tDQ - 1;

//    // Calculating twr + 2 (command decoding) + 1 (security margin):
//// [ns] [ns]   [ns]    [ns]  [ns]
//    twr = 2 + bltau + tGDL + 1;

//    return true;
//}

//bool
//Timing::calctrp()
//{
//    //calculating trp
//    //trp = Master wordline delay ( discharging ) + local wordline delay
//    // (discharging ) + 1 ns ( equalizer delay )
////  [ns]    [ns]   [ns]  [ns]
//    trp =  wltau + 1 + bltau;

//    return true;
//}

//bool
//Timing::calctrc()
//{
//    //calculating trc
////  [ns]  [ns]   [ns]
//    trc = tras + trp;

//    return true;
//}

//bool
//Timing::calctrl()
//{
//    //calculating trl
//    //trl is equal to tcl
////  [ns]  [ns]
//    trl = tcl;

//    return true;
//}

//bool
//Timing::calctwl()
//{
//    return true;
//}

//bool
//Timing::calctrfc()
//{
//    //time for refresh cycle
//    //numberofbanks refreshed*(5(Act cmd delay ) +
//    // 5(pre cmd delay)) + 10 ns (offset)) + trc
//    int numberbanks;
//    if (ThreeD == "ON") {
////      [ns]           [??]              [??/bank]
//        trfc = rowRefreshRate * banksRefreshFactor
////                     [bank]           []         [] []
//                * nBanks/vaultsPerLayer*(5+5)
////               [ns]   [ns]
//                + 10  + trc;
//        numberbanks = nBanks;
//    } else {
////      [ns]           [??]              [??]
//        trfc = rowRefreshRate * banksRefreshFactor
////                     [bank]     [] []
//                * nBanks*(5+5)
////               [ns]   [ns]
//                + 10  + trc;
//    }

//    return true;
//}

//bool
//Timing::calctref1()
//{
//    //time for average refresh period
//    //number of rows
//    //number of rows = (DRAM SIZE/#ofbanks) / rowbuffer
////       [row/bank]        [Gbit]   [Gbit -> Kbit]     [bank]
//    int numberofrows = (dramSize * 1024 * 1024 / (nBanks) )
////        [Kbyte/row] [Kbyte -> Kbit]
//    / (pageSize * 8);
////   [ns]          [??]             [ms]           [ms -> ns]      [row/bank]
//    tref1 = (rowRefreshRate * retentionTime * 1000 * 1000) / (numberofrows);

//    return true;
//}

//bool
//Timing::Timingclk()
//{

////        [MHz]   [MHz]
//    float freq = dramFreq;
////           [MHz]            [ns]  [GHz -> MHz]
//    float freq_core_max = (1/ tccd ) * 1000;


//    float freq_core_actual;
//    // if we specified a core Freq then take the value else calculate it
//    if (dramCoreFreq != 0) {
////           [MHz]           [MHz]
//        freq_core_actual = dramCoreFreq;
//    } else {
////            [??]
//        int clockfactor;
//        if (DramType == "DDR") {
//            clockfactor = 2;
//        } else {
//            clockfactor = 1;
//        }
////           [MHz]        [MHz]      [??]        [??]
//        freq_core_actual = freq / (Prefetch/clockfactor);
//    }
////   [MHz]         [MHz]
//    ActFreq = freq_core_actual;
//    // scale the actual tcl, tccd, trl to the actual core freq
////   [ns]    [ns]        [MHz]          [MHz]
//// ?? MEANINLESS ??
//    tcl_act = tcl * (freq_core_max/freq_core_actual);
////   [ns]      [ns]
//    // ?? MEANINLESS ??
//    trl_act = tcl_act;
////   [ns]      [ns]        [MHz]          [MHz]
//    // ?? MEANINLESS ??
//    tccd_act = tccd * (freq_core_max/freq_core_actual);


//    if(freq_core_actual < freq_core_max ) {
//        std::cout<<"The Specified Frequency fits the DRAM Design!!!"<<"\n";
//    } else {
//        std::cout<<"WARNING : Specified Frequency too high"
//                 <<"for DRAM DesigGo Down with Frequency!!"
//                 <<"\n";

//        std::cout<<"If User wants to keep the high frequency,"
//                 <<"try using a smaller bank or a higher "
//                 <<"subarray2rowbufferfactor"<<"\n";
//    }
////[ns/clock]   [MHz]  [us -> ns] !Add clock^-1!
//    clk = (1 / freq ) * 1000;
//    float clk_act = (1 / freq_core_actual ) * 1000;

//    //trcd in clk cycles round-up
////  [clock]        [ns] [ns/clock]
//    trcd_clk = ceil(trcd/clk);

//    //tcl in clk cycles
//// [clock]         [ns] [ns/clock]
//    tcl_clk = ceil(tcl/clk);

//    //tcl_act in clk cycles
////   [clock]           [ns] [ns/clock]
//    tcl_act_clk = ceil(tcl/clk_act);

//    //tras in clk cycles
////   [clock]        [ns] [ns/clock]
//    tras_clk = ceil(tras/clk);

//    //trp in clk cycles
////   [clock]       [ns] [ns/clock]
//    trp_clk = ceil(trp/clk);

//    //trc in clk cycles
////   [clock]       [ns] [ns/clock]
//    trc_clk = ceil(trc/clk);
    
//    //trl in clk cycles
//    //tal is additional latency defined in tech. parameter
////   [clock]    [ns] [ns/clock] [clock]
//    trl_clk = ceil(tcl/clk) + additionalLatencyTrl;

//    //trl_act in clk cycles
//    //tal is additional latency defined in tech. parameter
////   [clock]         [ns] [ns/clock]  [clock]
//    trl_act_clk = ceil(tcl/clk_act) + additionalLatencyTrl;

//    //twl in clk cycles
////  [clock]   [clock]  clock]
//    twl_clk = trl_clk - 1;

//    //trtp in clk cycles
////  [clock]        [ns] [ns/clock]
//    trtp_clk = ceil(trtp/clk);

//    //tccd in clk cycles
////  [clock]        [ns] [ns/clock]
//    tccd_clk = ceil(tccd/clk);

//    //tccd_act in clk cycles
////  [clock]              [ns] [ns/clock]
//    tccd_act_clk = ceil(tccd/clk_act);

//    //twr in clk cycles
////  [clock]        [ns] [ns/clock]
//    twr_clk = ceil(twr/clk);

//    //trfc in clk cycles
////  [clock]         [ns] [ns/clock]
//    trfc_clk = ceil(trfc/clk);
    
//    //tref1 in clk cycles
////  [clock]          [ns] [ns/clock]
//    tref1_clk = ceil(tref1/clk);

//    return true;
//}

//void
//Timing::printTiming()
//{
//    //print analog timings
//    std::cout << "Timing Parameters in ns" << "\n";
//    std::cout << "trcd"          << "\t"  << trcd     << ".\n";
//    std::cout << "tcl"           << "\t"  << tcl      << ".\n";
//    std::cout << "actual tcl"    << "\t"  << tcl_act  << ".\n";
//    std::cout << "trtp" << "\t"  << trtp  << ".\n";
//    std::cout << "tccd" << "\t"  << tccd  << ".\n";
//    std::cout << "actual tccd"   << "\t"  << tccd_act << ".\n";
//    std::cout << "tras" << "\t"  << tras  << ".\n";
//    std::cout << "twr" << "\t"   << twr   << "\n" ;
//    std::cout << "trp" << "\t"   << trp   << ".\n";
//    std::cout << "trc" << "\t"   << trc   << ".\n";
//    std::cout << "trl" << "\t"   << trl   << ".\n";
//    std::cout << "actual trl"    << "\t"  << trl_act  << ".\n";
//    std::cout << "trfc" << "\t"  << trfc  << "\n";
//    std::cout << "tref1" << "\t" << tref1 << "\n";
    
//}
