/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2016 Matthieu Schaller (matthieu.schaller@durham.ac.uk)
 *               2018 Jacob Kegerreis (jacob.kegerreis@durham.ac.uk).
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
#ifndef SWIFT_PLANETARY_HYDRO_IACT_H
#define SWIFT_PLANETARY_HYDRO_IACT_H

/**
 * @file Planetary/hydro_iact.h
 * @brief Minimal conservative implementation of SPH (Neighbour loop equations)
 *
 * The thermal variable is the internal energy (u). Simple constant
 * viscosity term with the Balsara (1995) switch (optional).
 * No thermal conduction term is implemented.
 *
 * This corresponds to equations (43), (44), (45), (101), (103)  and (104) with
 * \f$\beta=3\f$ and \f$\alpha_u=0\f$ of Price, D., Journal of Computational
 * Physics, 2012, Volume 231, Issue 3, pp. 759-794.
 */

#include "adiabatic_index.h"
#include "const.h"
#include "hydro_parameters.h"
#include "minmax.h"
#include "math.h"

/**
 * @brief Density interaction between two particles.
 *
 * @param r2 Comoving square distance between the two particles.
 * @param dx Comoving vector separating both particles (pi - pj).
 * @param hi Comoving smoothing-length of particle i.
 * @param hj Comoving smoothing-length of particle j.
 * @param pi First particle.
 * @param pj Second particle.
 * @param a Current scale factor.
 * @param H Current Hubble parameter.
 */
__attribute__((always_inline)) INLINE static void runner_iact_density(
    const float r2, const float dx[3], const float hi, const float hj,
    struct part *restrict pi, struct part *restrict pj, const float a,
    const float H) {

  float wi, wj, wi_dx, wj_dx;

#ifdef SWIFT_DEBUG_CHECKS
  if (pi->time_bin >= time_bin_inhibited)
    error("Inhibited pi in interaction function!");
  if (pj->time_bin >= time_bin_inhibited)
    error("Inhibited pj in interaction function!");
#endif

  /* Get r and 1/r. */
  const float r = sqrtf(r2);
  const float r_inv = r ? 1.0f / r : 0.0f;

  /* Get the masses. */
  const float mi = pi->mass;
  const float mj = pj->mass;

  /* Compute density of pi. */
  const float hi_inv = 1.f / hi;
  const float ui = r * hi_inv;
  kernel_deval(ui, &wi, &wi_dx);

  pi->rho += mj * wi;
  pi->density.rho_dh -= mj * (hydro_dimension * wi + ui * wi_dx);
  pi->density.wcount += wi;
  pi->density.wcount_dh -= (hydro_dimension * wi + ui * wi_dx);

  /* Compute density of pj. */
  const float hj_inv = 1.f / hj;
  const float uj = r * hj_inv;
  kernel_deval(uj, &wj, &wj_dx);

  pj->rho += mi * wj;
  pj->density.rho_dh -= mi * (hydro_dimension * wj + uj * wj_dx);
  pj->density.wcount += wj;
  pj->density.wcount_dh -= (hydro_dimension * wj + uj * wj_dx);

  /* Compute dv dot r */
  float dv[3], curlvr[3];

  const float faci = mj * wi_dx * r_inv;
  const float facj = mi * wj_dx * r_inv;

  dv[0] = pi->v[0] - pj->v[0];
  dv[1] = pi->v[1] - pj->v[1];
  dv[2] = pi->v[2] - pj->v[2];
  const float dvdr = dv[0] * dx[0] + dv[1] * dx[1] + dv[2] * dx[2];

  pi->density.div_v -= faci * dvdr;
  pj->density.div_v -= facj * dvdr;

  /* Compute dv cross r */
  curlvr[0] = dv[1] * dx[2] - dv[2] * dx[1];
  curlvr[1] = dv[2] * dx[0] - dv[0] * dx[2];
  curlvr[2] = dv[0] * dx[1] - dv[1] * dx[0];

  pi->density.rot_v[0] += faci * curlvr[0];
  pi->density.rot_v[1] += faci * curlvr[1];
  pi->density.rot_v[2] += faci * curlvr[2];

  pj->density.rot_v[0] += facj * curlvr[0];
  pj->density.rot_v[1] += facj * curlvr[1];
  pj->density.rot_v[2] += facj * curlvr[2];

#ifdef SWIFT_HYDRO_DENSITY_CHECKS
  pi->n_density += wi;
  pj->n_density += wj;
  pi->N_density++;
  pj->N_density++;
#endif

#ifdef PLANETARY_IMBALANCE

  /* Add contribution to kernel averages */
  pi->sum_wij += wi*mj;
  pj->sum_wij += wj*mi;

  /* Add contribution r_ij*mj*sqrt(Wij) */
  if (pi->mat_id == pj->mat_id){
  pi->sum_rij[0] += -dx[0]*wi*mj;
  pi->sum_rij[1] += -dx[1]*wi*mj;
  pi->sum_rij[2] += -dx[2]*wi*mj;

  pj->sum_rij[0] += dx[0]*wj*mi;
  pj->sum_rij[1] += dx[1]*wj*mi;
  pj->sum_rij[2] += dx[2]*wj*mi;
  }

  if (pi->mat_id != pj->mat_id){
  pi->sum_rij[0] += dx[0]*wi*mj;
  pi->sum_rij[1] += dx[1]*wi*mj;
  pi->sum_rij[2] += dx[2]*wi*mj;

  pj->sum_rij[0] += -dx[0]*wj*mi;
  pj->sum_rij[1] += -dx[1]*wj*mi;
  pj->sum_rij[2] += -dx[2]*wj*mi;
  }
#endif
}

