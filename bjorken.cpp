#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <string>
#include <string.h>
#include <iostream>
#include <iomanip>
using namespace std;
#include <sstream>
#include <fstream>
#include "qcd.hpp"
#include "evolution.hpp"
#include "inferredvariables.hpp"
#include "transport.hpp"

#define GEV_TO_INVERSE_FM 5.067731

#define GLASMA 0								// 0 = equilibrium initial conditions
#define KINETIC 0								// 0 = standard, 1 = quasiparticle

int main()
{
	// input parameters
	const double T0 = 0.5 * GEV_TO_INVERSE_FM;  // initial temperature in fm^-1
	const double tau0 = 0.25;					// initial time in fm
	const double tauf = 100.0;					// final time in fm


	// initialize temperature
	double T = T0;


	// initial flow profile
	double ut = 1.0;
	double ux = 0.0;
	double uy = 0.0;
	double un = 0.0;


	// initialize energy density and pressure  (units = [fm^-4])
	const double e0 = equilibriumEnergyDensity(T0);
	double e = e0;
	const double p0 = equilibriumPressure(e0);
	double p = p0;

	double cs2 = speedOfSoundSquared(e0);

	double s0 = (e0+p0)/T0; 						    // initial entropy density
	double zetas0 = bulkViscosityToEntropyDensity(T0);  // initial specific bulk viscosity
	double etas0 = shearViscosityToEntropyDensity(T0);  // initial specific shear viscosity

#if (KINETIC == 1)										// quasiparticle model
	double taupi = (s0*etas0) / beta_shear(T0);
	double taubulk = (s0*zetas0) / beta_bulk(T0);
#else 													// m/T << 1 fixed mass model
	double taupi = 5.0 * shearViscosityToEntropyDensity(T0) / T0;
	double taubulk = bulkViscosityToEntropyDensity(T0) / (15.0*T0*pow(1.0/3.0-cs2,2));
#endif

	// initial Tt\mu components (units = [fm^-4])
	double Ttt = e0;
	double Ttx = 0.0;
	double Tty = 0.0;
	double Ttn = 0.0;

	// initialize shear stress: pi = - tau^2 * pinn (units = [fm^-4])
	double pi0 = 4.0 * s0 / (3.0 * tau0) * etas0; // (Navier Stokes)
	double pi = 0.0;

	// initialize bulk pressure (units = [fm^-4])
	double bulk0 = - s0 * zetas0 / tau0;  // (Navier Stokes)
	double Pi = 0.0; // set to zero for now

	double piNS = pi0;
	double bulkNS = bulk0;

#if (GLASMA == 1)
	double PL = 0.014925 * e / 3.0;
	double PT = 1.4925 * e / 3.0;
	pi = 2.0*(PT-PL)/3.0;
	Pi = (2.0*PT/3.0 + PL/3.0 - p);
#endif

	double Beq = equilibriumBquasi(T);

	double dB2nd = -3.0*taubulk*mdmdT_Quasiparticle(T)/pow(z_Quasiparticle(T),2)*cs2*Pi/(tau0*T);

	double B = Beq + dB2nd;


	// intermediate and end values for heun's rule
	double Ttt_mid, Ttt_end;
	double Ttx_mid, Ttx_end;
	double Tty_mid, Tty_end;
	double Ttn_mid, Ttn_end;
	double pi_mid, pi_end;
	double Pi_mid, Pi_end;


	// initial time variable set to tau0, number of steps, stepsize
	double tau = tau0;
	const double dtau = 0.01;
	const int n = floor((tauf - tau0) / dtau);
	const int timesteps_per_write = 10;

	// Data files for plots
	ofstream eplot, piplot, bulkplot, plptplot;
	ofstream RpiInvplot, RbulkInvplot;
	ofstream taupiplot, taubulkplot;
	ofstream Bplot, dB2ndplot;
	ofstream piNSplot, bulkNSplot;
	ofstream tanhrhoplot;

	eplot.open("results/eplot.dat", ios::out);
	piplot.open("results/piplot.dat", ios::out);
	bulkplot.open("results/bulkplot.dat", ios::out);
	plptplot.open("results/plptplot.dat", ios::out);

	RpiInvplot.open("results/RpiInvplot.dat", ios::out);
	RbulkInvplot.open("results/RbulkInvplot.dat", ios::out);

	taupiplot.open("results/taupiplot.dat", ios::out);
	taubulkplot.open("results/taubulkplot.dat", ios::out);

	Bplot.open("results/Bplot.dat", ios::out);
	dB2ndplot.open("results/dB2ndplot.dat", ios::out);

	piNSplot.open("results/piNSplot.dat", ios::out);
	bulkNSplot.open("results/bulkNSplot.dat", ios::out);


	eplot << "tau [fm]" << "\t\t" << "e/e0" << endl << setprecision(6) << tau << "\t\t" << e/e0 << endl;
	piplot << "tau [fm]" << "\t\t" << "pi [fm^-4]" << endl << setprecision(6) << tau << "\t\t" << pi << endl;
	bulkplot << "tau [fm]" << "\t\t" << "Pi [fm^-4]" << endl << setprecision(6) << tau << "\t\t" << Pi << endl;
	plptplot << "tau [fm]" << "\t\t" << "PL/PT" << endl << setprecision(6) << tau << "\t\t" << (p + Pi - pi) / (p + Pi + 0.5*pi) << endl;

	RpiInvplot << "tau [fm]" << "\t\t" << "R_pi^-1" << endl << setprecision(6) << tau << "\t\t" << sqrt(1.5) * pi / p << endl;
	RbulkInvplot << "tau [fm]" << "\t\t" << "R_Pi^-1" << endl << setprecision(6) << tau << "\t\t" << Pi / p << endl;

	taupiplot << "tau [fm]" << "\t\t" << "tau_pi" << endl << setprecision(6) << tau << "\t\t" << taupi << endl;
	taubulkplot << "tau [fm]" << "\t\t" << "tau_Pi" << endl << setprecision(6) << tau << "\t\t" << taubulk << endl;

	Bplot << "tau [fm]" << "\t\t" << "B" << endl << setprecision(6) << tau << "\t\t" << B << endl;
	dB2ndplot << "tau [fm]" << "\t\t" << "dB_2nd" << endl << setprecision(6) << tau << "\t\t" << dB2nd << endl;

	piNSplot << "tau [fm]" << "\t\t" << "R_piNS^-1" << endl << setprecision(6) << tau << "\t\t" << sqrt(1.5) * piNS / p << endl;

	bulkNSplot << "tau [fm]" << "\t\t" << "R_PiNS^-1" << endl << setprecision(6) << tau << "\t\t" << bulkNS / p << endl;


	// start evolution
	for(int i = 0; i < n; i++)
	{
		// compute intermediate values with Euler step
		Ttt_mid = Ttt + dtau * dTtt_dtau(Ttt, Ttx, Tty, Ttn, pi, Pi, ut, ux, uy, un, e, p, tau);
		Ttx_mid = Ttx + dtau * dTtx_dtau(Ttt, Ttx, Tty, Ttn, pi, Pi, ut, ux, uy, un, e, p, tau);
		Tty_mid = Tty + dtau * dTty_dtau(Ttt, Ttx, Tty, Ttn, pi, Pi, ut, ux, uy, un, e, p, tau);
		Ttn_mid = Ttn + dtau * dTtn_dtau(Ttt, Ttx, Tty, Ttn, pi, Pi, ut, ux, uy, un, e, p, tau);
		pi_mid = pi + dtau * dpi_dtau(Ttt, Ttx, Tty, Ttn, pi, Pi, ut, ux, uy, un, e, p, tau);
		Pi_mid = Pi + dtau * dPi_dtau(Ttt, Ttx, Tty, Ttn, pi, Pi, ut, ux, uy, un, e, p, tau);


		// find intermediate inferred variables
		get_inferred_variables(Ttt_mid, Ttx_mid, Tty_mid, Ttn_mid, pi_mid, Pi_mid, &ut, &ux, &uy, &un, &e, &p, tau + dtau);


		// add Euler step with respect to the intermediate value
		Ttt_end = Ttt_mid + dtau * dTtt_dtau(Ttt_mid, Ttx_mid, Tty_mid, Ttn_mid, pi_mid, Pi_mid, ut, ux, uy, un, e, p, tau + dtau);
		Ttx_end = Ttx_mid + dtau * dTtx_dtau(Ttt_mid, Ttx_mid, Tty_mid, Ttn_mid, pi_mid, Pi_mid, ut, ux, uy, un, e, p, tau + dtau);
		Tty_end = Tty_mid + dtau * dTty_dtau(Ttt_mid, Ttx_mid, Tty_mid, Ttn_mid, pi_mid, Pi_mid, ut, ux, uy, un, e, p, tau + dtau);
		Ttn_end = Ttn_mid + dtau * dTty_dtau(Ttt_mid, Ttx_mid, Tty_mid, Ttn_mid, pi_mid, Pi_mid, ut, ux, uy, un, e, p, tau + dtau);
		pi_end = pi_mid + dtau * dpi_dtau(Ttt_mid, Ttx_mid, Tty_mid, Ttn_mid, pi_mid, Pi_mid, ut, ux, uy, un, e, p, tau + dtau);
		Pi_end = Pi_mid + dtau * dPi_dtau(Ttt_mid, Ttx_mid, Tty_mid, Ttn_mid, pi_mid, Pi_mid, ut, ux, uy, un, e, p, tau + dtau);

		// increase time step
		tau += dtau;


		// Heun's Rule (average initial and end)
		// updated variables at new time step
		Ttt = 0.5 * (Ttt + Ttt_end);
		Ttx = 0.5 * (Ttx + Ttx_end);
		Tty = 0.5 * (Tty + Tty_end);
		Ttn = 0.5 * (Ttn + Ttn_end);
		pi = 0.5 * (pi + pi_end);
		Pi = 0.5 * (Pi + Pi_end);


		// find inferred variables at new time step
		get_inferred_variables(Ttt, Ttx, Tty, Ttn, pi, Pi, &ut, &ux, &uy, &un, &e, &p, tau);

		T = effectiveTemperature(e);
		cs2 = speedOfSoundSquared(e);


		piNS = 4.0 * (e+p) / (3.0*T*tau) * shearViscosityToEntropyDensity(T);
		bulkNS = - (e+p) / (tau*T) * bulkViscosityToEntropyDensity(T);


	#if (KINETIC == 1)
		taupi = (e+p) * shearViscosityToEntropyDensity(T) / (T*beta_shear(T));
		taubulk = (e+p) * bulkViscosityToEntropyDensity(T) / (T*beta_bulk(T));
	#else
		taupi = 5.0 * shearViscosityToEntropyDensity(T) / T;
		taubulk = bulkViscosityToEntropyDensity(T) / (15.0*T*pow(1.0/3.0-cs2,2));
	#endif

		Beq = equilibriumBquasi(T);

		dB2nd = -3.0*taubulk*mdmdT_Quasiparticle(T)/pow(z_Quasiparticle(T),2)*cs2*Pi/(tau*T);

		B = Beq + dB2nd;


		// write updated energy density to file
		if((i+1)%timesteps_per_write == 0)
		{
			eplot << setprecision(6) << tau << "\t\t" << e/e0 << "\t\t" << endl;
			piplot << setprecision(6) << tau << "\t\t" << pi << "\t\t" << endl;
			bulkplot << setprecision(6) << tau << "\t\t" << Pi << "\t\t" << endl;
			plptplot << setprecision(6) << tau << "\t\t" << (p + Pi - pi) / (p + Pi + 0.5*pi) << "\t\t" << endl;

			RpiInvplot << setprecision(6) << tau << "\t\t" << sqrt(1.5) * pi / p << "\t\t" << endl;
			RbulkInvplot << setprecision(6) << tau << "\t\t" << Pi / p << "\t\t" << endl;

			taupiplot << setprecision(6) << tau << "\t\t" << taupi << "\t\t" << endl;
			taubulkplot << setprecision(6) << tau << "\t\t" << taubulk  << "\t\t" << endl;

			Bplot << setprecision(6) << tau << "\t\t" << B << "\t\t" << endl;
			dB2ndplot << setprecision(6) << tau << "\t\t" << dB2nd << "\t\t" << endl;

			piNSplot << setprecision(6) << tau << "\t\t" << sqrt(1.5) * piNS / p << endl;
			bulkNSplot << setprecision(6) << tau << "\t\t" << bulkNS / p << endl;
		}
	}

	// close plot data files

	eplot.close();
	piplot.close();
	bulkplot.close();
	plptplot.close();

	RpiInvplot.close();
	RbulkInvplot.close();

	taupiplot.close();
	taubulkplot.close();

	Bplot.close();
	dB2ndplot.close();

	piNSplot.close();
	bulkNSplot.close();
	tanhrhoplot.close();

	printf("Done\n\n");

	return 0;
}






