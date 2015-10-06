// TO-DO:
// * the calculation of "error" in back-prop is unclear

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <time.h>		// time as random seed in create_NN()
#include "RNN.h"

#define Eta 1.0				// learning rate
#define BIASOUTPUT 1.0		// output for bias. It's always 1.

//********sigmoid function and randomWeight generator********************//

double sigmoid(double v)
	{
	#define steepness 1.0
	return 1.0 / (1.0 + exp(-steepness * v));
	}

double randomWeight() // generate random weight between [+0.5,-0.5]
	{
	return (rand() / (double) RAND_MAX) * 1.0 - 0.5;
	}

//****************************create neural network*********************//
// GIVEN: how many layers, and how many neurons in each layer
void create_NN(NNET *net, int numLayers, int *neuronsOfLayer)
	{
	srand(time(NULL));
	net->numLayers = numLayers;

	assert(numLayers >= 3);

	net->layers = (LAYER *) malloc(numLayers * sizeof (LAYER));
	//construct input layer, no weights
	net->layers[0].numNeurons = neuronsOfLayer[0];
	net->layers[0].neurons = (NEURON *) malloc(neuronsOfLayer[0] * sizeof (NEURON));

	//construct hidden layers
	for (int l = 1; l < numLayers; l++) //construct layers
		{
		net->layers[l].neurons = (NEURON *) malloc(neuronsOfLayer[l] * sizeof (NEURON));
		net->layers[l].numNeurons = neuronsOfLayer[l];
		for (int n = 0; n < neuronsOfLayer[l]; n++) // construct each neuron in the layer
			{
			net->layers[l].neurons[n].weights =
					(double *) malloc((neuronsOfLayer[l - 1] + 1) * sizeof (double));
			for (int i = 0; i <= neuronsOfLayer[l - 1]; i++)
				{
				//construct weights of neuron from previous layer neurons
				//when k = 0, it's bias weight
				net->layers[l].neurons[n].weights[i] = randomWeight();
				//net->layers[i].neurons[j].weights[k] = 0.0f;
				}
			}
		}
	}

//**************************** forward-propagation ***************************//

void forward_prop(NNET *net, int dim_V, double V[])
	{
	//set the output of input layer
	//two inputs x1 and x2
	for (int i = 0; i < dim_V; ++i)
		net->layers[0].neurons[i].output = V[i];

	//calculate output from hidden layers to output layer
	for (int i = 1; i < net->numLayers; i++)
		{
		for (int j = 0; j < net->layers[i].numNeurons; j++)
			{
			double v = 0; //induced local field for neurons
			//calculate v, which is the sum of the product of input and weights
			for (int k = 0; k <= net->layers[i - 1].numNeurons; k++)
				{
				if (k == 0)
					v += net->layers[i].neurons[j].weights[k] * BIASOUTPUT;
				else
					v += net->layers[i].neurons[j].weights[k] *
						net->layers[i - 1].neurons[k - 1].output;
				}

			// For the last layer, skip the sigmoid function
			// Note: this idea seems to destroy back-prop convergence
			// if (i == net->numLayers - 1)
			//	net->layers[i].neurons[j].output = v;
			// else
				net->layers[i].neurons[j].output = sigmoid(v);
			}
		}
	}


//****************************** back-propagation ***************************//
// The error "delta" is propagated backwards starting from the output layer, hence the
// name for this algorithm.

// In the update formula, we need to adjust by "η ∙ input ∙ ∆", where η is the learning rate.
// The value of		∆_j = σ'(summed input) Σ_i W_ji ∆_i
// where σ is the sigmoid function, σ' is its derivative.  This formula is obtained directly
// from differentiating the error E with respect to the weights W.

// There is a neat trick for the calculation of σ':  σ'(x) = σ(x) (1−σ(x))
// For its simple derivation you can see this post:
// http://math.stackexchange.com/questions/78575/derivative-of-sigmoid-function-sigma-x-frac11e-x
// Therefore in the code, we use "output * (1 - output)" for the value of "σ'(summed input)",
// because output = σ(summed input), where summed_input_i = Σ_j W_ji input_j.