/**
 * @brief Density interaction between two particles (non-symmetric).
 *
 * @param r2 Comoving square distance between the two particles.
 * @param dx Comoving vector separating both particles (pi - pj).
 * @param hi Comoving smoothing-length of particle i.
 * @param hj Comoving smoothing-length of particle j.
 * @param pi First particle.
 * @param pj Second particle (not updated).
 * @param a Current scale factor.
 * @param H Current Hubble parameter.
 */
__attribute__((always_inline)) INLINE static void runner_iact_nonsym_density(
    const float r2, const float dx[3], const float hi, const float hj,
    struct part *restrict pi, const struct part *restrict pj, const float a,
    const float H) {

  float wi, wi_dx;

#ifdef SWIFT_DEBUG_CHECKS
  if (pi->time_bin >= time_bin_inhibited)
    error("Inhibited pi in interaction function!");
  if (pj->time_bin >= time_bin_inhibited)
    error("Inhibited pj in interaction function!");
#endif

  /* Get the masses. */
  const float mj = pj->mass;

  /* Get r and 1/r. */
  const float r = sqrtf(r2);
  const float r_inv = r ? 1.0f / r : 0.0f;

  const float h_inv = 1.f / hi;
  const float ui = r * h_inv;
  kernel_deval(ui, &wi, &wi_dx);

  pi->rho += mj * wi;
  pi->density.rho_dh -= mj * (hydro_dimension * wi + ui * wi_dx);
  pi->density.wcount += wi;
  pi->density.wcount_dh -= (hydro_dimension * wi + ui * wi_dx);

  /* Compute dv dot r */
  float dv[3], curlvr[3];

  const float faci = mj * wi_dx * r_inv;

  dv[0] = pi->v[0] - pj->v[0];
  dv[1] = pi->v[1] - pj->v[1];
  dv[2] = pi->v[2] - pj->v[2];
  const float dvdr = dv[0] * dx[0] + dv[1] * dx[1] + dv[2] * dx[2];

  pi->density.div_v -= faci * dvdr;

  /* Compute dv cross r */
  curlvr[0] = dv[1] * dx[2] - dv[2] * dx[1];
  curlvr[1] = dv[2] * dx[0] - dv[0] * dx[2];
  curlvr[2] = dv[0] * dx[1] - dv[1] * dx[0];

  pi->density.rot_v[0] += faci * curlvr[0];
  pi->density.rot_v[1] += faci * curlvr[1];
  pi->density.rot_v[2] += faci * curlvr[2];

#ifdef SWIFT_HYDRO_DENSITY_CHECKS
  pi->n_density += wi;
  pi->N_density++;
#endif

#ifdef PLANETARY_IMBALANCE

  /* Add contribution to kernel averages */
  pi->sum_wij += wi*mj;

  /* Add contribution r_ij*mj*sqrt(Wij) */
  if (pi->mat_id == pj->mat_id){
  pi->sum_rij[0] += -dx[0]*wi*mj;
  pi->sum_rij[1] += -dx[1]*wi*mj;
  pi->sum_rij[2] += -dx[2]*wi*mj;
  }

  if (pi->mat_id != pj->mat_id){
  pi->sum_rij[0] += dx[0]*wi*mj;
  pi->sum_rij[1] += dx[1]*wi*mj;
  pi->sum_rij[2] += dx[2]*wi*mj;
  }
#endif
}

