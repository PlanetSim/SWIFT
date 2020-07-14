/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2018 Matthieu Schaller (schaller@strw.leidenuniv.nl)
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
#ifndef SWIFT_FEEDBACK_SIMBA_H
#define SWIFT_FEEDBACK_SIMBA_H

#include "active.h"
#include "cosmology.h"
#include "error.h"
#include "feedback_properties.h"
#include "hydro_properties.h"
#include "part.h"
#include "units.h"
#include "random.h"

/**
 * @brief Update the properties of a particle fue to feedback effects after
 * the cooling was applied.
 *
 * Nothing to do here in the SIMBA model.
 *
 * @param p The #part to consider.
 * @param xp The #xpart to consider.
 * @param e The #engine.
 */
__attribute__((always_inline)) INLINE static void feedback_update_part(
    struct part* restrict p, struct xpart* restrict xp,
    const struct engine* restrict e) {}


/**
 * @brief Calculates speed particles will be kicked based on
 * host galaxy properties 
 *
 * @param sp The sparticle doing the feedback
 * @param feedback_props The properties of the feedback model
 */
static inline void compute_kick_speed(struct xpart *xp, const struct feedback_props *feedback_props, const struct cosmology *cosmo) {

  /* Calculate circular velocity based on Baryonic Tully-Fisher relation*/
  const float v_circ = pow(xp->feedback_data.host_galaxy_mass_baryons/feedback_props->simba_host_galaxy_mass_norm, feedback_props->simba_v_circ_exp);

  /* Calculate wind speed */
  xp->feedback_data.v_kick = feedback_props->galsf_firevel 
      * pow(v_circ * cosmo->a /feedback_props->scale_factor_norm,feedback_props->galsf_firevel_slope) 
      //* pow(feedback_props->scale_factor_norm,0.12 - feedback_props->galsf_firevel_slope) // this term seems to be just =1 since exponent is zero
      //* (1. - feedback_props->vwvf_scatter - 2.*feedback_props->vwvf_scatter*random_num)  // check if this randomness is necessary
      * v_circ;
  

}

/**
 * @brief Calculates speed particles will be kicked based on
 * host galaxy properties 
 *
 * @param sp The sparticle doing the feedback
 * @param feedback_props The properties of the feedback model
 */
static inline void compute_mass_loading(struct xpart *xp, const struct feedback_props *feedback_props) {
  // ALEXEI: temporary definition for debugging. Move elsewhere
  const double msun = 1.e-10;

  float galaxy_stellar_mass_msun = xp->feedback_data.host_galaxy_mass_stars / msun;
  float star_mass = xp->sf_data.star_mass_formed;
  if (galaxy_stellar_mass_msun < feedback_props->simba_mass_spectrum_break_msun) {
    xp->feedback_data.wind_mass = feedback_props->simba_wind_mass_eta 
        * star_mass * pow(galaxy_stellar_mass_msun/feedback_props->simba_mass_spectrum_break_msun,feedback_props->simba_low_mass_power);
  } else {
    xp->feedback_data.wind_mass = feedback_props->simba_wind_mass_eta 
        * star_mass * pow(galaxy_stellar_mass_msun/feedback_props->simba_mass_spectrum_break_msun,feedback_props->simba_high_mass_power);
  }
};

/**
 * @brief Calculates amount of extra thermal energy injection required to
 * make up difference between energy injected as wind and total energy
 * injected due to SN.  
 *
 * @param sp The sparticle doing the feedback
 * @param feedback_props The properties of the feedback model
 */
static inline void compute_heating(struct part *p, struct xpart *xp, const struct feedback_props *feedback_props) {
  /* Calculate the amount of energy injected in the wind. 
   * This is in terms of internal energy because we don't 
   * know the mass of the particle being kicked yet. */
  float u_wind = 0.5*xp->feedback_data.v_kick*xp->feedback_data.v_kick;

  /* Calculate internal energy contribution from SN */
  float u_SN = feedback_props->SN_energy * (0.01020788/1.989e33) * xp->sf_data.star_mass_formed 
               / xp->feedback_data.wind_mass;

  if (p->chemistry_data.metal_mass_fraction[0] < 1.e-9) {
    // Schaerer 2003
    u_SN *= exp10(-0.0029*pow(log10(p->chemistry_data.metal_mass_fraction[0])+9,2.5)+0.417694); 
  } else {
    // As above but at zero metallicity
    u_SN *= 2.61634;
  }

  if (u_wind > u_SN * feedback_props->simba_wind_energy_limit) {
    xp->feedback_data.v_kick *= sqrt(feedback_props->simba_wind_energy_limit*u_SN/u_wind);
  }
  if (feedback_props->simba_wind_energy_limit < 1.f) {
    u_SN *= feedback_props->simba_wind_energy_limit;
  }

  /* Now we can decide if there's any energy left over to distribute */
  xp->feedback_data.u_extra = max(u_SN - u_wind, 0.);
};

