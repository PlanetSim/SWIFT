/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2022 Matthieu Schaller (schaller@strw.leidenuniv.nl)
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
#ifndef SWIFT_DI_MHD_STRUCT_H
#define SWIFT_DI_MHD_STRUCT_H

/**
 * @brief Particle-carried fields for the MHD scheme.
 */
struct mhd_part_data {

  /*! Full Step Magnetic field */
  float Bfld[3];
  /*! Predicted Bfield */
  float BPred[3];
  /*! Full step Divergence of B */
  float divB;
  /*! limiters Induction and force */
  float Q1, Q0;
  /*! dB Direct Induction */
  float dBdt[3];
  /* Full step Dedner Cleaning Scalar */
  float phi;
  float Test[3];
};

/**
 * @brief Particle-carried extra fields for the MHD scheme.
 */
struct mhd_xpart_data {

  /* Dedner Cleaning Scalar */
  float phi;
  // NOT SURE
  // float Bfld[3];
};

#endif /* SWIFT_DI_MHD_STRUCT_H */