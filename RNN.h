
//**********************struct for NEURON**********************************//
typedef struct rNEURON
	{
    double input;
    double output;
    double *weights;
    double grad;			// "local gradient"
    double error;
	} rNEURON;

//**********************struct for LAYER***********************************//
typedef struct rLAYER
	{
    int numNeurons;
    rNEURON *neurons;
	} rLAYER;

//*********************struct for NNET************************************//
typedef struct RNN
	{
    double *inputs;
    int numLayers;
    rLAYER *layers;
	} RNN;

#define dim_K	10