/**
 * @brief Prepares a s-particle for its feedback interactions
 *
 * @param sp The particle to act upon
 */
__attribute__((always_inline)) INLINE static void feedback_init_spart(
    struct spart* sp) {}

/**
 * @brief Should we do feedback for this star?
 *
 * @param sp The star to consider.
 */
__attribute__((always_inline)) INLINE static int feedback_do_feedback(
    const struct spart* sp) {

  return (sp->birth_time != -1.);
}

/**
 * @brief Should this particle be doing any feedback-related operation?
 *
 * @param sp The #spart.
 * @param time The current simulation time (Non-cosmological runs).
 * @param cosmo The cosmological model (cosmological runs).
 * @param with_cosmology Are we doing a cosmological run?
 */
__attribute__((always_inline)) INLINE static int feedback_is_active(
    const struct spart* sp, const float time, const struct cosmology* cosmo,
    const int with_cosmology) {

  return 1;
}

/**
 * @brief Returns the length of time since the particle last did
 * enrichment/feedback. In SIMBA default to zero because the star particles
 * themselves don't do any feedback
 *
 * @param sp The #spart.
 * @param with_cosmology Are we running with cosmological time integration on?
 * @param cosmo The cosmological model.
 * @param time The current time (since the Big Bang / start of the run) in
 * internal units.
 * @param dt_star the length of this particle's time-step in internal units.
 * @return The length of the enrichment step in internal units.
 */
INLINE static double feedback_get_enrichment_timestep(
    const struct spart* sp, const int with_cosmology,
    const struct cosmology* cosmo, const double time, const double dt_star) {

  return 0;
}


/**
 * @brief Prepares a star's feedback field before computing what
 * needs to be distributed.
 */
__attribute__((always_inline)) INLINE static void feedback_reset_feedback(
    struct spart* sp, const struct feedback_props* feedback_props) {}

/**
 * @brief Initialises the s-particles feedback props for the first time
 *
 * This function is called only once just after the ICs have been
 * read in to do some conversions.
 *
 * @param sp The particle to act upon.
 * @param feedback_props The properties of the feedback model.
 */
__attribute__((always_inline)) INLINE static void feedback_first_init_spart(
    struct spart* sp, const struct feedback_props* feedback_props) {
  sp->feedback_data.to_distribute.simba_delay_time = feedback_props->simba_delay_time;
}

/**
 * @brief Initialises the s-particles feedback props for the first time
 *
 * This function is called only once just after the ICs have been
 * read in to do some conversions.
 *
 * @param sp The particle to act upon.
 * @param feedback_props The properties of the feedback model.
 */
__attribute__((always_inline)) INLINE static void feedback_prepare_spart(
    struct spart* sp, const struct feedback_props* feedback_props) {}

/**
 * @brief Evolve the stellar properties of a #spart.
 *
 * This function allows for example to compute the SN rate before sending
 * this information to a different MPI rank.
 *
 * @param sp The particle to act upon
 * @param feedback_props The #feedback_props structure.
 * @param cosmo The current cosmological model.
 * @param us The unit system.
 * @param star_age_beg_step The age of the star at the star of the time-step in
 * internal units.
 * @param dt The time-step size of this star in internal units.
 */
__attribute__((always_inline)) INLINE static void feedback_evolve_spart(
    struct spart* restrict sp, const struct feedback_props* feedback_props,
    const struct cosmology* cosmo, const struct unit_system* us,
    const struct phys_const* phys_const, const double star_age_beg_step,
    const double dt, const double time, const integertime_t ti_begin,
    const int with_cosmology) {}

/**
 * @brief If gas particle isn't transformed into a star, kick the particle
 * based on quantities for feedback. 
 *
 * @param p Gas particle of interest
 * @param xp Corresponding xpart
 * @param cosmo Cosmology data structure
 * @param ti_current current integer time
 */
