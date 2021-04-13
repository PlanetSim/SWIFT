/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2021 Bert Vandenbroucke (bert.vandenbroucke@gmail.com)
 *               2021 Nina Sartorio (sartorio.nina@gmail.com)
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

/* Config parameters. */
#include "turbulence.h"

#include "../config.h"
#include "engine.h"

#ifdef TURBULENCE_DRIVING_ALVELIUS

/**
 * @brief Initialises the turbulence driving in the internal system of units.
 *
 * This function reads the parameters from the parameter file and counts the
 * number of wavevectors within the driving range. It then allocates appropriate
 * arrays to store the driving information and initialises these, including
 * the random number generator used to update the stochastic forcing.
 *
 * Finally, the state of the random number generator is fast-forwarded to the
 * desired starting time, making it possible to reproduce the driving even for
 * a simulation that was restarted from an arbitrary snapshot.
 *
 * @param parameter_file The parsed parameter file
 * @param phys_const Physical constants in internal units
 * @param us The current internal system of units
 * @param turbulence The turbulence driving structure to initialize
 */
void turbulence_init_backend(struct swift_params* parameter_file,
                             const struct phys_const* phys_const,
                             const struct unit_system* us,
                             const struct space* s,
                             struct turbulence_driving* turbulence) {

  /* make sure the box is a cube */
  if (s->dim[0] != s->dim[1] || s->dim[0] != s->dim[2]) {
    error("Turbulent forcing only works in a cubic box!");
  }

  /* get dimensionless parameters */
  const int random_seed = parser_get_opt_param_int(
      parameter_file, "TurbulentDriving:random_seed", 42);
  const double kmin =
      parser_get_opt_param_double(parameter_file, "TurbulentDriving:kmin", 2.);
  const double kmax =
      parser_get_opt_param_double(parameter_file, "TurbulentDriving:kmax", 3.);
  const double kforcing = parser_get_opt_param_double(
      parameter_file, "TurbulentDriving:kforcing", 2.5);
  const double concentration_factor = parser_get_opt_param_double(
      parameter_file, "TurbulentDriving:concentration_factor", 0.2);

  /* get parameters with units */
  const double power_forcing_cgs = parser_get_opt_param_double(
      parameter_file, "TurbulentDriving:power_forcing_in_cm2_per_s3", 17.);
  const double dtfor_cgs = parser_get_opt_param_double(
      parameter_file, "TurbulentDriving:dt_forcing_in_s", 1.e6);
  const double starting_time_cgs = parser_get_opt_param_double(
      parameter_file, "TurbulentDriving:starting_time_in_s", 0.);

  /* convert units */
  const float forcing_quantity[5] = {0.0f, 2.0f, -3.0f, 0.0f, 0.0f};
  const double uf_in_cgs =
      units_general_cgs_conversion_factor(us, forcing_quantity);
  const double power_forcing = power_forcing_cgs / uf_in_cgs;
  const double ut_in_cgs = units_cgs_conversion_factor(us, UNIT_CONV_TIME);
  const double dtfor = dtfor_cgs / ut_in_cgs;
  const double starting_time = starting_time_cgs / ut_in_cgs;

  /* pre-compute some constants */
  const double Linv = 1. / s->dim[0];
  const double cinv = 1. / (concentration_factor * concentration_factor);

  /* initialise the random number generator */
  turbulence->random_generator = gsl_rng_alloc(gsl_rng_ranlxd2);
  gsl_rng_set(turbulence->random_generator, random_seed);

  /* count the number of k-modes within the forcing shell */
  int num_modes = 0;
  for (double k1 = 0.; k1 <= kmax; k1 += 1.) {
    double k2start;
    if (k1 == 0.) {
      k2start = 0.;
    } else {
      k2start = -kmax;
    }
    for (double k2 = k2start; k2 <= kmax; k2 += 1.) {
      double k3start;
      if (k1 == 0. && k2 == 0.) {
        k3start = 0.;
      } else {
        k3start = -kmax;
      }
      for (double k3 = k3start; k3 <= kmax; k3 += 1.) {
        const double pwrk1 = k1 * k1;
        const double pwrk2 = k2 * k2;
        const double pwrk3 = k3 * k3;
        const double kk = pwrk1 + pwrk2 + pwrk3;
        const double k = sqrt(kk);
        if (k <= kmax && k >= kmin) {
          ++num_modes;
        }
      }
    }
  }

  /* allocate forcing arrays */
  turbulence->k =
      (double*)swift_malloc("turbulent_k", 3 * num_modes * sizeof(double));
  turbulence->amplitudes =
      (double*)swift_malloc("turbulent_A", 6 * num_modes * sizeof(double));
  turbulence->unit_vectors =
      (double*)swift_malloc("turbulent_uv", 6 * num_modes * sizeof(double));
  turbulence->forcing =
      (double*)swift_malloc("turbulent_f", num_modes * sizeof(double));

  /* compute the k-modes, unit vectors and forcing per k-mode */
  int kindex = 0;
  double spectrum_sum = 0.;
  for (double k1 = 0.; k1 <= kmax; k1 += 1.) {
    double k2start;
    if (k1 == 0.) {
      k2start = 0.;
    } else {
      k2start = -kmax;
    }
    for (double k2 = k2start; k2 <= kmax; k2 += 1.) {
      double k3start;
      if (k1 == 0. && k2 == 0.) {
        k3start = 0.;
      } else {
        k3start = -kmax;
      }
      for (double k3 = k3start; k3 <= kmax; k3 += 1.) {
        const double pwrk1 = k1 * k1;
        const double pwrk2 = k2 * k2;
        const double pwrk3 = k3 * k3;
        const double kk = pwrk1 + pwrk2 + pwrk3;
        const double k = sqrt(kk);
        if (k <= kmax && k >= kmin) {
          const double kdiff = k - kforcing;
          const double sqrtk12 = sqrt(pwrk1 + pwrk2);
          const double invkk = 1. / kk;
          const double invk = 1. / k;
          /* alias the unit vector arrays for ease of notation */
          double* u1 = &turbulence->unit_vectors[6 * kindex];
          double* u2 = &turbulence->unit_vectors[6 * kindex + 3];
          if (sqrtk12 > 0.) {
            const double invsqrtk12 = 1. / sqrtk12;
            u1[0] = k2 * invsqrtk12;
            u1[1] = -k1 * invsqrtk12;
            u1[2] = 0.;
            u2[0] = k1 * k3 * invsqrtk12 * invk;
            u2[1] = k2 * k3 * invsqrtk12 * invk;
            u2[2] = -sqrtk12 * invk;
          } else {
            const double sqrtk13 = sqrt(pwrk1 + pwrk3);
            const double invsqrtk13 = 1. / sqrtk13;
            u1[0] = -k3 * invsqrtk13;
            u1[1] = 0.;
            u1[2] = k1 * invsqrtk13;
            u2[0] = k1 * k2 * invsqrtk13 * invk;
            u2[1] = -sqrtk13 * invk;
            u2[2] = k2 * k3 * invsqrtk13 * invk;
          }

          double* kvec = &turbulence->k[3 * kindex];
          kvec[0] = k1 * Linv;
          kvec[1] = k2 * Linv;
          kvec[2] = k3 * Linv;
          const double gaussian_spectrum = exp(-kdiff * kdiff * cinv) * invkk;
          spectrum_sum += gaussian_spectrum;
          turbulence->forcing[kindex] = gaussian_spectrum;
          ++kindex;
        }
      }
    }
  }

  /* normalise the forcing */
  const double norm = power_forcing / (spectrum_sum * dtfor);
  for (int i = 0; i < num_modes; ++i) {
    turbulence->forcing[i] *= norm;
    turbulence->forcing[i] = sqrt(turbulence->forcing[i]);
  }

  /* fast-forward the driving to the desired point in time */
  int num_steps = 0;
  while (num_steps * dtfor < starting_time) {
    /* 3 random numbers are generated per mode in potential_update() */
    for (int i = 0; i < 3 * num_modes; ++i) {
      gsl_rng_uniform(turbulence->random_generator);
    }
    ++num_steps;
  }

  /* initialise the final variables */
  turbulence->number_of_steps = 0;
  turbulence->dt = dtfor;
  turbulence->number_of_modes = num_modes;
}

