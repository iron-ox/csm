#include <math.h>
#include <gsl/gsl_nan.h>
#include "math_utils.h"

#include "gsl_stack.h"
#include "laser_data.h"

void find_neighbours(LDP ld, int i, int max_num, int*indexes, size_t*num_found);
void filter_orientation(double theta0, double rho0, size_t n,
 	const double*thetas, const double*rhos, double *alpha, double*cov0_alpha );

void ld_compute_orientation(LDP ld, int size_neighbourhood, double sigma) {
	int i;
	for(i=0;i<ld->nrays;i++){
		if(!ld_valid_ray(ld,i)) {
			ld->alpha[i] = GSL_NAN;
			ld->cov_alpha[i] = GSL_NAN;
			ld->alpha_valid[i] = 0;
			continue;
		}
		
		int neighbours[size_neighbourhood*2];
		size_t num_neighbours;
		find_neighbours(ld, i, size_neighbourhood, neighbours, &num_neighbours);

		if(0==num_neighbours) {
			ld->alpha[i] = GSL_NAN;
			ld->cov_alpha[i] = GSL_NAN;
			ld->alpha_valid[i] = 0;
			continue;
		}

		printf("orientation for i=%d:\n",i);
		double thetas[num_neighbours];
		double readings[num_neighbours];
		size_t a=0; for(a=0;a<num_neighbours;a++){
			thetas[a] = ld->theta[neighbours[a]];
			readings[a] = ld->readings[neighbours[a]];
	//		printf(" j = %d theta = %f rho = %f\n",neighbours[a],thetas[a],readings[a]);
		}
		
		double alpha=42, cov0_alpha=32;
		filter_orientation(ld->theta[i],ld->readings[i],num_neighbours,
			thetas,readings,&alpha,&cov0_alpha);
			
		ld->alpha[i] = alpha;
		ld->cov_alpha[i] = cov0_alpha * square(sigma);
		ld->alpha_valid[i] = 1;
		printf("---------- i = %d alpha = %f sigma=%f cov_alpha = %f\n", i,
			alpha, ld->cov_alpha[i]);
	}
}

void filter_orientation(double theta0, double rho0, size_t n,
 	const double*thetas, const double*rhos, double *alpha, double*cov0_alpha ) {
	
	
	// y = l x + r epsilon
	gsl_matrix * y = gsl_matrix_alloc(n,1);
	gsl_matrix * l = gsl_matrix_alloc(n,1);
	gsl_matrix * r = gsl_matrix_alloc(n,n+1);

	gsl_matrix_set_all(r, 0.0);
	gsl_matrix_set_all(l, 1.0);
	
 	size_t i; for(i=0;i<n;i++) {
		gms(y,i,0,    (rhos[i]-rho0)/(thetas[i]-theta0) );
		gms(r,i,0,    -1/(thetas[i]-theta0) );
		gms(r,i,i+1,   1/(thetas[i]-theta0) );
	}
	
	int m;
	gsls_set(r);
	gsls_trans();
	gsls_mult_left(r);
	gsls_inv(); 
	gsl_matrix *Rinv = gsls_copy();
	
	// cov(f1) = (l^t Rinv l)^-1 
	gsls_set(l); 
	gsls_trans(); 
	gsls_mult(Rinv); 
	gsls_mult(l); 
	gsls_inv(); 
	double cov_f1 = gsls_get_at(0,0);
	//     f1  = cov_f1 l^t Rinv y
	gsls_mult_t(l);
	gsls_mult(Rinv);
	gsls_mult(y);
	double f1 = gsls_get_at(0,0);
	
/*	printf("l= ");
	gsl_matrix_fprintf(stdout, l, "%f");
	printf("\ny= ");
	gsl_matrix_fprintf(stdout, y, "%f");
	printf("\nr= ");
	gsl_matrix_fprintf(stdout, r, "%f");
	printf("\ninv(r*r)= ");
	gsl_matrix_fprintf(stdout, Rinv, "%f");*/

	gsl_matrix_free(Rinv);
	gsl_matrix_free(y);
	gsl_matrix_free(l);
	gsl_matrix_free(r);
	
	*alpha = theta0 - atan(f1/rho0);

	if(cos(*alpha)*cos(theta0)+sin(*alpha)*sin(theta0)>0) {
		*alpha = *alpha + M_PI;
	}
	
	double dalpha_df1  = rho0 / (square(rho0) + square(f1));
	double dalpha_drho = -f1 /  (square(rho0) + square(f1));
	
	*cov0_alpha	= square(dalpha_df1) * cov_f1 + square(dalpha_drho);
	printf("dalpha_df1 = %f dalpha_drho = %f\n",dalpha_df1,dalpha_drho);
	printf("f1 = %f covf1 = %f alpha = %f cov_alpha = %f\n ",f1,cov_f1,*alpha,*cov0_alpha);
	printf("sotto = %f\n ",(square(rho0) + square(f1)));
	
/*	printf("   f1 = %f cov =%f \n", f1,cov_f1);
	printf("   f1/rho = %f \n", f1/rho0);
	printf("   atan = %f \n", atan(f1/rho0));
	printf("   theta0= %f \n", theta0);*/
//	printf("   alpha = %f sigma= %f°\n", *alpha, rad2deg(0.01*sqrt(*cov0_alpha)));
}
	


// indexes: an array of size "max_num*2"
void find_neighbours(LDP ld, int i, int max_num, int*indexes, size_t*num_found) {
	*num_found = 0;
	
	int up = i; 
	while ((up+1 <= i+max_num) && (up+1<ld->nrays) && ld_valid_ray(ld,up+1)
			&& (ld->cluster[up+1] == ld->cluster[i])) {
		up+=1; 
		indexes[(*num_found)++] = up;
	}
	int down = i; 
	while ((down >= i-max_num) && (down-1>=0) && ld_valid_ray(ld,down-1) && 
			(ld->cluster[down-1] == ld->cluster[i])) {
		down-=1;
		indexes[(*num_found)++] = down;
	}
}