__attribute__((always_inline)) INLINE static void launch_wind(
    struct part* restrict p, struct xpart* restrict xp, 
    struct cell* c,
    const struct feedback_props* feedback_props,
    const struct cosmology* cosmo, const integertime_t ti_current){

  if (ti_current == 0 || part_is_decoupled(p)) return;

  /* Determine direction to kick particle (v cross a_grav) */
  float v_new[3];
  v_new[0] = xp->a_grav[1] * p->v[2] - xp->a_grav[2] * p->v[1];
  v_new[1] = xp->a_grav[2] * p->v[0] - xp->a_grav[0] * p->v[2];
  v_new[2] = xp->a_grav[0] * p->v[1] - xp->a_grav[1] * p->v[0];

  /* Randomise +- direction of above cross product to not get all winds going in one direction out of galaxy*/
  // ALEXEI: come up with better choice for random number
  const double random_number =
      random_unit_interval(p->id, ti_current, random_number_stellar_feedback_1);
  if (((int) (10e7*random_number)) % 2) {
    for (int i = 0; i < 3; i++) v_new[i] *= -1;
  }

  /* Now normalise and multiply by the kick velocity */
  float v_new_norm = sqrt(v_new[0]*v_new[0] + v_new[1]*v_new[1] + v_new[2]*v_new[2]);
  /* If for some reason the norm is zero, arbitrarily choose a direction */
  if (v_new_norm == 0) {
    v_new_norm = 1.;
    v_new[0] = 1.;
    v_new[1] = 0.;
    v_new[2] = 0.;
  }
  for (int i = 0; i < 3; i++) v_new[i] = v_new[i]*xp->feedback_data.v_kick/v_new_norm + p->v[i]; 

  /* Set the velocity */
  hydro_set_velocity(p, xp, v_new);

  /* Heat particle */
  // Come up with better random number seed
  const float prob_heat = 0.3;
  if (random_number > prob_heat) {
    const float u_init = hydro_get_physical_internal_energy(p, xp, cosmo);
    const float u_new = u_init + xp->feedback_data.u_extra;
    hydro_set_physical_internal_energy(p, xp, cosmo, u_new);
    hydro_set_drifted_physical_internal_energy(p, cosmo, u_new);
  }

  /* Set delaytime before which the particle cannot interact */
  p->delay_time = feedback_props->simba_delay_time;
  p->time_bin = time_bin_decoupled;

  /* Increment cell counter of decoupled particles */
  c->hydro.nparts_decoupled++;

#ifdef SWIFT_DEBUG_CHECKS
  p->ti_decoupled = ti_current;
#endif
}

__attribute__((always_inline)) INLINE static void star_formation_feedback(
    struct part* restrict p, struct xpart* restrict xp, 
    struct cell* c,
    const struct cosmology* cosmo,
    const struct feedback_props* feedback_props, 
    const integertime_t ti_current) {
  
  /* Calculate the velocity to kick neighbouring particles with */
  compute_kick_speed(xp, feedback_props, cosmo);

  /* Compute wind mass loading */
  compute_mass_loading(xp, feedback_props);

  /* Compute residual heating */
  compute_heating(p, xp, feedback_props);

  /* Launch wind */
  /* Get a unique random number between 0 and 1 for star formation */
  // ALEXEI: seems like we're getting too much feedback, so added an extra 0.1 factor to the probability calculation
  const double prob_launch = (1. - exp(-xp->feedback_data.wind_mass/hydro_get_mass(p)))*0.1;
  const double random_number =
      random_unit_interval(p->id, ti_current, random_number_stellar_feedback_2);
  if (random_number < prob_launch) {
    launch_wind(p, xp, c, feedback_props, cosmo, ti_current);
  }

}


/**
 * @brief Write a feedback struct to the given FILE as a stream of bytes.
 *
 * @param feedback the struct
 * @param stream the file stream
 */
static INLINE void feedback_struct_dump(const struct feedback_props* feedback,
                                        FILE* stream) {}

/**
 * @brief Restore a hydro_props struct from the given FILE as a stream of
 * bytes.
 *
 * @param feedback the struct
 * @param stream the file stream
 * @param cosmo #cosmology structure
 */
static INLINE void feedback_struct_restore(struct feedback_props* feedback,
                                           FILE* stream) {}

/**
 * @brief Will this star particle want to do feedback during the next time-step?
 *
 * Nothing to do here in the SIMBA model
 *
 * @param sp The star of interest.
 * @param feedback_props The properties of the feedback model.
 * @param with_cosmology Are we running with cosmological time integration?
 * @param cosmo The #cosmology object.
 * @param time The current time (since the Big Bang).
 */
__attribute__((always_inline)) INLINE static int feedback_will_do_feedback(
    struct spart* restrict sp, const struct feedback_props* feedback_props,
    const int with_cosmology, const struct cosmology* cosmo,
    const double time) {
  return 1;
}

/** 
 * @brief Checks whether particle should be recoupled
 * 
 * Nothing to do here in EAGLE model
 * 
 * @param p particle to check
 * @param feedback The properties of the feedback scheme
 */
INLINE static int feedback_is_recoupling(const struct part* p, const struct feedback_props* feedback) {
  return (p->delay_time < 0. || p->rho > feedback->recoupling_density);
}

/**
 * @brief Clean-up the memory allocated for the feedback routines
 *
 * We simply free all the arrays.
 *
 * @param feedback_props the feedback data structure.
 */
static INLINE void feedback_clean(struct feedback_props* feedback_props) {}

#ifdef HAVE_HDF5
/**
 * @brief Writes the current model of feedback to the file
 *
 * @param feedback The properties of the feedback scheme.
 * @param h_grp The HDF5 group in which to write.
 */
INLINE static void feedback_write_flavour(struct feedback_props* feedback,
                                          hid_t h_grp) {

  io_write_attribute_s(h_grp, "Feedback Model", "EAGLE");
}
#endif  // HAVE_HDF5


#endif /* SWIFT_FEEDBACK_SIMBA_H */
