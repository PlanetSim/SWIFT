/*******************************************************************************
 * This file is part of SWIFT.
 * Coypright (c) 2022 Bert Vandenbroucke (bert.vandenbroucke@gmail.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/
#ifndef SWIFT_PRESSURE_FLOOR_DEBUG_H
#define SWIFT_PRESSURE_FLOOR_DEBUG_H

/* Config parameters. */
#include <config.h>

/* Import the debug routines of the right pressure floor definition */
#if defined(PRESSURE_FLOOR_NONE)
#include "./pressure_floor/none/pressure_floor_debug.h"
#elif defined(PRESSURE_FLOOR_GEAR)
#include "./pressure_floor/GEAR/pressure_floor_debug.h"
#else
#error "Invalid choice of pressure floor structure."
#endif

#endif /* SWIFT_PRESSURE_FLOOR_DEBUG_H */
