//-----------------------------------------------------------------------bl-
//--------------------------------------------------------------------------
//
// Antioch - A Gas Dynamics Thermochemistry Library
//
// Copyright (C) 2014-2016 Paul T. Bauman, Benjamin S. Kirk,
//                         Sylvain Plessis, Roy H. Stonger
//
// Copyright (C) 2013 The PECOS Development Team
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the Version 2.1 GNU Lesser General
// Public License as published by the Free Software Foundation.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc. 51 Franklin Street, Fifth Floor,
// Boston, MA  02110-1301  USA
//
//-----------------------------------------------------------------------el-

//Antioch
#include "antioch/vector_utils_decl.h"

#include "antioch/physical_constants.h"
#include "antioch/reaction_set.h"
#include "antioch/chemical_species.h"
#include "antioch/chemical_mixture.h"
#include "antioch/read_reaction_set_data.h"
#include "antioch/units.h"

#include "antioch/vector_utils.h"

//C++
#include <cmath>
#include <limits>

template<typename Scalar>
Scalar HE(const Scalar &T, const Scalar &Cf, const Scalar &eta, const Scalar &Tf = 1.L)
{
  return Cf * std::pow(T/Tf,eta);
}

template<typename Scalar>
Scalar Arrh(const Scalar &T, const Scalar &Cf, const Scalar &Ea)
{
  return Cf * std::exp(-Ea /(Antioch::Constants::R_universal<Scalar>() * T));
}

template<typename Scalar>
Scalar Kooij(const Scalar &T, const Scalar &Cf, const Scalar &eta, const Scalar &Ea, const Scalar &Tf = 1.L)
{
  return Cf * std::pow(T/Tf,eta) * std::exp(-Ea /(Antioch::Constants::R_universal<Scalar>() * T));
}

template<typename Scalar>
Scalar FcentTroe(const Scalar &T, const Scalar &alpha, const Scalar &T3, const Scalar &T1, const Scalar &T2 = -1.L)
{
  return (T2 < Scalar(0.))?(Scalar(1.) - alpha) * std::exp(-T/T3) + alpha * std::exp(-T/T1):
                           (Scalar(1.) - alpha) * std::exp(-T/T3) + alpha * std::exp(-T/T1) + std::exp(-T2/T);
}

template<typename Scalar>
Scalar coeffTroe(const Scalar &coef1, const Scalar &coef2, const Scalar &Fcent)
{
  return coef1 + coef2 * std::log10(Fcent);
}

template<typename Scalar>
Scalar cTroe(const Scalar &Fcent)
{
  Scalar c1(-0.40);
  Scalar c2(-0.67);
  return coeffTroe(c1,c2,Fcent);
}

template<typename Scalar>
Scalar nTroe(const Scalar &Fcent)
{
  Scalar c1(0.75);
  Scalar c2(-1.27);
  return coeffTroe(c1,c2,Fcent);
}

template<typename Scalar>
Scalar FTroe(const Scalar &Fcent, const Scalar &Pr)
{
  Scalar d(0.14L);
  return std::pow(10.L,std::log10(Fcent)/(Scalar(1.) + 
               std::pow( (std::log10(Pr) + cTroe(Fcent)) /
                                ( nTroe(Fcent) - d * (std::log10(Pr) + cTroe(Fcent)) ),2)));
}

template<typename VectorScalar>
typename Antioch::value_type<VectorScalar>::type k_photo(const VectorScalar &solar_lambda, const VectorScalar &solar_irr, 
                                                         const VectorScalar &sigma_lambda, const VectorScalar &sigma_sigma)
{
  Antioch::SigmaBinConverter<VectorScalar> bin;
  VectorScalar sigma_rescaled;
  bin.y_on_custom_grid(sigma_lambda,sigma_sigma,solar_lambda,sigma_rescaled);

  typename Antioch::value_type<VectorScalar>::type _k(0.L);
  for(unsigned int il = 0; il < solar_irr.size() - 1; il++)
  {
     _k += sigma_rescaled[il] * solar_irr[il] * (solar_lambda[il+1] - solar_lambda[il]);
  }

  return _k;
}

