/* P1P2_System.h
 *
 * Copyright (c) 2024 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * Version history
 * 20240515 v0.9.46 first version Daikin system parameters
 * ..
 */

#ifndef P1P2_System
#define P1P2_System

#ifdef E_SERIES

#define INIT_R4T_OFFSET 0 // div100
#define INIT_R1T_OFFSET 0 // div100
#define INIT_R2T_OFFSET 0 // div100
#define INIT_RT_OFFSET  0 // div100

#define INIT_USE_TOTAL 0  // 0=use compressor-heating for COP-lifetime/before/after, 1=use total for COP-lifetime/before/after

#endif /* E_SERIES */

#ifdef F_SERIES

#define INIT_SETPOINT_COOLING_MIN 16
#define INIT_SETPOINT_COOLING_MAX 32
#define INIT_SETPOINT_HEATING_MIN 16
#define INIT_SETPOINT_HEATING_MAX 32

#endif /* F_SERIES */

#ifdef H_SERIES
#define HITACHI_MODEL 1
#endif /* H_SERIES */

#endif /* P1P2_System */