/**
 * @brief Calculate the gradient interaction between particle i and particle j
 *
 * This method wraps around hydro_gradients_collect, which can be an empty
 * method, in which case no gradients are used.
 *
 * @param r2 Comoving square distance between the two particles.
 * @param dx Comoving vector separating both particles (pi - pj).
 * @param hi Comoving smoothing-length of particle i.
 * @param hj Comoving smoothing-length of particle j.
 * @param pi First particle.
 * @param pj Second particle.
 * @param a Current scale factor.
 * @param H Current Hubble parameter.
 */
__attribute__((always_inline)) INLINE static void runner_iact_gradient(
    float r2, const float *dx, float hi, float hj, struct part *restrict pi,
    struct part *restrict pj, float a, float H) {

  float wi, wj, wi_dx, wj_dx;

  /* Get r and 1/r. */
  const float r = sqrtf(r2);
  const float r_inv = r ? 1.0f / r : 0.0f;

  /* Compute kernel of pi. */
  const float hi_inv = 1.f / hi;
  const float ui = r * hi_inv;
  kernel_deval(ui, &wi, &wi_dx);
  
  /* Compute kernel of pj. */
  const float hj_inv = 1.f / hj;
  const float uj = r * hj_inv;
  kernel_deval(uj, &wj, &wj_dx);

  /* Correction factors for kernel gradients */ 
  const float rho_inv_i = 1.f / pi->rho;
  const float rho_inv_j = 1.f / pj->rho;
  
  pi->weighted_wcount += pj->mass * r2 * wi_dx * r_inv;
  pj->weighted_wcount += pi->mass * r2 * wj_dx * r_inv;

  pi->weighted_neighbour_wcount += pj->mass * r2 * wi_dx * rho_inv_j * r_inv;
  pj->weighted_neighbour_wcount += pi->mass * r2 * wj_dx * rho_inv_i * r_inv;

#ifdef PLANETARY_IMBALANCE
  /* Compute kernel averages */
  pi->sum_wij_exp += wi * expf(-pj->I*pj->I);
  pi->sum_wij_exp_P += pj->P * wi * expf(-pj->I*pj->I);
  pi->sum_wij_exp_T += pj->T * wi * expf(-pj->I*pj->I);

  pj->sum_wij_exp += wj * expf(-pi->I*pi->I);
  pj->sum_wij_exp_P += pi->P * wj * expf(-pi->I*pi->I);
  pj->sum_wij_exp_T += pi->T * wj * expf(-pi->I*pi->I);
#endif
    
      
    const float hid_inv = pow_dimension_plus_one(hi_inv); /* 1/h^(d+1) */
    const float hjd_inv = pow_dimension_plus_one(hj_inv); /* 1/h^(d+1) */
   
    pi->Cinv[0][0] += pj->mass * dx[0] * dx[0] * wi * hid_inv * rho_inv_j;
    pi->Cinv[0][1] += pj->mass * dx[0] * dx[1] * wi * hid_inv   * rho_inv_j;
    pi->Cinv[0][2] += pj->mass * dx[0] * dx[2] * wi * hid_inv   * rho_inv_j;
    pi->Cinv[1][0] += pj->mass * dx[1] * dx[0] * wi * hid_inv   * rho_inv_j;
    pi->Cinv[1][1] += pj->mass * dx[1] * dx[1] * wi * hid_inv   * rho_inv_j;
    pi->Cinv[1][2] += pj->mass * dx[1] * dx[2] * wi * hid_inv   * rho_inv_j;
    pi->Cinv[2][0] += pj->mass * dx[2] * dx[0] * wi * hid_inv   * rho_inv_j;
    pi->Cinv[2][1] += pj->mass * dx[2] * dx[1] * wi * hid_inv   * rho_inv_j;
    pi->Cinv[2][2] += pj->mass * dx[2] * dx[2] * wi * hid_inv   * rho_inv_j;
        
    pj->Cinv[0][0] += pi->mass * dx[0] * dx[0] * wj * hjd_inv  * rho_inv_i;
    pj->Cinv[0][1] += pi->mass * dx[0] * dx[1] * wj * hjd_inv  * rho_inv_i;
    pj->Cinv[0][2] += pi->mass * dx[0] * dx[2] * wj * hjd_inv  * rho_inv_i;
    pj->Cinv[1][0] += pi->mass * dx[1] * dx[0] * wj * hjd_inv  * rho_inv_i;
    pj->Cinv[1][1] += pi->mass * dx[1] * dx[1] * wj * hjd_inv  * rho_inv_i;
    pj->Cinv[1][2] += pi->mass * dx[1] * dx[2] * wj * hjd_inv  * rho_inv_i;
    pj->Cinv[2][0] += pi->mass * dx[2] * dx[0] * wj * hjd_inv  * rho_inv_i;
    pj->Cinv[2][1] += pi->mass * dx[2] * dx[1] * wj * hjd_inv  * rho_inv_i;
    pj->Cinv[2][2] += pi->mass * dx[2] * dx[2] * wj * hjd_inv  * rho_inv_i;
    
    
    
        #if defined(HYDRO_DIMENSION_2D)

    pi->Cinv[2][2] = 1.f;
    pj->Cinv[2][2] = 1.f;

    #elif defined(HYDRO_DIMENSION_1D)
    error("The dimension is not defined !");
    #endif
}

