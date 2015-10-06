#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "RNN.h"

extern void create_NN(NNET *, int, int *);
extern void forward_prop(NNET *, int, double *);
extern void back_prop(NNET *);
extern void pause_graphics();
extern void quit_graphics();
extern void start_NN_plot(void);
extern void start_NN2_plot(void);
extern void start_W_plot(void);
extern void start_K_plot(void);
extern void start_output_plot(void);
extern void plot_NN(NNET *net);
extern void plot_NN2(NNET *net);
extern void plot_W(NNET *net);
extern void plot_output(NNET *net);
extern void flush_output();
extern void plot_tester(double, double);
extern void plot_K();
extern int delay_vis(int);
extern void plot_trainer(double);
extern void plot_ideal(void);
extern void beep(void);
extern double sigmoid(double);

extern double K[];

#define LastLayer (Net->layers[NumLayers - 1])
#define NumLayers (sizeof(neuronsOfLayer) / sizeof(int))

// Randomly generate an RNN, watch it operate on K and see how K moves
// Observation: 
void K_wandering_test()
	{
	NNET *Net = (NNET *) malloc(sizeof (NNET));
	int neuronsOfLayer[3] = {10, 10, 10}; // first = input layer, last = output layer
	create_NN(Net, NumLayers, neuronsOfLayer);
	double K2[dim_K];
	int quit = 0;
	
	start_K_plot();
	printf("Press 'Q' to quit\n\n");

	// Initialize K vector
	for (int k = 0; k < dim_K; ++k)
		K[k] = (rand() / (float) RAND_MAX) - 0.5f;
	
	for (int j = 0; j < 10000; j++) // max number of iterations
		{
		forward_prop(Net, dim_K, K);

		// printf("%02d", j);
		double d = 0.0;

		// copy output to input
		for (int k = 0; k < dim_K; ++k)
			{
			K2[k] = K[k];
			K[k] = LastLayer.neurons[k].output;
			// printf(", %0.4lf", K[k]);
			double diff = (K2[k] - K[k]);
			d += (diff * diff);
			}

		plot_trainer(0);				// required to clear window
		plot_K();
		if (quit = delay_vis(60))		// delay in milliseconds
			break;
		
		// printf("\n");
		if (d < 0.000001)
			{
			fprintf(stderr, "terminated after %d cycles,\t delta = %lf\n", j, d);
			break;
			}
		}
	
	beep();

	if (!quit)
		pause_graphics();
	free(Net);
	}

// Train RNN to reproduce a sine wave time-series
// Train the 0-th component of K to move as sine wave
// This version uses the time-step *differences* to train K
// In other words, K moves like the sine wave, but K's magnitude is free to vary and
// will be different every time this test is called.
void sine_wave_test()
	{
	NNET *Net = (NNET *) malloc(sizeof (NNET));
	int neuronsOfLayer[3] = {10, 12, 10}; // first = input layer, last = output layer
	create_NN(Net, NumLayers, neuronsOfLayer);
	double K2[dim_K];
	int quit;
	double sum_error2;
	#define Pi 3.141592654f

	start_NN_plot();
	start_W_plot();
	start_K_plot();
	printf("Press 'Q' to quit\n\n");
	
	// Initialize K vector
	for (int k = 0; k < dim_K; ++k)
		K[k] = (rand() / (float) RAND_MAX) * 2.0 - 1.0;
	
	for (int i = 0; 1; ++i)
		{
		sum_error2 = 0.0f;

		#define N 20		// loop from 0 to 2π in N divisions
		for (int j = 0; j < N; j++) 
			{
			K[1] = cos(2 * Pi * j / N) + 1.0f;		// Phase information to aid learning
			
			// Allow multiple forward propagations
			forward_prop(Net, dim_K, K);

			// The difference between K[0] and K'[0] should be equal to [sin(θ+dθ) - sinθ]
			// where θ = 2π j/60.
			#define Amplitude 0.5f
			double dK_star = Amplitude * ( sin(2*Pi * (j+1) / N) - sin(2*Pi * j / N) );

			// Calculate actual difference between K[0] and K'[0]:
			double dK = LastLayer.neurons[0].output - K[0];

			// The error is the difference between the above two values:
			double error = dK_star - dK;

			// Error in the back-prop NN is recorded as [ideal - actual]:
			//		K* - K = dK*
			//		K' - K = dK
			// thus, K* - k' = dK* - dK 
			LastLayer.neurons[0].error = error;

			// The rest of the errors are zero:
			for (int k = 1; k < dim_K; ++k)
				LastLayer.neurons[k].error = 0.0f;

			back_prop(Net);
			
			// copy output to input
			for (int k = 0; k < dim_K; ++k)
				K[k] = LastLayer.neurons[k].output;

			sum_error2 += (error * error);		// record sum of squared errors
			
			plot_W(Net);
			plot_NN(Net);
			plot_trainer(dK_star / 5.0 * N);
			plot_K();
			if (quit = delay_vis(0))
				break;
			}

		printf("iteration: %05d, error: %lf\n", i, sum_error2);
		if (isnan(sum_error2))
			break;
		
		if (sum_error2 < 0.01)
			break;
		
		if (quit)
			break;
		}
	
	if (!quit)
		pause_graphics();
	free(Net);
	}

