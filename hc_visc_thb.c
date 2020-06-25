#include "hc.h"
#include <math.h>

/* 
   
   implementation of Hager & O'Connell (1981) method of solving mantle
   circulation given internal density anomalies, only radially varying
   viscosity, and either free-slip or plate velocity boundary
   condition at surface. based on Hager & O'Connell (1981), Hager &
   Clayton (1989), and Steinberger (2000). the original code is due to
   Brad Hager, Rick O'Connell, and was largely modified by Bernhard
   Steinberger. this version by Thorsten Becker (twb@usc.edu) for
   additional comments, see hc.c
   
   scan through viscosities and compute correlation with the geoid
   
*/

//helper functions
int randInt(int n)//random integer in the range 0-n-1, inclusive
{
  //This implements rejection sampling to ensure uniform distribution of integers
  int r;
  int limit = RAND_MAX - (RAND_MAX % n);
  
  while((r = rand()) >= limit);
  
  return r % n;
}

double randDouble()//random double in the range 0-1
{
  double r = (double) rand() / ((double) RAND_MAX);
  return r;
}

double randn(gsl_rng *rng)//normally distributed random number
{
  double r = gsl_ran_gaussian(rng, 1.0);
  return r;
}

void interpolate_viscosity(struct thb_solution *solution,HC_PREC *rvisc,HC_PREC *visc, struct hc_parameters *p){
  const int nlayer = HC_INTERP_LAYERS;
  int i=0;
  int j=0;
  for(i=0;i<nlayer;i++){
    double this_r = rvisc[i];
    // find j such that this_r <= layer_r[j]
    while( solution->r[j+1] < this_r && (j+1) < solution->nlayer )
      j++;
    visc[i] = solution->visc[j]+(solution->visc[j+1]-solution->visc[j])/(solution->r[j+1]-solution->r[j])*(this_r-solution->r[j]);
    visc[i] = pow(10.0,visc[i]);
  }
  if( p->verbose ){
    fprintf(stderr,"interpolate_viscosity input:\n");
    for(i=0;i<solution->nlayer;i++){
      fprintf(stderr,"%.3e %.3e\n",(double) solution->r[i],(double) solution->visc[i]);
    }
    fprintf(stderr,"interpolate viscosity output:\n");
    for(i=0;i<HC_INTERP_LAYERS;i++){
      fprintf(stderr,"%.3e %.3e\n",(double) rvisc[i],(double) visc[i]);
    }

  }
}

int thb_max_layers(int iter){
  // Calculate the maximum number of control points/layers allowed according to a burn-in schedule
  // and the maximum number of allowed layers.
  int burnin_steps[MAX_NUM_VOR];

  burnin_steps[2] = 5000;
  int i;
  for(i=3;i<MAX_NUM_VOR;i++){
    burnin_steps[i] = burnin_steps[i-1] + 5000*i;
    i++;
  }
  int max_layers = 2;
  while(max_layers < MAX_NUM_VOR && burnin_steps[max_layers] < iter){
    max_layers++;
  }
  return max_layers;
}

