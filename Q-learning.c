// Q-learner
// Basically it is a feed-forward neural net that approximates the Q-value function
// used in reinforcement learning.  Call this neural network the Q-net.
// Q-net accepts K and K' as input.  K is the current state of the Reasoner,
// K' is the "next" state.  Q-net's output is the Q-value, ie, the utility at state
// K making the transition to K'.
// In other words, Q-net approximates the function K x K' -> ℝ.
// Given K, K', and Q, Q-net learns via traditional back-prop.
// The special thing about Q-net is that there is another algorithm that computes,
// when given K, the K' that achieves maximum Q value.  This is the optimization part.

// TO-DO:
// * interface with Tic Tac Toe


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <time.h>
// #include <SDL2/SDL.h>

#include "feedforward-NN.h"

extern double sigmoid(double v);
extern double randomWeight();
extern NNET *create_NN(int numberOfLayers, int *neuronsOfLayer);
extern void forward_prop_sigmoid(NNET *, int, double *);
extern double calc_error(NNET *net, double *Y);
extern void back_prop(NNET *, double *errors);

//************************** prepare Q-net ***********************//
NNET *Qnet;
NNET *Vnet;

NNET *init_Qlearn()
	{
	int numLayers2 = 5;
	//the first layer -- input layer
	//the last layer -- output layer
	// int neuronsOfLayer[5] = {2, 3, 4, 4, 4};
	// int neuronsOfLayer[5] = {18, 18, 15, 10, 1};
	int neuronsOfLayer2[5] = {9, 13, 10, 5, 1};

	Qnet = (NNET*) malloc(sizeof (NNET));
	//create neural network for backpropagation
	Qnet = create_NN(numLayers2, neuronsOfLayer2);

	// SDL_Renderer *gfx = newWindow();		// create graphics window
	return Qnet;
	}

//************************** Q-learning ***********************//
// Algorithm:
// ---- (Part 1) Acting ----
// At some point in the main algorithm, control is passed here.
// At current state K, We pick an optimal action according to Q.
// We make the state transition from K to K', getting reward R.
// ---- (Part 2) Learning ----
// Then we can use R to update the Q value via:
//			Q(K,K') += η { R + γ max_a Q(K',a) }.
// So the above is a ΔQ value that needs to be added to Q(K,K').
// The Q-net computes Q(K,K'); its output should be adjusted by ΔQ.
// Thus we use back-prop to adjust the weights in Q-net to achieve this.
// ==============================================================

// Finds Q value by forward-propagation

double Q(double K[], double K2[])
	{
	// For input, we need to combine K1 and K2 into a single vector
	double K12[dim_K * 2];
	for (int k = 0; k < dim_K; ++k)
		{
		K12[k] = K[k];
		K12[k + dim_K] = K2[k];
		}

	forward_prop_sigmoid(Qnet, dim_K * 2, K12);

	int numLayers = 5;
	LAYER LastLayer = (Qnet->layers[numLayers - 1]);
	// The last layer has only 1 neuron, which outputs the Q value:
	return LastLayer.neurons[0].output;
	}

// returns the Euclidean norm (absolute value, or size) of the gradient vector

double norm(double grad[])
	{
	double r;
	for (int k = 0; k < dim_K; ++k)
		r += (grad[k] * grad[k]);
	return sqrt(r);
	}

int numLayers = 5;
int neuronsOfLayer[] = {9, 40, 30, 20, 1};

void init_Vnet()
	{
	//the first layer -- input layer
	//the last layer -- output layer
	// int neuronsOfLayer[5] = {2, 3, 4, 4, 4};
	// int neuronsOfLayer[5] = {18, 18, 15, 10, 1};
	Vnet = (NNET*) malloc(sizeof (NNET));
	//create neural network for backpropagation
	Vnet = create_NN(numLayers, neuronsOfLayer);

	// return Vnet;
	}

void load_Vnet()
	{
	int numLayers2;
	int *neuronsOfLayer2;
	extern NNET * loadNet(int *, int *p[], char *);
	Vnet = loadNet(&numLayers2, &neuronsOfLayer2, "v.net");
	// LAYER lastLayer = Vnet->layers[numLayers - 1];

	return;
	}

void save_Vnet(char *fname)
	{
	extern void saveNet(NNET *, int, int *, char *, char *);

	saveNet(Vnet, numLayers, neuronsOfLayer, "", fname);
	}

// **** Learn a simple V-value map via backprop and Bellman update

void train_V(int s[9], double V)
	{
	double S[9];

	for (int j = 0; j < 3; ++j)
		{
		for (int k = 0; k < 9; ++k)
			S[k] = (double) s[k];

		forward_prop_sigmoid(Vnet, 9, S);

		int numLayers = 5;
		LAYER LastLayer = (Vnet->layers[numLayers - 1]);
		// The last layer has only 1 neuron, which outputs the Q value:
		double V2 = LastLayer.neurons[0].output;

		double error[1];
		*error = V - V2; // desired - actual

		back_prop(Vnet, error);
		}
	}

// **** Learn a simple V-value map via backprop and Bellman update