/**
 * @brief Calculate the gradient interaction between particle i and particle j:
 * non-symmetric version
 *
 * This method wraps around hydro_gradients_nonsym_collect, which can be an
 * empty method, in which case no gradients are used.
 *
 * @param r2 Comoving square distance between the two particles.
 * @param dx Comoving vector separating both particles (pi - pj).
 * @param hi Comoving smoothing-length of particle i.
 * @param hj Comoving smoothing-length of particle j.
 * @param pi First particle.
 * @param pj Second particle (not updated).
 * @param a Current scale factor.
 * @param H Current Hubble parameter.
 */
__attribute__((always_inline)) INLINE static void runner_iact_nonsym_gradient(
    float r2, const float *dx, float hi, float hj, struct part *restrict pi,
    const struct part *restrict pj, float a, float H) {

  float wi, wi_dx;

  /* Get r and 1/r. */
  const float r = sqrtf(r2);
  const float r_inv = r ? 1.0f / r : 0.0f;

  /* Compute kernel of pi. */
  const float h_inv = 1.f / hi;
  const float ui = r * h_inv;
  kernel_deval(ui, &wi, &wi_dx);

  /* Correction factors for kernel gradients */ 

  const float rho_inv_j = 1.f / pj->rho;

  pi->weighted_wcount += pj->mass * r2 * wi_dx * r_inv;
  pi->weighted_neighbour_wcount += pj->mass * r2 * wi_dx * rho_inv_j * r_inv;

#ifdef PLANETARY_IMBALANCE
  /* Compute kernel averages */
  pi->sum_wij_exp += wi * expf(-pj->I*pj->I);
  pi->sum_wij_exp_P += pj->P * wi * expf(-pj->I*pj->I);
  pi->sum_wij_exp_T += pj->T * wi * expf(-pj->I*pj->I);
#endif
    
    
    const float hid_inv = pow_dimension_plus_one(h_inv); /* 1/h^(d+1) */
    
    pi->Cinv[0][0] += pj->mass * dx[0] * dx[0] * wi * hid_inv  * rho_inv_j;
    pi->Cinv[0][1] += pj->mass * dx[0] * dx[1] * wi * hid_inv  * rho_inv_j;
    pi->Cinv[0][2] += pj->mass * dx[0] * dx[2] * wi * hid_inv  * rho_inv_j;
    pi->Cinv[1][0] += pj->mass * dx[1] * dx[0] * wi * hid_inv  * rho_inv_j;
    pi->Cinv[1][1] += pj->mass * dx[1] * dx[1] * wi * hid_inv  * rho_inv_j;
    pi->Cinv[1][2] += pj->mass * dx[1] * dx[2] * wi * hid_inv  * rho_inv_j;
    pi->Cinv[2][0] += pj->mass * dx[2] * dx[0] * wi * hid_inv  * rho_inv_j;
    pi->Cinv[2][1] += pj->mass * dx[2] * dx[1] * wi * hid_inv  * rho_inv_j;
    pi->Cinv[2][2] += pj->mass * dx[2] * dx[2] * wi * hid_inv  * rho_inv_j;
    
    
    #if defined(HYDRO_DIMENSION_2D)

    pi->Cinv[2][2] = 1.f;

    #elif defined(HYDRO_DIMENSION_1D)
    error("The dimension is not defined !");
    #endif
  
}