void propose_solution(struct hcs *model, struct thb_solution *old_solution, struct thb_solution *new_solution,gsl_rng *rng, int iter, struct hc_parameters *p){
  const double visc_min = 19.0;
  const double visc_max = 25.0;
  const double visc_range = visc_max - visc_min;
  const double visc_change = 0.2;
  
  const double rad_min = model->r_cmb;
  const double rad_max = 1.0;
  const double rad_change = 0.05;      // shape parameter for proposal distribution
  const double rad_range = rad_max-rad_min;
  const double drmin = 7e-3;           // minimum layer thickness
  
  const double var_min = 1e-3;
  const double var_max = 1e50;
  const double var_change = 0.05;

  const int max_vor = thb_max_layers( iter );

  // choose one of five options at random
  int random_choice = 0;
  int success = 0;
  while(!success){
    success=1;
    new_solution[0] = old_solution[0]; // copy old to new
    random_choice = randInt(5);
    if(random_choice == 0){
      if( new_solution->nlayer == max_vor ){
	success = 0;
      }else{
	// Add a control point at random
	double new_rad = rad_min + rad_range*randDouble();
	double new_visc = visc_min + visc_range*randDouble();
	int i=0;
	while(new_solution->r[i] < new_rad && i < new_solution->nlayer)
	  i++;
	int j;
	for(j=new_solution->nlayer;j>i;j--){
	  new_solution->r[j] = new_solution->r[j-1];
	  new_solution->visc[j] = new_solution->visc[j-1];
	}
	new_solution->r[i] = new_rad;
	new_solution->visc[i] = new_visc;
	new_solution->nlayer++;
      }
    }else if(random_choice == 1){
      // kill a layer (at random)
      if( new_solution->nlayer <= 2){
	success = 0; // can't kill when there are 2 control points!
      }else if( new_solution->nlayer == 3){
	int kill_layer = 1;
	new_solution->r[1] = new_solution->r[2];
	new_solution->visc[1] = new_solution->visc[2];
	new_solution->nlayer--;
      }else{
	int kill_layer = randInt(new_solution->nlayer-2) + 1;
	int j;
	for(j=new_solution->nlayer-1;j>kill_layer;j--){
	  new_solution->r[j-1] = new_solution->r[j];
	  new_solution->visc[j-1] = new_solution->visc[j];
	}
	new_solution->nlayer--;
      }
    }else if(random_choice == 2){
      // perturb the location of a control point
      if(new_solution->nlayer == 2){
	success = 0;
      }else{
	int perturb_layer = randInt(new_solution->nlayer-2)+1;
	new_solution->r[ perturb_layer ] += rad_change*randn(rng);
	if(new_solution->r[ perturb_layer] <= model-> r_cmb || new_solution->r[ perturb_layer ] >= 1.0){
	  success = 0;
	}
      }
    }else if(random_choice == 3){
      //perturb viscosity
      int perturb_layer = randInt(new_solution->nlayer);
      new_solution->visc[perturb_layer] += visc_change*randn(rng);
      if( new_solution->visc[perturb_layer] > visc_max || new_solution->visc[perturb_layer] < visc_min){
	success = 0;
      }
    }else if(random_choice == 4){
      //change variance
      new_solution->var = pow(10.0, log10(new_solution->var) + var_change*randn(rng));
      if( new_solution->var > var_max || new_solution->var < var_min){
	success = 0;
      }
    }else{
      exit(-1);
    }
    // any additional sanity checks on the proposed solution:
    int j;
    for(j=1;j<new_solution->nlayer;j++){
      double dr = new_solution->r[j]-new_solution->r[j-1];
      if( dr < drmin )
	success = 0; // This ensures that the nodes remain in increasing order
    }
  }// End while not successful
  if( p-> verbose){
    fprintf(stderr,"Proposed solution:\n");
    int i;
    for(i=0;i<new_solution->nlayer;i++){
      fprintf(stderr,"%.3e %.3e\n",new_solution->r[i],new_solution->visc[i]);
    }
  }
}