/**
 * @brief Prints the properties of the turbulence driving to stdout.
 *
 * @param turbulence The turbulence driving structure to print
 */
void turbulence_print_backend(const struct turbulence_driving* turbulence) {

  message("Turbulence driving mode is 'Alvelius'.");
  message("%i modes, dt = %g", turbulence->number_of_modes, turbulence->dt);
}

/**
 * @brief Updates the turbulence driving for the start of the next time step.
 *
 * @param e Engine.
 */
void turbulence_update(struct engine* restrict e) {

  /* Get the end of the next time step. */
  const double time = e->time;
  /* Get the turbulence driving properties used by the engine. */
  struct turbulence_driving* turbulence = e->turbulence;

  /* first, check if we need to do anything */
  if (turbulence->number_of_steps * turbulence->dt < time) {

    /* reset the amplitudes */
    for (int i = 0; i < 6 * turbulence->number_of_modes; ++i) {
      turbulence->amplitudes[i] = 0.;
    }

    /* accumulate contributions to the forcing until we reach the desired point
       in time */
    while (turbulence->number_of_steps * turbulence->dt < time) {
      for (int i = 0; i < turbulence->number_of_modes; ++i) {

        /* generate 3 pseudo-random numbers */
        const double phi =
            2. * M_PI * gsl_rng_uniform(turbulence->random_generator);
        const double theta1 =
            2. * M_PI * gsl_rng_uniform(turbulence->random_generator);
        const double theta2 =
            2. * M_PI * gsl_rng_uniform(turbulence->random_generator);

        /* convert these to random phases */
        const double ga = sin(phi);
        const double gb = cos(phi);
        const double real_rand1 = cos(theta1) * ga;
        const double imag_rand1 = sin(theta1) * ga;
        const double real_rand2 = cos(theta2) * gb;
        const double imag_rand2 = sin(theta2) * gb;

        const double kforce = turbulence->forcing[i];
        /* alias the driving arrays for ease of notation */
        const double* u1 = &turbulence->unit_vectors[6 * i];
        const double* u2 = &turbulence->unit_vectors[6 * i + 3];
        double* Areal = &turbulence->amplitudes[6 * i];
        double* Aimag = &turbulence->amplitudes[6 * i + 3];

        /* update the forcing for this driving time step */
        Areal[0] += kforce * (real_rand1 * u1[0] + real_rand2 * u2[0]);
        Areal[1] += kforce * (real_rand1 * u1[1] + real_rand2 * u2[1]);
        Areal[2] += kforce * (real_rand1 * u1[2] + real_rand2 * u2[2]);
        Aimag[0] += kforce * (imag_rand1 * u1[0] + imag_rand2 * u2[0]);
        Aimag[1] += kforce * (imag_rand1 * u1[1] + imag_rand2 * u2[1]);
        Aimag[2] += kforce * (imag_rand1 * u1[2] + imag_rand2 * u2[2]);
      }
      ++turbulence->number_of_steps;
    }
  }

  /* now accelerate all gas particles using the updated forces */
  const int count = e->s->nr_parts;
  struct part* parts = e->s->parts;
  struct xpart* xparts = e->s->xparts;
  for (int i = 0; i < count; ++i) {
    struct part* p = &parts[i];
    struct xpart* xp = &xparts[i];
    turbulence_accelerate(p, xp, turbulence);
  }
}

