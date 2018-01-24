/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2017 Matthieu Schaller (matthieu.schaller@durham.ac.uk)
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
#ifndef SWIFT_COSMOLOGY_H
#define SWIFT_COSMOLOGY_H

/* Config parameters. */
#include "../config.h"

#include "engine.h"
#include "parser.h"

/**
 * @brief Cosmological parameters
 */
struct cosmology {

  /*! Current expansion factor of the Universe */
  double a;

  /*! Inverse of the current expansion factor of the Universe */
  double a_inv;

  /*! Current redshift */
  double z;

  /*! Hubble constant at the current redshift (in internal units) */
  double H;

  /*! Starting expansion factor */
  double a_begin;

  /*! Ending expansion factor */
  double a_end;

  /*! Reduced Hubble constant (H0 / (100km/s/Mpc)) */
  double h;

  /*! Hubble constant at z = 0 (in internal units) */
  double H0;

  /*! Matter density parameter */
  double Omega_m;

  /*! Baryon density parameter */
  double Omega_b;

  /*! Radiation constant density parameter */
  double Omega_lambda;

  /*! Cosmological constant density parameter */
  double Omega_r;

  /*! Curvature density parameter */
  double Omega_k;
};

void cosmology_update(struct cosmology *c, const struct engine *e);

void cosmology_init(const struct swift_params *params,
                    const struct unit_system *us,
                    const struct phys_const *phys_const, struct cosmology *c);

void cosmology_print(const struct cosmology *c);

#endif /* SWIFT_COSMOLOGY_H */