// Train RNN to reproduce a sine wave time-series
// Train the 0-th component of K to move as sine wave
// This version uses the actual value of sine to train K
// New idea: allow RNN to act *multiple* times within each step of the sine wave.
// This will stretch the time scale arbitrarily so the "sine" shape will be lost, but
// I think this kind of learning is more suitable for this RNN model's capability.

// Currently this test fails miserably because the training error is jumping all around
// the place and so BP fails to converge.
void sine_wave_test2()
	{
	NNET *Net = (NNET *) malloc(sizeof (NNET));
	int neuronsOfLayer[3] = {10, 7, 10}; // first = input layer, last = output layer
	create_NN(Net, NumLayers, neuronsOfLayer);
	double K2[dim_K];
	int quit;
	double sum_error2;
	#define Pi 3.141592654f

	start_NN_plot();
	start_W_plot();
	start_K_plot();
	printf("Press 'Q' to quit\n\n");
	
	// Initialize K vector
	for (int k = 0; k < dim_K; ++k)
		K[k] = (rand() / (float) RAND_MAX) * 1.0f;
	
	for (int i = 0; 1; ++i)
		{
		sum_error2 = 0.0f;

		#define N2 10		// loop from 0 to 2π in N divisions
		for (int j = 0; j < N2; j++) 
			{
			// K[1] = cos(2 * Pi * j / N2);		// Phase information to aid learning

			forward_prop(Net, dim_K, K);

			// Desired value
			#define Amplitude2 1.0f
			double K_star = Amplitude2 * ( sin(2 * Pi * j / N2) ) + 1.0f;

			// Difference between actual outcome and desired value:
			double error = LastLayer.neurons[0].output - K_star;
			LastLayer.neurons[0].error = error;

			// The rest of the errors are zero:
			for (int k = 1; k < dim_K; ++k)
				LastLayer.neurons[k].error = 0.0f;

			back_prop(Net);

			// copy output to input
			for (int k = 0; k < dim_K; ++k)
				K[k] = LastLayer.neurons[k].output;

			sum_error2 += (error * error);		// record sum of squared errors
			
			plot_W(Net);
			plot_NN(Net);
			plot_trainer(K_star);
			plot_K();
			if (quit = delay_vis(0))
				break;
			}

		printf("iteration: %05d, error: %lf\n", i, sum_error2);
		if (isnan(sum_error2))
			break;
		
		if (sum_error2 < 0.01)
			break;
		
		if (quit)
			break;
		}
	
	if (!quit)
		pause_graphics();
	free(Net);
	}