void learn_V(int s2[9], int s[9])
	{
	double S2[9], S[9];

	for (int j = 0; j < 4; ++j)
		{

		for (int k = 0; k < 9; ++k)
			{
			S2[k] = (double) s2[k];
			S[k] = (double) s[k];
			}

		forward_prop_sigmoid(Vnet, 9, S2);

		int numLayers = 5;
		LAYER LastLayer = (Vnet->layers[numLayers - 1]);
		// The last layer has only 1 neuron, which outputs the Q value:
		double V2 = LastLayer.neurons[0].output;

		forward_prop_sigmoid(Vnet, 9, S);
		double V = LastLayer.neurons[0].output;

		double error[1];
		*error = V2 - V;

		back_prop(Vnet, error);
		}
	}

// Get V-value by forward propagation

double get_V(int x[9])
	{
	double X[9];
	for (int k = 0; k < 9; ++k)
		X[k] = (double) x[k];

	forward_prop_sigmoid(Vnet, 9, X);

	int numLayers = 5;
	LAYER LastLayer = (Vnet->layers[numLayers - 1]);
	// The last layer has only 1 neuron, which outputs the Q value:
	return LastLayer.neurons[0].output;
	}

// (Part 1) Q-acting:
// Find K2 that maximizes Q(K,K2).  Q is a real number.
// Method: numerical differentiation to find the gradient ∇Q = [∂Q/∂K2] which is a vector.
//		The approximation formula for each component of ∂Q/∂K2 is:  (subscripts omitted)
//			∂Q/∂K2 ≈ { Q(K2 + δ) - Q(K2 - δ) } /2δ
// TO-DO: Perhaps with multiple random restarts
// Note: function changes the components of K2.

void Q_act(double K[], double K2[])
	{
	double gradQ[dim_K]; // the gradient vector ∇Q = [∂Q/∂K2]

	do // While change is smaller than threshold
		{
		// Start with a random K2
		for (int k = 0; k < dim_K; ++k)
			K2[k] = (rand() / (float) RAND_MAX) * 4.0 - 2.0; // in [+2,-2]

		// Find the steepest direction [∂Q/∂K2], using numerical differentiation.
		#define delta	0.01f
		for (int k = 0; k < dim_K; ++k)
			{
			// Create 2 copies of K2, whose k-th component is added / subtracted with δ
			double K2plus[dim_K], K2minus[dim_K];
			for (int k2 = 0; k2 < dim_K; ++k2)
				K2plus[k2] = K2minus[k2] = K2[k2];
			K2plus[k] += delta;
			K2minus[k] -= delta;

			gradQ[k] = (Q(K, K2plus) - Q(K, K2minus)) / 2 / delta;
			}

		// Move a little along the gradient direction: K2 += -λ ∇Q
		// (There seems to be a negative sign in the above formula)
		#define Lambda	0.01f
		for (int k = 0; k < dim_K; ++k)
			K2[k] += (-Lambda * gradQ[k]);
		}
		#define Epsilon 0.01f
		while (norm(gradQ) > Epsilon);

	// Return with optimal K2 value
	}

// Find maximum Q(K,K') value at state K, by varying K'.
// Method: gradient descent, using numerical differentiation to find the gradient [∂Q/∂K'].
// Algorithm is similar to above.

double maxQ(double K[])
	{
	double gradQ[dim_K]; // the gradient vector ∇Q = [∂Q/∂K2]
	double *K2 = (double *) malloc(sizeof (double) * dim_K);

	do // While change is smaller than threshold
		{
		// Start with a random K2
		for (int k = 0; k < dim_K; ++k)
			K2[k] = (rand() / (float) RAND_MAX) * 4.0 - 2.0; // in [+2,-2]

		// Find the steepest direction [∂Q/∂K2], using numerical differentiation.
		#define delta	0.01f
		for (int k = 0; k < dim_K; ++k)
			{
			// Create 2 copies of K2, whose k-th component is added / subtracted with δ
			double K2plus[dim_K], K2minus[dim_K];
			for (int k2 = 0; k2 < dim_K; ++k2)
				K2plus[k2] = K2minus[k2] = K2[k2];
			K2plus[k] += delta;
			K2minus[k] -= delta;

			gradQ[k] = (Q(K, K2plus) - Q(K, K2minus)) / 2 / delta;
			}

		// Move a little along the gradient direction: K2 += -λ ∇Q
		// (There seems to be a negative sign in the above formula)
		#define Lambda	0.01f
		for (int k = 0; k < dim_K; ++k)
			K2[k] += (-Lambda * gradQ[k]);
		}
		#define Epsilon 0.01f
		while (norm(gradQ) > Epsilon);

	free(K2);
	return 0.0f; // returns Q value
	}

// (Part 2) Q-learning:
// Invoke ordinary back-prop to learn Q.
// On entry, we have just made a transition K1 -> K2 with maximal Q(K1, a: K1->K2)
// and gotten a reward R(K1->K2).
// We need to calculate the max value of Q(K2,a) which, beware, is from the NEXT state K2.
// We know old Q(K1,K2), but it is now adjusted to Q += ΔQ, thus the "error" for back-prop
// is ΔQ.

void Q_learn(double K1[], double K2[], double R, double oldQ)
	{
	#define Gamma	0.05
	#define Eta		0.1

	// Calculate ΔQ = η { R + γ max_a Q(K2,a) }
	double dQ = Eta * (R + Gamma * maxQ(K2));

	// Adjust old Q value
	oldQ += dQ;
	// Use dQ as the error for back-prop
	#define LastLayer (Qnet->layers[numLayers - 1])
	// LastLayer.neurons[0].error = dQ;

	// Invoke back-prop a few times (perhaps this would make the learning effect stronger?)
	for (int i = 0; i < 5; ++i)
		back_prop(Qnet, &dQ);
	}
