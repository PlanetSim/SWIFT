/*******************************************************************************
 * This file is part of SWIFT.
 * Coypright (c) 2016 Matthieu Schaller (matthieu.schaller@durham.ac.uk)
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
#ifndef SWIFT_DEFAULT_HYDRO_IO_H
#define SWIFT_DEFAULT_HYDRO_IO_H

#include "adiabatic_index.h"
#include "hydro.h"
#include "io_properties.h"
#include "kernel_hydro.h"
#include "config.h"

/**
 * @brief Specifies which particle fields to read from a dataset
 *
 * @param parts The particle array.
 * @param list The list of i/o properties to read.
 * @param num_fields The number of i/o fields to read.
 */
INLINE static void hydro_read_particles(struct part* parts,
                                        struct io_props* list,
                                        int* num_fields) {

#if !defined(EOS_MULTIFLUID_TAIT)
  *num_fields = 11;
#else
  *num_fields = 12;
#endif

  /* List what we want to read */
  list[0] = io_make_input_field("Coordinates", DOUBLE, 3, COMPULSORY,
                                UNIT_CONV_LENGTH, parts, x);
  list[1] = io_make_input_field("Velocities", FLOAT, 3, COMPULSORY,
                                UNIT_CONV_SPEED, parts, v);
  list[2] = io_make_input_field("Masses", FLOAT, 1, COMPULSORY, UNIT_CONV_MASS,
                                parts, mass);
  list[3] = io_make_input_field("SmoothingLength", FLOAT, 1, COMPULSORY,
                                UNIT_CONV_LENGTH, parts, h);
  list[4] = io_make_input_field("ParticleIDs", ULONGLONG, 1, COMPULSORY,
                                UNIT_CONV_NO_UNITS, parts, id);
  list[5] = io_make_input_field("Accelerations", FLOAT, 3, OPTIONAL,
                                UNIT_CONV_ACCELERATION, parts, a_hydro);
  list[6] = io_make_input_field("Density", DOUBLE/*FLOAT*/, 1, COMPULSORY,
                                UNIT_CONV_DENSITY, parts, rho);
  list[7] = io_make_input_field("ConstantAcceleration", FLOAT, 3, OPTIONAL,
                                UNIT_CONV_ACCELERATION, parts, a_constant);
  list[8] = io_make_input_field("IsBoundary", INT, 1, OPTIONAL,
                                 UNIT_CONV_NO_UNITS, parts, is_boundary);
  list[9] = io_make_input_field("Velocities", FLOAT, 3, COMPULSORY,
                                 UNIT_CONV_SPEED, parts, v_minus1);
  list[10] = io_make_input_field("Viscosity", FLOAT, 1, COMPULSORY,
                                 UNIT_CONV_KINEMATIC_VISCOSITY, parts, viscosity);
#if defined(EOS_MULTIFLUID_TAIT)
  list[11] = io_make_input_field("ReferenceDensity", FLOAT, 1, COMPULSORY,
                                  UNIT_CONV_DENSITY, parts, rho_base);
#endif
}
INLINE static void convert_S(const struct engine* e, const struct part* p,
                             const struct xpart* xp, float* ret) {

  ret[0] = 0.0;
}

INLINE static void convert_P(const struct engine* e, const struct part* p,
                             const struct xpart* xp, float* ret) {

  ret[0] = p->pressure;
}

INLINE static void convert_part_pos(const struct engine* e,
                                    const struct part* p,
                                    const struct xpart* xp, double* ret) {

  if (e->s->periodic) {
    ret[0] = box_wrap(p->x[0], 0.0, e->s->dim[0]);
    ret[1] = box_wrap(p->x[1], 0.0, e->s->dim[1]);
    ret[2] = box_wrap(p->x[2], 0.0, e->s->dim[2]);
  } else {
    ret[0] = p->x[0];
    ret[1] = p->x[1];
    ret[2] = p->x[2];
  }
}