// Test classical back-prop
// To test convergence, we record the sum of squared errors for the last N and last 2N~N
// trials, then compare their ratio.
void classic_BP_test()
	{
	int neuronsOfLayer[3] = {2, 3, 1}; // first = input layer, last = output layer
	NNET *Net = (NNET *) malloc(sizeof (NNET));
	create_NN(Net, NumLayers, neuronsOfLayer);

	int quit = 0;
	#define M	100			// how many errors to record for averaging
	double errors1[M], errors2[M];	// two arrays for recording errors
	double sum_err1 = 0.0, sum_err2 = 0.0;	// sums of errors
	int tail = 0;			// index for cyclic arrays (last-in, first-out)
	
	for (int i = 0; i < M; ++i)			// clear errors to 0.0
		errors1[i] = errors2[i] = 0.0;
	
	// start_NN_plot();
	start_W_plot();
	// start_K_plot();
	start_output_plot();
	plot_ideal();
	printf("Press 'Q' to quit\n\n");
	
	for (int i = 0; 1; ++i)
		{
		printf("iteration: %05d: ", i);

		// Create random K vector
		for (int k = 0; k < 2; ++k)
			K[k] = (rand() / (float) RAND_MAX);
		// printf("*** K = <%lf, %lf>\n", K[0], K[1]);
		
		if ((i % 4) == 0)
			K[0] = 1.0, K[1] = 0.0;
		if ((i % 4) == 1)
			K[0] = 0.0, K[1] = 0.0;
		if ((i % 4) == 2)
			K[0] = 0.0, K[1] = 1.0;
		if ((i % 4) == 3)
			K[0] = 1.0, K[1] = 1.0;

		forward_prop(Net, 2, K);		// dim K = 2

		// Desired value = K_star
		double training_err = 0.0;
		for (int k = 0; k < 1; ++k)
			{
			// double ideal = K[k];				/* identity function */
			#define f2b(x) (x > 0.5f ? 1 : 0)	// convert float to binary
			// ^ = binary XOR
			double ideal = ((double) (f2b(K[0]) ^ f2b(K[1]))); // ^ f2b(K[2]) ^ f2b(K[3])))
			// #define Ideal ((double) (f2b(K[k]) ^ f2b(K[2]) ^ f2b(K[3])))
			// double ideal = 1.0f - (0.5f - K[0]) * (0.5f - K[1]);
			// printf("*** ideal = %lf\n", ideal);
			
			// Difference between actual outcome and desired value:
			double error = ideal - LastLayer.neurons[k].output;
			LastLayer.neurons[k].error = error;		// record this for back-prop

			training_err += (error * error);		// record sum of squared errors
			}
		printf("sum of squared error = %lf  ", training_err);
		
		// update error arrays cyclically
		// (This is easier to understand by referring to the next block of code)
		sum_err2 -= errors2[tail];
		sum_err2 += errors1[tail];
		sum_err1 -= errors1[tail];
		sum_err1 += training_err;
		// printf("sum1, sum2 = %lf %lf\n", sum_err1, sum_err2);
		
		// record new error in cyclic arrays
		errors2[tail] = errors1[tail];
		errors1[tail] = training_err;
		++tail;
		if (tail == M)				// loop back in cycle
			tail = 0;

		back_prop(Net);

		// Testing set
		double test_err = 0.0;
		for (int j = 0; j < 20; ++j)
			{
			// Create random K vector
			for (int k = 0; k < 2; ++k)
				K[k] = ((double) rand() / (double) RAND_MAX);
			// plot_tester(K[0], K[1]);
			
			forward_prop(Net, 2, K);

			// Desired value = K_star
			double single_err = 0.0;
			for (int k = 0; k < 1; ++k)
				{
				// double ideal = 1.0f - (0.5f - K[0]) * (0.5f - K[1]);
				double ideal = (double) (f2b(K[0]) ^ f2b(K[1]));
				// double ideal = K[k];				/* identity function */

				// Difference between actual outcome and desired value:
				double error = ideal - LastLayer.neurons[k].output;

				single_err += (error * error);		// record sum of squared errors
				}
			test_err += single_err;
			}
		test_err /= 20.0f;
		printf("random test error = %1.06lf  ", test_err);

		double ratio = sum_err1 / (sum_err1 + sum_err2);
		printf("error ratio = %1.05lf\n", ratio);

		if ((i % 200) == 0)			// display status periodically
			{
			plot_output(Net);
			flush_output();
			plot_W(Net);
			// plot_NN(Net);
			plot_trainer(0);		// required to clear the window
			// plot_K();
			if (quit = delay_vis(0))
				break;
			}
		
		if (isnan(ratio))
			break;
		// if (ratio - 0.5f < 0.0000001)	// ratio == 0.5 means stationary
		// if (test_err < 0.01)
		if (training_err < 0.01)
			break;
		}

	beep();
	plot_output(Net);
	flush_output();
	plot_W(Net);
	
	if (!quit)
		pause_graphics();
	else
		quit_graphics();
	free(Net);
	}