template<typename Scalar>
int tester(const std::string &root_name)
{

  std::vector<std::string> species_str_list;
  species_str_list.push_back( "O2" );
  species_str_list.push_back( "OH" );
  species_str_list.push_back( "H2" );
  species_str_list.push_back( "H2O" );
  species_str_list.push_back( "H2O2" );
  species_str_list.push_back( "HO2" );
  species_str_list.push_back( "O" );
  species_str_list.push_back( "CH3" );
  species_str_list.push_back( "CH4" );
  species_str_list.push_back( "H" );
  unsigned int n_species = species_str_list.size();

  Antioch::ChemicalMixture<Scalar> chem_mixture( species_str_list );
  Antioch::ReactionSet<Scalar> reaction_set( chem_mixture );
  Antioch::read_reaction_set_data_chemkin<Scalar>( root_name + "/test_parsing.chemkin", true, reaction_set );

  Scalar T = 2000.L;
  Antioch::Units<Scalar> unitA_m1("(mol/cm3)/s"),unitA_0("s-1"),unitA_1("cm3/mol/s"),unitA_2("cm6/mol2/s"),
                         unitEa_cal("cal/mol");

//
  // Molar densities
  std::vector<Scalar> molar_densities(n_species,5e-4);
  Scalar tot_dens((Scalar)n_species * 5e-4);

///Elementary, + Kooij
  std::vector<Scalar> k;
  Scalar A,b,Ea;
/*
! Hessler, J. Phys. Chem. A, 102:4517 (1998)
H+O2=O+OH                 3.547e+15 -0.406  1.6599E+4
*/
 A  = 3.547e15L * unitA_1.get_SI_factor();
 b  = -0.406L;
 Ea = 1.6599e4L * unitEa_cal.get_SI_factor(); 
 k.push_back(Kooij(T,A,b,Ea));


/*
! Sutherland et al., 21st Symposium, p. 929 (1986)
O+H2=H+OH                 0.508E+05  2.67  0.629E+04
*/
 A  = 0.508e5L * unitA_1.get_SI_factor();
 b  = 2.67L;
 Ea = 0.629e4L * unitEa_cal.get_SI_factor(); 
 k.push_back(Kooij(T,A,b,Ea));

/*
! Michael and Sutherland, J. Phys. Chem. 92:3853 (1988)
H2+OH=H2O+H               0.216E+09  1.51  0.343E+04
*/
 A  = 0.216e9L * unitA_1.get_SI_factor();
 b  = 1.51L;
 Ea = 0.343e4L * unitEa_cal.get_SI_factor(); 
 k.push_back(Kooij(T,A,b,Ea));

/*
! Sutherland et al., 23rd Symposium, p. 51 (1990)
O+H2O=OH+OH               2.97e+06   2.02  1.34e+4
*/
 A  = 2.97e6L * unitA_1.get_SI_factor();
 b  = 2.02L;
 Ea = 1.34e4L * unitEa_cal.get_SI_factor(); 
 k.push_back(Kooij(T,A,b,Ea));

//! *************** H2-O2 Dissociation Reactions ******************
/*
! Tsang and Hampson, J. Phys. Chem. Ref. Data, 15:1087 (1986)
H2+M=H+H+M                4.577E+19 -1.40  1.0438E+05
   H2/2.5/ H2O/12/
*/
 A  = 4.577e19L * unitA_1.get_SI_factor();
 b  = -1.40L;
 Ea = 1.0438e5L * unitEa_cal.get_SI_factor(); 
 Scalar sum_eps = 5e-4L * (2.5L + 12.L + (Scalar)(species_str_list.size() - 2));
 k.push_back(sum_eps * Kooij(T,A,b,Ea));

/*
! Tsang and Hampson, J. Phys. Chem. Ref. Data, 15:1087 (1986)
O+O+M=O2+M                6.165E+15 -0.50  0.000E+00
   H2/2.5/ H2O/12/
*/
 A = 6.165e15L * unitA_2.get_SI_factor();
 b = -0.50L;
 k.push_back(sum_eps * HE(T,A,b));

/*
! Tsang and Hampson, J. Phys. Chem. Ref. Data, 15:1087 (1986)
O+H+M=OH+M                4.714E+18 -1.00  0.000E+00
   H2/2.5/ H2O/12/
*/
 A  = 4.714e18L * unitA_2.get_SI_factor();
 b  = -1.00L;
 k.push_back(sum_eps * HE(T,A,b));

/*
! Tsang and Hampson, J. Phys. Chem. Ref. Data, 15:1087 (1986)
!H+OH+M=H2O+M              2.212E+22 -2.00  0.000E+00
H+OH+M=H2O+M               3.800E+22 -2.00  0.000E+00
   H2/2.5/ H2O/12/
*/
 A  = 3.800e22L * unitA_2.get_SI_factor();
 b  = -2.00L;
 k.push_back(sum_eps * HE(T,A,b));

/*
!************** Formation and Consumption of HO2******************

! Cobos et al., J. Phys. Chem. 89:342 (1985) for kinf
! Michael, et al., J. Phys. Chem. A, 106:5297 (2002) for k0

!******************************************************************************
*/
/*
! MAIN BATH GAS IS N2 (comment this reaction otherwise)
!
 H+O2(+M)=HO2(+M)      1.475E+12  0.60  0.00E+00
     LOW/6.366E+20  -1.72  5.248E+02/
     TROE/0.8  1E-30  1E+30/
     H2/2.0/ H2O/11./ O2/0.78/
*/
  A  = 6.366e20L * unitA_2.get_SI_factor();
  b  = -1.72L;
  Ea = 5.248e2L * unitEa_cal.get_SI_factor(); 
  Scalar k0   = Kooij(T,A,b,Ea);
  A  = 1.475e12L * unitA_1.get_SI_factor();
  b  = 0.60L;
  Ea = 0.00L * unitEa_cal.get_SI_factor(); 
  Scalar M = tot_dens + Scalar(5.39e-3L);
  Scalar kinf = Kooij(T,A,b,Ea);
  Scalar Pr = M * k0/kinf;
  Scalar Fc = FcentTroe(T,(Scalar)0.8L,(Scalar)1e-30L,(Scalar)1e30L);
  k.push_back(k0 / (1.L/M + k0/kinf) * FTroe(Fc,Pr));

/*
! Tsang and Hampson, J. Phys. Chem. Ref. Data, 15:1087 (1986) [modified]
HO2+H=H2+O2               1.66E+13   0.00   0.823E+03
*/
 A  = 1.66e13L * unitA_1.get_SI_factor();
 Ea = 0.823e3L * unitEa_cal.get_SI_factor(); 
 k.push_back(Arrh(T,A,Ea));

/*
! Tsang and Hampson, J. Phys. Chem. Ref. Data, 15:1087 (1986) [modified]
HO2+H=OH+OH               7.079E+13   0.00   2.95E+02
*/
 A  = 7.079e13L * unitA_1.get_SI_factor();
 Ea = 2.95e2L   * unitEa_cal.get_SI_factor(); 
 k.push_back(Arrh(T,A,Ea));

/*
! Baulch et al., J. Phys. Chem. Ref Data, 21:411 (1992)
HO2+O=O2+OH               0.325E+14  0.00   0.00E+00
*/
 A  = 0.325e14L * unitA_1.get_SI_factor();
 k.push_back(A);

/*
! Keyser, J. Phys. Chem. 92:1193 (1988)
HO2+OH=H2O+O2             2.890E+13  0.00 -4.970E+02
*/
 A  = 2.890e13L * unitA_1.get_SI_factor();
 Ea = -4.97e2L  * unitEa_cal.get_SI_factor(); 
 k.push_back(Arrh(T,A,Ea));

//! ***************Formation and Consumption of H2O2******************
/*
! Hippler et al., J. Chem. Phys. 93:1755 (1990)
HO2+HO2=H2O2+O2            4.200e+14  0.00  1.1982e+04
  DUPLICATE
HO2+HO2=H2O2+O2            1.300e+11  0.00 -1.6293e+3
  DUPLICATE
*/
 A  = 4.200e14L * unitA_1.get_SI_factor();
 Ea = 1.1982e4L * unitEa_cal.get_SI_factor(); 
 Scalar A2  = 1.300e11L  * unitA_1.get_SI_factor();
 Scalar Ea2 = -1.6293e3L * unitEa_cal.get_SI_factor(); 
 k.push_back(Arrh(T,A,Ea) + Arrh(T,A2,Ea2));

/*
! Brouwer et al., J. Chem. Phys. 86:6171 (1987) for kinf
! Warnatz, J. in Combustion chemistry (1984) for k0
H2O2(+M)=OH+OH(+M)         2.951e+14   0.00  4.843E+04
  LOW/1.202E+17  0.00  4.55E+04/
  TROE/0.5 1E-30 1E+30/
  H2/2.5/ H2O/12/
*/
 A  = 1.202e17L * unitA_1.get_SI_factor();
 Ea = 4.55e4L * unitEa_cal.get_SI_factor(); 
 k0 = Arrh(T,A,Ea);
 A  = 2.951e14L * unitA_0.get_SI_factor();
 Ea = 4.843e4L * unitEa_cal.get_SI_factor(); 
 M = tot_dens + 6.25e-3;
 kinf = Arrh(T,A,Ea);
 Pr = M * k0/kinf;
 Fc = FcentTroe(T,(Scalar)0.5L,(Scalar)1e-30L,(Scalar)1e30L);
 k.push_back(k0 / (1.L/M + k0/kinf)  * FTroe(Fc,Pr));
//

/*
! Tsang and Hampson, J. Phys. Chem. Ref. Data, 15:1087 (1986)
H2O2+H=H2O+OH             0.241E+14  0.00  0.397E+04
*/
 A  = 0.241e14L * unitA_1.get_SI_factor();
 Ea = 0.397e4L * unitEa_cal.get_SI_factor(); 
 k.push_back(Arrh(T,A,Ea));

/*
! Tsang and Hampson, J. Phys. Chem. Ref. Data, 15:1087 (1986)
H2O2+H=HO2+H2             0.482E+14  0.00  0.795E+04
*/
 A  = 0.482e14L * unitA_1.get_SI_factor();
 Ea = 0.795e4L * unitEa_cal.get_SI_factor(); 
 k.push_back(Arrh(T,A,Ea));

/*
! Tsang and Hampson, J. Phys. Chem. Ref. Data, 15:1087 (1986)
H2O2+O=OH+HO2             9.550E+06  2.00  3.970E+03
*/
 A  = 9.550e6L * unitA_1.get_SI_factor();
 b  = 2.00;
 Ea = 3.970e3L * unitEa_cal.get_SI_factor(); 
 k.push_back(Kooij(T,A,b,Ea));

/*
! Hippler and Troe, J. Chem. Phys. Lett. 192:333 (1992)
H2O2+OH=HO2+H2O           1.000E+12  0.00  0.000
    DUPLICATE
H2O2+OH=HO2+H2O           5.800E+14  0.00  9.557E+03
    DUPLICATE
*/
 A   = 1.000e12L * unitA_1.get_SI_factor();
 Ea  = 0.000L    * unitEa_cal.get_SI_factor(); 
 A2  = 5.800e14L * unitA_1.get_SI_factor();
 Ea2 = 9.557e3L  * unitEa_cal.get_SI_factor(); 
 k.push_back(Arrh(T,A,Ea) + Arrh(T,A2,Ea2));

/*! made up for rev test
CH3 + H <=>  CH4 1e12 1.2 3.125e4
  REV/ 1e8 1.2 3.5e3/
*/
  A  = 1e12    * unitA_1.get_SI_factor();
  b  = 1.2;
  Ea = 3.125e4 * unitEa_cal.get_SI_factor();
  k.push_back(Kooij(T,A,b,Ea));
  A  = 1e8   * unitA_0.get_SI_factor();
  b  = 0.8;
  Ea = 3.5e3 * unitEa_cal.get_SI_factor();
  k.push_back(Kooij(T,A,b,Ea));


  const Scalar tol = (std::numeric_limits<Scalar>::epsilon() < 1e-17L)?7e-16L:
                                                                       std::numeric_limits<Scalar>::epsilon() * 100;
  int return_flag(0);

  if(reaction_set.n_reactions() != k.size())
  {
     std::cerr << reaction_set << std::endl;
     std::cerr << "Not the right number of reactions" << std::endl;
     std::cerr << reaction_set.n_reactions() << " instead of " << k.size() << std::endl;
     return_flag = 1;
  }
  {
    for(unsigned int ir = 0; ir < k.size(); ir++)
    {
     const Antioch::Reaction<Scalar> * reac = &reaction_set.reaction(ir);

     if(std::abs(k[ir] - reac->compute_forward_rate_coefficient(molar_densities,T))/k[ir] > tol)
     {
        std::cout << *reac << std::endl;
        std::cout << std::scientific << std::setprecision(16)
                  << "Error in kinetics comparison\n"
                  << "reaction #"        << ir            << "\n"
                  << "temperature: "     << T     << " K" << "\n"
                  << "theory: "          << k[ir]         << "\n"
                  << "calculated: "      << reac->compute_forward_rate_coefficient(molar_densities,T) << "\n"
                  << "relative error = " << std::abs(k[ir] - reac->compute_forward_rate_coefficient(molar_densities,T))/k[ir] << "\n"
                  << "tolerance = "      <<  tol
                  << std::endl;
        return_flag = 1;
     }
    }
   }

  return return_flag;
}

int main(int argc, char* argv[])
{
  return (tester<float>(std::string(argv[1])) ||
          tester<double>(std::string(argv[1])) ||
          tester<long double>(std::string(argv[1])));
}