int main(int argc, char **argv)
{

  int rank = 0;// for future parallel tempering implementation
  
  struct hcs *model;		/* main structure, make sure to initialize with 
				   zeroes */
  struct sh_lms *sol_spectral=NULL, *geoid = NULL;		/* solution expansions */
  struct sh_lms *pvel=NULL;					/* local plate velocity expansion */
  int nsol,lmax,solved;
  struct hc_parameters p[1]; /* parameters */
  HC_PREC corr[2];			/* correlations */
  HC_PREC vl[4][3],v[4],dv;			/*  for viscosity scans */

  struct thb_solution sol1,sol2; /* accepted and proposed solutions */
  /* THB residual vectors */
  int thb_ll[] = {2,3,4,5,6,7};
  int thb_nl = 6;
  int thb_nlm=0;
  {
    int i;
    for(i=0;i<thb_nlm;i++){
      thb_nlm += 2*thb_ll[i]+1;
    }
  }
  double *residual1 = (double *) malloc(sizeof(double)*thb_nlm);
  double *residual2 = (double *) malloc(sizeof(double)*thb_nlm);
  double likeprob1,likeprob2;
  
  
  /* Initialize random number generation using GNU Scientific Library */
  
  const gsl_rng_type *rng_type;
  gsl_rng *rng;
  gsl_rng_env_setup();
  rng_type = gsl_rng_default;
  rng = gsl_rng_alloc (rng_type);
  /*     
     (1)
      
     initialize the model structure, this is needed to initialize some
     of the default values before callign the parameter handling
     routine this call also involves initializing the hc parameter
     structure
      
  */
  hc_struc_init(&model);
  /* 
     
     (2)
     init parameters to default values
     
  */
  hc_init_parameters(p);
  /* 
     
     special options for this computation
     
  */
  p->solver_mode = HC_SOLVER_MODE_VISC_SCAN;
  p->visc_init_mode = HC_INIT_E_INTERP;
  p->compute_geoid = 1;
  p->compute_geoid_correlations = TRUE;
  
  if(argc > 1){
    /* read in the reference geoid */
    strcpy(p->ref_geoid_file,argv[1]);
    hc_read_scalar_shexp(p->ref_geoid_file,&(p->ref_geoid),"reference geoid",p);
  }else{
    fprintf(stderr,"%s: ERROR: need geoid.ab file as an argument\n",argv[0]);
    fprintf(stderr,"%s: usage:\n\n%s geoid.ab\n\n",argv[0],argv[0]);
    fprintf(stderr,"%s: for help, use:\n\n%s geoid.ab -h\n\n",argv[0],argv[0]);
    exit(-1);
  }
  /* 
     handle other command line arguments
  */
  hc_handle_command_line(argc,argv,2,p);
  /* 
     
     begin main program part
     
  */
#ifdef __TIMESTAMP__
  if(p->verbose)
    fprintf(stderr,"%s: starting version compiled on %s\n",
	    argv[0],__TIMESTAMP__);
#else
  if(p->verbose)
    fprintf(stderr,"%s: starting main program\n",argv[0]);
#endif
  /* 
     
     (3)
     
     initialize all variables
     
     - choose the internal spherical harmonics convention
     - assign constants
     - assign phase boundaries, if any
     - read in viscosity structure
     - assign density anomalies
     - read in plate velocities
     
  */
  hc_init_main(model,SH_RICK,p);
  nsol = (model->nradp2) * 3;	/* 
				   number of solutions (r,pol,tor) * (nlayer+2) 
				   
				   total number of layers is nlayer +2, 
				   
				   because CMB and surface are added
				   to intermediate layers which are
				   determined by the spacing of the
				   density model
				   
				*/
  if(p->free_slip)		/* maximum degree is determined by the
				   density expansion  */
    lmax = model->dens_anom[0].lmax;
  else				/* max degree is determined by the
				   plate velocities  */
    lmax = model->pvel.p[0].lmax;	/*  shouldn't be larger than that*/
  /* 
     make sure we have room for the plate velocities 
  */
  sh_allocate_and_init(&pvel,2,lmax,model->sh_type,1,p->verbose,FALSE);
  
  /* init done */     

  /*
    SOLUTION PART
    
    
  */
  /* 
     make room for the spectral solution on irregular grid
  */
  sh_allocate_and_init(&sol_spectral,nsol,lmax,model->sh_type,HC_VECTOR,
		       p->verbose,FALSE);
  /* make room for geoid solution at surface */
  sh_allocate_and_init(&geoid,1,model->dens_anom[0].lmax,
		       model->sh_type,HC_SCALAR,p->verbose,FALSE);

  /* BEGIN THB VISCOSITY LOOP */
  int maxit = 1e3;

  // initialize solution
  sol1.nlayer = 2;
  sol1.r[0] = model->r_cmb;
  sol1.r[1] = 1.0;
  sol1.visc[0] = 22.0;
  sol1.visc[1] = 22.0;
  sol1.var = 1.0;
  interpolate_viscosity(&sol1,model->rvisc,model->visc,p);
  /* select plate velocity */
  if(!p->free_slip)
    hc_select_pvel(p->pvel_time,&model->pvel,pvel,p->verbose);
  // do the initial solution
  solved=0;
  hc_solve(model,p->free_slip,p->solution_mode,sol_spectral,
	   (solved)?(FALSE):(TRUE), /* density changed? */
	   (solved)?(FALSE):(TRUE), /* plate velocity changed? */
	   TRUE,			/* viscosity changed */
	   FALSE,p->compute_geoid,
	   pvel,model->dens_anom,geoid,
	   p->verbose);
  solved=1;
  //hc_compute_correlation(geoid,p->ref_geoid,corr,1,p->verbose);
  sh_residual_vector(geoid,p->ref_geoid,thb_ll,thb_nl,residual1,0);
  /* Calculate the Mahalanobis Distance Phi */
  {
    double mdist = 0.0;    
    int i;
    for(i=0;i<thb_nlm;i++){
      mdist += residual1[i]*residual1[i]/sol1.var; // Assumes a diagonal covariance matrix
    }    
    likeprob1 = -0.5 * mdist;
  }
  fprintf(stdout,"%10.7f %10.7f ",
	  (double)corr[0],(double)corr[1]);
  fprintf(stdout,"\n");
  
  int iter=0;
  while(iter < maxit){
    propose_solution( model, &sol1, &sol2, rng, iter, p);
    interpolate_viscosity(&sol2, model->rvisc, model->visc,p);
    hc_solve(model,p->free_slip,p->solution_mode,sol_spectral,
	     (solved)?(FALSE):(TRUE), /* density changed? */
	     (solved)?(FALSE):(TRUE), /* plate velocity changed? */
	     TRUE,			/* viscosity changed */
	     FALSE,p->compute_geoid,
	     pvel,model->dens_anom,geoid,
	     p->verbose); 
    double varfakt = sol2.var / sol1.var;
    //hc_compute_residual(p,geoid,p->ref_geoid,corr,2,p->verbose);
    HC_PREC total_residual2 = sh_residual_vector(geoid,p->ref_geoid,thb_ll,thb_nl,residual2,0);

    /* Calculate the Mahalanobis Distance Phi */
    {
      double mdist = 0.0;    
      int i;
      for(i=0;i<thb_nlm;i++){
	mdist += residual2[i]*residual2[i]/sol2.var; // Assumes a diagonal covariance matrix
      }
      likeprob2 = -0.5*mdist;
    }
    
    /* Calculate the probablity of acceptance: */
    int k2 = sol2.nlayer-1;
    int k1 = sol1.nlayer-1;
    double prefactor = -0.5 * ((double) thb_nlm)*log(varfakt);
    double probAccept = prefactor + likeprob2 - likeprob1 + log((double) (k1+1)) - log((double) (k2+1));
    if( probAccept > 0 || probAccept > log(randDouble())){
      // Accept the proposed solution
      likeprob1 = likeprob2;
      sol1 = sol2;
      if( p->verbose ){
	fprintf(stdout,"Accepted new solution:\n");
	int i;
	for(i=0;i<sol2.nlayer;i++){
	  fprintf(stdout,"%le %le\n",sol2.r[i],sol2.visc[i]);
	}
	fprintf(stdout,"Residual: %le\n",(double) total_residual2);
      }

      
    }


    
    fprintf(stdout,"%10.7f %10.7f\n",(double)corr[0],(double)corr[1]);
    fprintf(stderr,"Finished iteration %d\n",iter);
    
    
    iter++;
  }
  /* END THB VISCOSITY LOOP */  
  /*
     
    free memory

  */
  free(residual1);
  free(residual2);
  sh_free_expansion(sol_spectral,nsol);
  /* local copies of plate velocities */
  sh_free_expansion(pvel,2);
  /*  */
  sh_free_expansion(geoid,1);
  if(p->verbose)
    fprintf(stderr,"%s: done\n",argv[0]);
  hc_struc_free(&model);
  return 0;
}