// Test forward propagation
void forward_test()
	{
	NNET *Net = (NNET *) malloc(sizeof (NNET));
	int neuronsOfLayer[4] = {4, 3, 3, 2}; // first = input layer, last = output layer
	create_NN(Net, NumLayers, neuronsOfLayer);
	double sum_error2;

	start_NN_plot();
	start_W_plot();
	start_K_plot();
	
	printf("This test sets all the weights to 1, then compares the output with\n");
	printf("the test's own calculation, with 100 randomized inputs.\n\n");
	
	// Set all weights to 1
	for (int l = 1; l < NumLayers; l++)		// for each layer
		for (int n = 0; n < neuronsOfLayer[l]; n++)		// for each neuron
			for (int k = 0; k <= neuronsOfLayer[l - 1]; k++)	// for each weight
				Net->layers[l].neurons[n].weights[k] = 1.0f;

	for (int i = 0; i < 100; ++i)
		{
		// Randomize K
		double sum = 1.0f;
		for (int k = 0; k < 4; ++k)
			{
			K[k] = (rand() / (float) RAND_MAX) * 2.0 - 1.0;
			sum += K[k];
			}

		forward_prop(Net, 4, K);

		// Expected output value:
		double K_star = sigmoid(3.0f * sigmoid(3.0f * sigmoid(sum) + 1.0f) + 1.0f);
		
		// Calculate error
		sum_error2 = 0.0f;
		for (int k = 0; k < 2; ++k)
			{
			// Difference between actual outcome and desired value:
			double error = LastLayer.neurons[k].output - K_star;
			sum_error2 += (error * error);		// record sum of squared errors
			}
		
		plot_W(Net);
		plot_NN(Net);
		plot_trainer(0);
		plot_K();
		delay_vis(50);
		
		printf("iteration: %05d, error: %lf\n", i, sum_error2);		
		}
	
	pause_graphics();
	free(Net);
	}


// Randomly generate a loop of K vectors;  make the RNN learn to traverse this loop.
void loop_dance_test()
	{
	NNET *Net = (NNET *) malloc(sizeof (NNET));
	int neuronsOfLayer[4] = {dim_K, 10, 10, dim_K}; // first = input layer, last = output layer
	create_NN(Net, NumLayers, neuronsOfLayer);
	double sum_error2;
	int quit;

	// start_NN_plot();
	start_W_plot();
	start_K_plot();

	printf("Randomly generate a loop of K vectors;\n");
	printf("Make the RNN learn to traverse this loop.\n\n");

	#define LoopLength 3
	double Kn[LoopLength][dim_K];
	for (int i = 0; i < LoopLength; ++i)
		for (int k = 0; k < dim_K; ++k)
			Kn[i][k] = (rand() / (float) RAND_MAX);			// random in [0,1]

	for (int j = 0; 1; ++j)			// iterations
		{
		sum_error2 = 0.0f;

		for (int i = 0; i < LoopLength; ++i)			// do one loop
			{
			forward_prop(Net, dim_K, K);

			// Expected output value = Kn[i][k].
			// Calculate error:
			for (int k = 0; k < dim_K; ++k)
				{
				// Difference between actual outcome and desired value:
				double error = LastLayer.neurons[k].output - Kn[i][k];
				LastLayer.neurons[k].error = error;		// record this for back-prop
				sum_error2 += (error * error);		// record sum of squared errors

				// copy output to input
				K[k] = LastLayer.neurons[k].output;
				}

			back_prop(Net);

			plot_W(Net);
			// plot_NN(Net);
			plot_trainer(0);
			plot_K();
			if (quit = delay_vis(0))
				break;
			}

		printf("iteration: %05d, error: %lf\n", j, sum_error2);
		if (quit)
			break;
		}

	pause_graphics();
	free(Net);
	}

// **************** 2-Digit Primary-school Subtraction Arithmetic test *****************

// The goal is to perform subtraction like a human child would.
// Input: 2-digit numbers A and B, for example "12", "07"
// Output: A - B, eg:  "12" - "07" = "05"

// State vector = [ A1, A0, B1, B0, C1, C0, carry-flag, current-digit, result-ready-flag,
//		underflow-error-flag ]

// Algorithm:

// If current-digit = 0:
//		if A0 >= B0 then C0 = A0 - B0
//		else C0 = 10 + (A0 - B0) , carry-flag = 1
//		current-digit = 1

// If current-digit = 1:
//		if A1 >= B1 then
//			C1 = A1 - B1
//		else Underflow Error
//		if carry-flag = 0:
//			result-ready = 1
//		else	// carry-flag = 1
//			if C1 >= 1
//				--C1
//			else Underflow error
//			result-ready = 1