/**
 * @brief Force interaction between two particles.
 *
 * @param r2 Comoving square distance between the two particles.
 * @param dx Comoving vector separating both particles (pi - pj).
 * @param hi Comoving smoothing-length of particle i.
 * @param hj Comoving smoothing-length of particle j.
 * @param pi First particle.
 * @param pj Second particle.
 * @param a Current scale factor.
 * @param H Current Hubble parameter.
 */
__attribute__((always_inline)) INLINE static void runner_iact_force(
    const float r2, const float dx[3], const float hi, const float hj,
    struct part *restrict pi, struct part *restrict pj, const float a,
    const float H) {

#ifdef SWIFT_DEBUG_CHECKS
  if (pi->time_bin >= time_bin_inhibited)
    error("Inhibited pi in interaction function!");
  if (pj->time_bin >= time_bin_inhibited)
    error("Inhibited pj in interaction function!");
#endif

  /* Cosmological factors entering the EoMs */
  const float fac_mu = pow_three_gamma_minus_five_over_two(a);
  const float a2_Hubble = a * a * H;

  /* Get r and 1/r. */
  const float r = sqrtf(r2);
  const float r_inv = r ? 1.0f / r : 0.0f;

  /* Recover some data */
  const float mi = pi->mass;
  const float mj = pj->mass;
  const float rhoi = pi->rho;
  const float rhoj = pj->rho;
  const float pressurei = pi->force.pressure;
  const float pressurej = pj->force.pressure;

  /* Get the kernel for hi. */
  const float hi_inv = 1.0f / hi;
  const float xi = r * hi_inv;
  float wi, wi_dx;
  kernel_deval(xi, &wi, &wi_dx);

  /* Get the kernel for hj. */
  const float hj_inv = 1.0f / hj;
  const float xj = r * hj_inv;
  float wj, wj_dx;
  kernel_deval(xj, &wj, &wj_dx);
    
    
    
    const float hid_inv = pow_dimension_plus_one(hi_inv); /* 1/h^(d+1) */
    const float hjd_inv = pow_dimension_plus_one(hj_inv); /* 1/h^(d+1) */
    
  float Gj[3], Gi[3];  

    int k;
    for(k=0;k<3;k++){
        Gi[k] = -(pi->C[k][0] * dx[0] + pi->C[k][1] * dx[1] + pi->C[k][2] * dx[2]) * wi * hid_inv;//wi_dr * dx[k] * r_inv;//
        Gj[k] = -(pj->C[k][0] * dx[0] + pj->C[k][1] * dx[1] + pj->C[k][2] * dx[2]) * wj * hjd_inv;//wj_dr * dx[k] * r_inv;//
    }

 float kernel_gradient[3];
    kernel_gradient[0] = 0.5f * (Gi[0] + Gj[0]);//0.5f * (Gi[0] * pi->f_gdf + Gj[0] * pj->f_gdf);//
    kernel_gradient[1] = 0.5f * (Gi[1] + Gj[1]);//0.5f * (Gi[1] * pi->f_gdf + Gj[1] * pj->f_gdf);//
    kernel_gradient[2] = 0.5f * (Gi[2]  + Gj[2]);//0.5f * (Gi[2] * pi->f_gdf + Gj[2] * pj->f_gdf);//
    
    
    
              // Compute dv dot G. 
    const float dvdGi = (pi->v[0] - pj->v[0]) * Gi[0] +
                    (pi->v[1] - pj->v[1]) * Gi[1] +
                    (pi->v[2] - pj->v[2]) * Gi[2];
      
    
    const float dvdGj = (pj->v[0] - pi->v[0]) * Gj[0] +
                     (pj->v[1] - pi->v[1]) * Gj[1] +
                    (pj->v[2] - pi->v[2]) * Gj[2];
    
    
        const float dvdGij = (pi->v[0] - pj->v[0]) * kernel_gradient[0] +
                     (pi->v[1] - pj->v[1]) * kernel_gradient[1] +
                     (pi->v[2] - pj->v[2]) * kernel_gradient[2];

  /* Compute dv dot r. */
  const float dvdr = (pi->v[0] - pj->v[0]) * dx[0] +
                     (pi->v[1] - pj->v[1]) * dx[1] +
                     (pi->v[2] - pj->v[2]) * dx[2] + a2_Hubble * r2;

  /* Balsara term */
  const float balsara_i = pi->force.balsara;
  const float balsara_j = pj->force.balsara;

  /* Are the particles moving towards each other? */
  const float omega_ij = min(dvdr, 0.f);
  const float mu_ij = fac_mu * r_inv * omega_ij; /* This is 0 or negative */

  /* Compute sound speeds and signal velocity */
  const float ci = pi->force.soundspeed;
  const float cj = pj->force.soundspeed;
  const float v_sig = ci + cj - const_viscosity_beta * mu_ij;

  /* Now construct the full viscosity term */
  const float rho_ij = 0.5f * (rhoi + rhoj);
  const float visc = -0.25f * v_sig * mu_ij * (balsara_i + balsara_j) / rho_ij;

  /* Convolve with the kernel */
  const float visc_acc_term = visc;

  /* SPH acceleration term */
  const float sph_acc_term =
      (pressurei + pressurej) / (pi->rho * pj->rho);//(pressurei + Q_i + pressurej + Q_j) / (pi->rho * pj->rho);

  /* Assemble the acceleration */
  const float acc = sph_acc_term + visc_acc_term;

  /* Use the force Luke ! */
  pi->a_hydro[0] -= mj * acc * kernel_gradient[0];
  pi->a_hydro[1] -= mj * acc * kernel_gradient[1];
  pi->a_hydro[2] -= mj * acc * kernel_gradient[2];

  pj->a_hydro[0] += mi * acc * kernel_gradient[0];
  pj->a_hydro[1] += mi * acc * kernel_gradient[1];
  pj->a_hydro[2] += mi * acc * kernel_gradient[2];

  /* Get the time derivative for u. */
  const float sph_du_term_i =
      (pressurei) * dvdGij / (rhoi * rhoj);//(pressurei + Q_i) * dvdGij / (rhoi * rhoj);
  const float sph_du_term_j =
      (pressurej) * dvdGij / (rhoi * rhoj);//(pressurej + Q_j) * dvdGij / (rhoi * rhoj);

  /*if (abs(sph_du_term_i*mj) > 1E+05 && pi->mat_id == 100){
  printf(
    "du_term_i: %.7g\n"
    "dvdr, r_inv, kernel: %.7g, %.7g, %.7g\n"
    "u_i, P_i, rho_i, mat_id_i, u_j, P_j, rho_j, mat_id_j:\n"
    "%.7g, %.7g, %.7g %d, %.7g, %.7g, %.7g, %d\n",
    sph_du_term_i*mj, dvdr, r_inv, kernel_gradient,
    pi->u, pressurei, rhoi, pi->mat_id,
    pj->u, pressurej, rhoj, pj->mat_id);

  }*/

  /* Viscosity term */
  const float visc_du_term = 0.5f * visc_acc_term * dvdGij;

  /* Assemble the energy equation term */
  const float du_dt_i = sph_du_term_i + visc_du_term;
  const float du_dt_j = sph_du_term_j + visc_du_term;

  /* Internal energy time derivative */
  pi->u_dt += du_dt_i * mj;
  pj->u_dt += du_dt_j * mi;

  /* Get the time derivative for h. */
  pi->force.h_dt -= mj * dvdGi / rhoj;
  pj->force.h_dt -= -mi * dvdGj / rhoi;

  /* Update the signal velocity. */
  pi->force.v_sig = max(pi->force.v_sig, v_sig);
  pj->force.v_sig = max(pj->force.v_sig, v_sig);

#ifdef SWIFT_HYDRO_DENSITY_CHECKS
  pi->n_force += wi + wj;
  pj->n_force += wi + wj;
  pi->N_force++;
  pj->N_force++;
#endif
}