INLINE static void convert_part_vel(const struct engine* e,
                                    const struct part* p,
                                    const struct xpart* xp, float* ret) {

  const int with_cosmology = (e->policy & engine_policy_cosmology);
  const struct cosmology* cosmo = e->cosmology;
  const integertime_t ti_current = e->ti_current;
  const double time_base = e->time_base;

  const integertime_t ti_beg = get_integer_time_begin(ti_current, p->time_bin);
  const integertime_t ti_end = get_integer_time_end(ti_current, p->time_bin);

  /* Get time-step since the last kick */
  float dt_kick_grav, dt_kick_hydro;
  if (with_cosmology) {
    dt_kick_grav = cosmology_get_grav_kick_factor(cosmo, ti_beg, ti_current);
    dt_kick_grav -=
        cosmology_get_grav_kick_factor(cosmo, ti_beg, (ti_beg + ti_end) / 2);
    dt_kick_hydro = cosmology_get_hydro_kick_factor(cosmo, ti_beg, ti_current);
    dt_kick_hydro -=
        cosmology_get_hydro_kick_factor(cosmo, ti_beg, (ti_beg + ti_end) / 2);
  } else {
    dt_kick_grav = (ti_current - ((ti_beg + ti_end) / 2)) * time_base;
    dt_kick_hydro = (ti_current - ((ti_beg + ti_end) / 2)) * time_base;
  }

  /* Extrapolate the velocites to the current time */
  hydro_get_drifted_velocities(p, xp, dt_kick_hydro, dt_kick_grav, ret);

  /* Conversion from internal units to peculiar velocities */
  ret[0] *= cosmo->a_inv;
  ret[1] *= cosmo->a_inv;
  ret[2] *= cosmo->a_inv;
}

INLINE static void convert_part_potential(const struct engine* e,
                                          const struct part* p,
                                          const struct xpart* xp, float* ret) {

  if (p->gpart != NULL)
    ret[0] = gravity_get_comoving_potential(p->gpart);
  else
    ret[0] = 0.f;
}

/**
 * @brief Specifies which particle fields to write to a dataset
 *
 * @param parts The particle array.
 * @param list The list of i/o properties to write.
 * @param num_fields The number of i/o fields to write.
 */
INLINE static void hydro_write_particles(const struct part* parts,
                                         const struct xpart* xparts,
                                         struct io_props* list,
                                         int* num_fields) {

  *num_fields = 11;

  /* List what we want to write */
  list[0] = io_make_output_field_convert_part("Coordinates", DOUBLE, 3,
                                              UNIT_CONV_LENGTH, 0.f, parts, xparts,
                                              convert_part_pos, "coords");
  list[1] = io_make_output_field_convert_part(
      "Velocities", FLOAT, 3, UNIT_CONV_SPEED,0.f, parts, xparts, convert_part_vel, "vels");
  list[2] =
      io_make_output_field("Masses", FLOAT, 1, UNIT_CONV_MASS, 0.f,parts, mass, "mass");
  list[3] = io_make_output_field("SmoothingLength", FLOAT, 1, UNIT_CONV_LENGTH, 0.f,
                                 parts, h, "smoothing length");
  list[4] = io_make_output_field("InternalEnergy", FLOAT, 1,
                                 UNIT_CONV_ENERGY_PER_UNIT_MASS,0.f, parts, u, "N/A");
  list[5] = io_make_output_field("ParticleIDs", ULONGLONG, 1,
                                 UNIT_CONV_NO_UNITS, 0.f,parts, id, "id");
  list[6] =
      io_make_output_field("Density", DOUBLE/*FLOAT*/, 1, UNIT_CONV_DENSITY,0.f, parts, rho,"density");
  list[7] = io_make_output_field(
      "Pressure", FLOAT, 1, UNIT_CONV_PRESSURE,0.f, parts, pressure, "pressure");

  list[8] = io_make_output_field("ConstantAcceleration", DOUBLE, 3,
                                               UNIT_CONV_ACCELERATION,0.f, parts, a_constant, "a_constant");
  list[9] = io_make_output_field("IsBoundary", INT, 1, UNIT_CONV_NO_UNITS,0.f, parts, is_boundary, "is boundary");
  list[10] = io_make_output_field("Acceleration", FLOAT, 3, UNIT_CONV_ACCELERATION,0.f, parts, a_hydro, "Acceleration");
                                             
}

/**
 * @brief Writes the current model of SPH to the file
 * @param h_grpsph The HDF5 group in which to write
 */
INLINE static void hydro_write_flavour(hid_t h_grpsph) {

  /* Viscosity and thermal conduction */
  io_write_attribute_s(h_grpsph, "Thermal Conductivity Model",
                       "Price (2008) without switch");
/*  io_write_attribute_f(h_grpsph, "Thermal Conductivity alpha",
                       const_conductivity_alpha);
  io_write_attribute_s(
      h_grpsph, "Viscosity Model",
      "Morris & Monaghan (1997), Rosswog, Davies, Thielemann & "
      "Piran (2000) with additional Balsara (1995) switch");*/

  /* Time integration properties */
/*  io_write_attribute_f(h_grpsph, "Maximal Delta u change over dt",
                       const_max_u_change);*/
}

/**
 * @brief Are we writing entropy in the internal energy field ?
 *
 * @return 1 if entropy is in 'internal energy', 0 otherwise.
 */
INLINE static int writeEntropyFlag(void) { return 0; }

#endif /* SWIFT_DEFAULT_HYDRO_IO_H */
