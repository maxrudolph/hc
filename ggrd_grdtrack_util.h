/* 

header files for modified GMT grd interpolation routines dealing
with grd interpolation

*/

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef __GGRD_READ_GMT__
#include "gmt.h"
#define __GGRD_READ_GMT__
#endif

#include "ggrd.h"
/* 

wrappers

*/
int ggrd_grdtrack_init_general(ggrd_boolean ,char *, char *,char *,
			       struct ggrd_gt *,ggrd_boolean ,
			       ggrd_boolean);
ggrd_boolean ggrd_grdtrack_interpolate_rtp(double ,double ,double ,
					    struct ggrd_gt *,double *,
					    ggrd_boolean);
ggrd_boolean ggrd_grdtrack_interpolate_xyz(double ,double ,double ,
					    struct ggrd_gt *,double *,
					    ggrd_boolean);
ggrd_boolean ggrd_grdtrack_interpolate_xy(double ,double ,
					   struct ggrd_gt *,
					   double *,
					   ggrd_boolean );
ggrd_boolean ggrd_grdtrack_interpolate_tp(double ,double ,
					   struct ggrd_gt *,
					   double *,
					   ggrd_boolean );

void ggrd_grdtrack_free_gstruc(struct ggrd_gt *);

int ggrd_grdtrack_rescale(struct ggrd_gt *,ggrd_boolean , ggrd_boolean , 
			  ggrd_boolean ,double);


/* 

moderately external

*/
int ggrd_init_thist_from_file(struct ggrd_t *,char *,ggrd_boolean ,ggrd_boolean);
int ggrd_read_vel_grids(struct ggrd_vel *, double, unsigned short, unsigned short, char *);

#ifdef USE_GMT4
/* GMT4.1.2 */
ggrd_boolean ggrd_grdtrack_interpolate(double *, ggrd_boolean , struct GRD_HEADER *, float *,
					struct GMT_EDGEINFO *, int, float *, int ,	double *,ggrd_boolean,
					struct GMT_BCR *);
int ggrd_grdtrack_init(double *, double *, double *, double *, float **, int *, char *, struct GRD_HEADER **, struct GMT_EDGEINFO **, char *, ggrd_boolean *, int *, ggrd_boolean, char *, float **, int *, ggrd_boolean, ggrd_boolean, ggrd_boolean, struct GMT_BCR *);

#else
/* GMT 3.4.5 */
ggrd_boolean ggrd_grdtrack_interpolate(double *, ggrd_boolean , struct GRD_HEADER *, float *,
				       struct GMT_EDGEINFO *, int, 
				       float *, int ,	
				       double *,ggrd_boolean,
				       struct BCR *);

int ggrd_grdtrack_init(double *, double *,double *, double *, /* geographic bounds,
								 set all to zero to 
								 get the whole range from the
								 input grid files
							      */
			float **,	/* data */
			int *,  /* size of data */
			char *,	/* name, or prefix, of grd file with scalars */
			struct GRD_HEADER **,
			struct GMT_EDGEINFO **,
			char *,ggrd_boolean *,
			int *,	/* [4] array with padding (output) */
			ggrd_boolean _d, char *, 	/* depth file name */
			float **,	/* layers, pass as NULL */
			int *,		/* number of layers */
			ggrd_boolean , /* linear/cubic? */
		       ggrd_boolean ,ggrd_boolean,
		       struct BCR *);

void ggrd_global_bcr_assign(struct BCR *);

void my_GMT_bcr_init (struct GRD_HEADER *, int *, 
		      int ,struct BCR *);

#endif
/* 

local 

 */

void ggrd_gt_interpolate_z(double,float *,int ,
			   int *, int *, double *, double *,ggrd_boolean,
			   ggrd_boolean *); /*  */
float ggrd_gt_rms(float *,int );
float ggrd_gt_mean(float *,int );

void ggrd_print_layer_avg(float *,float *,int , int ,FILE *);

void ggrd_find_spherical_vel_from_rigid_cart_rot(double *,
						 double *,
						 double *,
						 double *, 
						 double *);
						 


void ggrd_vecalloc(double **,int,char *);
void ggrd_vecrealloc(double **,int,char *);