/**
 * @brief Force interaction between two particles (non-symmetric).
 *
 * @param r2 Comoving square distance between the two particles.
 * @param dx Comoving vector separating both particles (pi - pj).
 * @param hi Comoving smoothing-length of particle i.
 * @param hj Comoving smoothing-length of particle j.
 * @param pi First particle.
 * @param pj Second particle (not updated).
 * @param a Current scale factor.
 * @param H Current Hubble parameter.
 */
__attribute__((always_inline)) INLINE static void runner_iact_nonsym_force(
    const float r2, const float dx[3], const float hi, const float hj,
    struct part *restrict pi, const struct part *restrict pj, const float a,
    const float H) {

#ifdef SWIFT_DEBUG_CHECKS
  if (pi->time_bin >= time_bin_inhibited)
    error("Inhibited pi in interaction function!");
  if (pj->time_bin >= time_bin_inhibited)
    error("Inhibited pj in interaction function!");
#endif

  /* Cosmological factors entering the EoMs */
  const float fac_mu = pow_three_gamma_minus_five_over_two(a);
  const float a2_Hubble = a * a * H;

  /* Get r and 1/r. */
  const float r = sqrtf(r2);
  const float r_inv = r ? 1.0f / r : 0.0f;

  /* Recover some data */
  const float mj = pj->mass;
  const float rhoi = pi->rho;
  const float rhoj = pj->rho;
  const float pressurei = pi->force.pressure;
  const float pressurej = pj->force.pressure;

  /* Get the kernel for hi. */
  const float hi_inv = 1.0f / hi;
  const float xi = r * hi_inv;
  float wi, wi_dx;
  kernel_deval(xi, &wi, &wi_dx);

  /* Get the kernel for hj. */
  const float hj_inv = 1.0f / hj;
  const float xj = r * hj_inv;
  float wj, wj_dx;
  kernel_deval(xj, &wj, &wj_dx);

    
    const float hid_inv = pow_dimension_plus_one(hi_inv); /* 1/h^(d+1) */
    const float hjd_inv = pow_dimension_plus_one(hj_inv); /* 1/h^(d+1) */
    
  float Gj[3], Gi[3];  

    int k;
    for(k=0;k<3;k++){
        Gi[k] = -(pi->C[k][0] * dx[0] + pi->C[k][1] * dx[1] + pi->C[k][2] * dx[2]) * wi * hid_inv;//wi_dr * dx[k] * r_inv;//
        Gj[k] = -(pj->C[k][0] * dx[0] + pj->C[k][1] * dx[1] + pj->C[k][2] * dx[2]) * wj * hjd_inv;//wj_dr * dx[k] * r_inv;//
    }

  float kernel_gradient[3];
    kernel_gradient[0] = 0.5f * (Gi[0] + Gj[0]);//0.5f * (Gi[0] * pi->f_gdf + Gj[0] * pj->f_gdf);//
    kernel_gradient[1] = 0.5f * (Gi[1] + Gj[1]);//0.5f * (Gi[1] * pi->f_gdf + Gj[1] * pj->f_gdf);//
    kernel_gradient[2] = 0.5f * (Gi[2]  + Gj[2]);//0.5f * (Gi[2] * pi->f_gdf + Gj[2] * pj->f_gdf);//
    
                  // Compute dv dot G. 
    const float dvdGi = (pi->v[0] - pj->v[0]) * Gi[0] +
                    (pi->v[1] - pj->v[1]) * Gi[1] +
                    (pi->v[2] - pj->v[2]) * Gi[2];
      
    
    
        const float dvdGij = (pi->v[0] - pj->v[0]) * kernel_gradient[0] +
                     (pi->v[1] - pj->v[1]) * kernel_gradient[1] +
                     (pi->v[2] - pj->v[2]) * kernel_gradient[2];
    
    

  /* Compute dv dot r. */
  const float dvdr = (pi->v[0] - pj->v[0]) * dx[0] +
                     (pi->v[1] - pj->v[1]) * dx[1] +
                     (pi->v[2] - pj->v[2]) * dx[2] + a2_Hubble * r2;

  /* Balsara term */
  const float balsara_i = pi->force.balsara;
  const float balsara_j = pj->force.balsara;

  /* Are the particles moving towards each other? */
  const float omega_ij = min(dvdr, 0.f);
  const float mu_ij = fac_mu * r_inv * omega_ij; /* This is 0 or negative */

  /* Compute sound speeds */
  const float ci = pi->force.soundspeed;
  const float cj = pj->force.soundspeed;

  /* Signal velocity */
  const float v_sig = ci + cj - const_viscosity_beta * mu_ij;

  /* Construct the full viscosity term */
  const float rho_ij = 0.5f * (rhoi + rhoj);
  const float visc = -0.25f * v_sig * mu_ij * (balsara_i + balsara_j) / rho_ij;

  /* Convolve with the kernel */
  const float visc_acc_term = visc;
 
  /* SPH acceleration term */
  const float sph_acc_term =
      (pressurei + pressurej)/ (pi->rho * pj->rho);//(pressurei + Q_i + pressurej + Q_j)/ (pi->rho * pj->rho);

  /* Assemble the acceleration */
  const float acc = sph_acc_term + visc;

  /* Use the force Luke ! */
  pi->a_hydro[0] -= mj * acc * kernel_gradient[0];
  pi->a_hydro[1] -= mj * acc * kernel_gradient[1];
  pi->a_hydro[2] -= mj * acc * kernel_gradient[2];

 /* Get the time derivative for u. */
      const float sph_du_term_i =
      (pressurei) * dvdGij / (rhoi * rhoj); //;(pressurei + Q_i) * dvdGij / (rhoi * rhoj);    

  /* Viscosity term */
  const float visc_du_term = 0.5f * visc_acc_term * dvdGij;

  /* Assemble the energy equation term */
  const float du_dt_i = sph_du_term_i + visc_du_term;

  /* Internal energy time derivative */
  pi->u_dt += du_dt_i * mj;

  /* Get the time derivative for h. */
      pi->force.h_dt -= mj * dvdGi / rhoj;

  /* Update the signal velocity. */
  pi->force.v_sig = max(pi->force.v_sig, v_sig);

#ifdef SWIFT_HYDRO_DENSITY_CHECKS
  pi->n_force += wi + wj;
  pi->N_force++;
#endif
}

#endif /* SWIFT_PLANETARY_HYDRO_IACT_H */
