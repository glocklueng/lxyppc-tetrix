/*
  June 2012

  BaseFlightPlus Rev -

  An Open Source STM32 Based Multicopter

  Includes code and/or ideas from:

  1)AeroQuad
  2)BaseFlight
  3)CH Robotics
  4)MultiWii
  5)S.O.H. Madgwick

  Designed to run on Naze32 Flight Control Board

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MARGAHRS_H
#define MARGAHRS_H

//=====================================================================================================
// AHRS.h
// S.O.H. Madgwick
// 25th August 2010
//
// 1 June 2012 Modified by J. Ihlein
//=====================================================================================================
//
// See AHRS.c file for description.
//
//=====================================================================================================

//----------------------------------------------------------------------------------------------------
// Variable declaration
#include "alg_types.h"
extern float q0, q1, q2, q3;    // quaternion elements representing the estimated orientation

//---------------------------------------------------------------------------------------------------
// Function declaration

void MargAHRSupdate(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz, float accelCutoff, uint8_t newMagData, float dt);

//=====================================================================================================
// End of file
//=====================================================================================================

#endif