/**
 * @brief Accelerate a particle using the turbulent driving forces.
 *
 * @param p Particle.
 * @param xp Extended particle.
 * @param turbulence Turbulence driving properties.
 */
void turbulence_accelerate(struct part* restrict p, struct xpart* restrict xp,
                           const struct turbulence_driving* restrict
                               turbulence) {

  /* alias the particle position for ease of notation */
  const double* x = p->x;

  /* accumulate force contributions for all driving modes */
  double force[3] = {0., 0., 0.};
  for (int ik = 0; ik < turbulence->number_of_modes; ++ik) {

    /* alias the amplitudes and wave number for ease of notation */
    const double* fr = &turbulence->amplitudes[6 * ik];
    const double* fi = &turbulence->amplitudes[6 * ik + 3];
    const double* k = &turbulence->k[3 * ik];

    /* compute the fourier contribution from this wave number */
    const double kdotx = 2. * M_PI * (k[0] * x[0] + k[1] * x[1] + k[2] * x[2]);
    const double cosxyz = cos(kdotx);
    const double sinxyz = sin(kdotx);

    force[0] += fr[0] * cosxyz - fi[0] * sinxyz;
    force[1] += fr[1] * cosxyz - fi[1] * sinxyz;
    force[2] += fr[2] * cosxyz - fi[2] * sinxyz;
  }

  /* update both the velocity and drifted velocity */
  p->v[0] += force[0] * turbulence->dt;
  p->v[1] += force[1] * turbulence->dt;
  p->v[2] += force[2] * turbulence->dt;
  xp->v_full[0] += force[0] * turbulence->dt;
  xp->v_full[1] += force[1] * turbulence->dt;
  xp->v_full[2] += force[2] * turbulence->dt;
}

#else /* TURBULENCE_DRIVING_NONE */

/**
 * @brief Initialises the turbulence driving in the internal system of units.
 *
 * Nothing needs to be done here.
 *
 * @param parameter_file The parsed parameter file
 * @param phys_const Physical constants in internal units
 * @param us The current internal system of units
 * @param turbulence The turbulence driving structure to initialize
 */
void turbulence_init_backend(struct swift_params* parameter_file,
                             const struct phys_const* phys_const,
                             const struct unit_system* us,
                             const struct space* s,
                             struct turbulence_driving* turbulence) {}

/**
 * @brief Prints the properties of the turbulence driving to stdout.
 *
 * Nothing needs to be done here.
 *
 * @param turbulence The turbulence driving structure to print
 */
void turbulence_print_backend(const struct turbulence_driving* turbulence) {}

/**
 * @brief Updates the turbulence driving for the start of the next time step.
 *
 * Nothing needs to be done here.
 *
 * @param e Engine.
 */
void turbulence_update(struct engine* restrict e) {}

/**
 * @brief Accelerate a particle using the turbulent driving forces.
 *
 * Nothing needs to be done here.
 *
 * @param p Particle.
 * @param xp Extended particle.
 * @param turbulence Turbulence driving properties.
 */
void turbulence_accelerate(struct part* restrict p, struct xpart* restrict xp,
                           const struct turbulence_driving* restrict
                               turbulence) {}

#endif /* TURBULENCE_DRIVING */