// This defines the transition operator acting on vector space K1 (of dimension 10)
void transition(double K1[], double K2[])
	{
	double A1 = floor(K1[0] * 10.0) / 10.0;
	double A0 = floor(K1[1] * 10.0) / 10.0;
	double B1 = floor(K1[2] * 10.0) / 10.0;
	double B0 = floor(K1[3] * 10.0) / 10.0;
	double C1 = K1[4];
	double C0 = K1[5];
	double carryFlag = K1[6];
	double currentDigit = K1[7];
	double resultReady = K1[8];
	double underflowError = K1[9];

	if (currentDigit <= 0.5)
		{
		if (A0 >= B0)
			{
			C0 = A0 - B0;
			carryFlag = 0.0;
			}
		else
			{
			C0 = 1.0 + (A0 - B0);
			carryFlag = 1.0;
			}
		currentDigit = 1.0;
		resultReady = 0.0;
		underflowError = 0.0;
		}
	else		// current digit = 1
		{
		if (A1 >= B1)
			{
			C1 = A1 - B1;
			underflowError = 0.0;
			}
		else
			underflowError = 1.0;

		if (carryFlag <= 0.5)
			resultReady = 1.0;
		else
			{
			if (C1 >= 0.0999)
				C1 -= 0.1;
			else
				underflowError = 1.0;
			resultReady = 1.0;
			}
		}
	
	K2[0] = A1;
	K2[1] = A0;
	K2[2] = B1;
	K2[3] = B0;
	K2[4] = C1;
	K2[5] = C0;
	K2[6] = carryFlag;
	K2[7] = currentDigit;
	K2[8] = resultReady;
	K2[9] = underflowError;
	}

// Test the transition operator (1 time)
void arithmetic_test_1()
	{
	double K1[10], K2[10];
	double a1, a0, b1, b0;
	
	// generate A,B randomly
	a1 = floor((rand() / (float) RAND_MAX) * 10.0) / 10.0;
	K1[0] = a1;
	a0 = floor((rand() / (float) RAND_MAX) * 10.0) / 10.0;
	K1[1] = a0;
	b1 = floor((rand() / (float) RAND_MAX) * 10.0) / 10.0;
	K1[2] = b1;
	b0 = floor((rand() / (float) RAND_MAX) * 10.0) / 10.0;
	K1[3] = b0;

	#define digit(d)	((int)(d * 10.0 + 0.001) + '0')
	printf("%c%c - %c%c\n", digit(a1), digit(a0), digit(b1), digit(b0));
	
	K1[6] = 0.0;
	K1[7] = 0.0;
	K1[8] = 0.0;
	K1[9] = 0.0;
	
	LOOP:
	// call the transition
	transition(K1, K2);
	
	// get result
	if (K2[8] >= 0.5)			// result ready?
		{
		int correct = 1;
		
		// correct answer
		int a = floor(a1 * 10) * 10 + a0 * 10;
		int b = floor(b1 * 10) * 10 + b0 * 10;
		printf("a = %d\n", a);
		printf("b = %d\n", b);

		int c = a - b;
		printf("c = %d\n", c);
		double c1 = (double)(c / 10) / 10.0;
		double c0 = (double)(c % 10) / 10.0;
		
		if (c < 0)				// underflow
			{
			if (K2[9] < 0.5)
				correct = 0;
			}
		else
			{
			if (K2[9] >= 0.5)
				correct = 0;

			if (K2[4] - c1 > 0.00001)
				correct = 0;
			if (K2[5] - c0 > 0.00001)
				correct = 0;
			}
		
		printf(" answer = %c%c\n", digit(c1), digit(c0));
		printf(" genifer = %c%c\n", digit(K2[4]), digit(K2[5]));
		
		if (correct)
			printf("Yes!\n");
		else
			{
			printf("\x1b[31m*************** wrong! **************** \x1b[39;49m\n");
			beep();
			}
		}
	else
		{
		for (int k = 0; k < 10; ++k)
			K1[k] = K2[k];
		goto LOOP;
		}
	}

// Repeat the test N times
void arithmetic_test()
	{
	for (int n = 0; n < 100; ++n)
		{
		arithmetic_test_1();
		printf("\n");
		}
	}

// At this point we are able to generate training examples for the transition operator.
// Perhaps now get to the main code to see if R can approximate this operator well?
// The learning algorithm would be to learn the transition operator as one single step.
// This should be very simple and back-prop would do.