// Some history:
// It was in 1974-1986 that Paul Werbos, David Rumelhart, Geoffrey Hinton and Ronald Williams
// discovered this algorithm for neural networks, although it has been described by
// Bryson, Denham, and Dreyfus in 1963 and by Bryson and Yu-Chi Ho in 1969 as a solution to
// optimization problems.  The book "Talking Nets" interviewed some of these people.

void back_prop(NNET *net)
	{
	int numLayers = net->numLayers;
	LAYER lastLayer = net->layers[numLayers - 1];

	// calculate ∆ for output layer
	for (int n = 0; n < lastLayer.numNeurons; ++n)
		{
		double output = lastLayer.neurons[n].output;
		double error = lastLayer.neurons[n].error;
		//for output layer, ∆ = y∙(1-y)∙error
		lastLayer.neurons[n].delta = steepness * output * (1.0 - output) * error;
		}

	// calculate ∆ for hidden layers
	for (int l = numLayers - 2; l > 0; --l)		// for each hidden layer
		{
		for (int n = 0; n < net->layers[l].numNeurons; n++)		// for each neuron in layer
			{
			double output = net->layers[l].neurons[n].output;
			double sum = 0.0f;
			LAYER nextLayer = net->layers[l + 1];
			for (int i = 0; i < nextLayer.numNeurons; i++)		// for each weight
				{
				sum += nextLayer.neurons[i].weights[n + 1]		// ignore weights[0] = bias
						* nextLayer.neurons[i].delta;
				}
			net->layers[l].neurons[n].delta = steepness * output * (1.0 - output) * sum;
			}
		}

	// update all weights
	for (int l = 1; l < numLayers; ++l)		// except for 0th layer which has no weights
		{
		for (int n = 0; n < net->layers[l].numNeurons; n++)		// for each neuron
			{
			net->layers[l].neurons[n].weights[0] += Eta * 
					net->layers[l].neurons[n].delta * 1.0;		// 1.0f = bias input
			for (int i = 0; i < net->layers[l - 1].numNeurons; i++)	// for each weight
				{	
				double inputForThisNeuron = net->layers[l - 1].neurons[i].output;
				net->layers[l].neurons[n].weights[i + 1] += Eta *
						net->layers[l].neurons[n].delta * inputForThisNeuron;
				}
			}
		}
	}


// Calculate error between output of forward-prop and a given answer Y
double calc_error(NNET *net, double Y[])
	{
	// calculate mean square error
	// desired value = Y = K* = trainingOUT
	double sumOfSquareError = 0;

	int numLayers = net->numLayers;
	#define LastLayer (net->layers[numLayers - 1])
	// This means each output neuron corresponds to a classification label --YKY
	for (int i = 0; i < LastLayer.numNeurons; i++)
		{
		//error = desired_value - output
		double error = Y[i] - LastLayer.neurons[i].output;
		LastLayer.neurons[i].error = error;
		sumOfSquareError += error * error / 2;
		}
	double mse = sumOfSquareError / LastLayer.numNeurons;
	return mse; //return the root of mean square error
	}

// **************************** Old code, currently not used *****************************

/*
// *************************calculate error average*************
// relative error = |average of second 10 errors : average of first 10 errors - 1|
// It is 0 if the errors stay constant, non-zero if the errors are changing rapidly
// these errors are from the training set --YKY

double relative_error(double error[], int len)
	{
	len = len - 1;
	if (len < 20)
		return 1;
	//keep track of the last 20 Root of Mean Square Errors
	int start1 = len - 20;
	int start2 = len - 10;

	double error1, error2 = 0;

	//calculate the average of the first 10 errors
	for (int i = start1; i < start1 + 10; i++)
		error1 += error[i];
	double averageError1 = error1 / 10;

	//calculate the average of the second 10 errors
	for (int i = start2; i < start2 + 10; i++)
		error2 += error[i];
	double averageError2 = error2 / 10;

	double relativeErr = (averageError1 - averageError2) / averageError1;
	return (relativeErr > 0) ? relativeErr : -relativeErr;
	}

*/