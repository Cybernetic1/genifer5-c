#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <gsl/gsl_matrix.h>		// GNU scientific library
#include <gsl/gsl_eigen.h>		// ...for finding matrix eigen values
#include <gsl/gsl_complex_math.h>	// ...for complex abs value
#include <stdbool.h>
#include "BPTT-RNN.h"
#include "feedforward-NN.h"

extern NNET *create_NN(int, int *);
extern void re_randomize(NNET *, int, int *);
extern RNN *create_BPTT_NN(int, int *);
extern void BPTT_re_randomize(RNN *, int, int *);
extern void free_NN(NNET *, int *);
extern void free_BPTT_NN(RNN *, int *);
extern void forward_prop(NNET *, int, double *);
extern void forward_prop_ReLU(NNET *, int, double *);
extern void forward_prop_SP(NNET *, int, double *);
extern void forward_BPTT(RNN *, int, double *, int);
extern void back_prop(NNET *, double *);
extern void back_prop_ReLU(NNET *, double *);
extern void backprop_through_time(RNN *, double *, int);
extern void pause_graphics();
extern void quit_graphics();
extern void start_NN_plot(void);
extern void start_NN2_plot(void);
extern void start_W_plot(void);
extern void start_K_plot(void);
extern void start_output_plot(void);
extern void start_LogErr_plot(void);
extern void restart_LogErr_plot(void);
extern void plot_NN(NNET *net);
extern void plot_NN2(NNET *net);
extern void plot_W(NNET *net);
extern void plot_W_BPTT(RNN *net);
extern void plot_output(NNET *net, void ());
extern void plot_LogErr(double, double);
extern void flush_output();
extern void plot_tester(double, double);
extern void plot_K();
extern int  delay_vis(int);
extern void pause_key();
extern void plot_trainer(double);
extern void plot_ideal(void);
extern void beep(void);
extern double sigmoid(double);
extern void start_timer(), end_timer();

extern double K[];

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
	double carryFlag = K1[4];
	double currentDigit = K1[5];
	double C1 = K1[6];
	double C0 = K1[7];
	double resultReady = K1[8];
	double underflowError = K1[9];

	if (currentDigit < 0.5)
		{
		if (A0 >= B0) // C seems to support >= for comparison of doubles
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
		C1 = 0.0; // optional
		}
	else // current digit = 1
		{
		resultReady = 1.0;

		if (A1 >= B1)
			{
			C1 = A1 - B1;
			underflowError = 0.0;
			}
		else
			{
			underflowError = 1.0;
			C1 = 0.0; // optional
			}

		if (carryFlag > 0.5)
			{
			if (C1 > 0.09999)
				C1 -= 0.1;
			else
				underflowError = 1.0;
			}

		C0 = C0; // necessary
		carryFlag = 0.0; // optional
		currentDigit = 1.0; // optional
		}

	K2[0] = A1;
	K2[1] = A0;
	K2[2] = B1;
	K2[3] = B0;
	K2[4] = carryFlag;
	K2[5] = currentDigit;
	K2[6] = C1;
	K2[7] = C0;
	K2[8] = resultReady;
	K2[9] = underflowError;
	}

// Test the transition operator (1 time)
// This tests both the arithmetics of the digits as well as the settings of flags.

void arithmetic_testA_1()
	{
	double K1[10], K2[10];
	double a1, a0, b1, b0;

	// generate A,B randomly
	a1 = floor((rand() / (double) RAND_MAX) * 10.0) / 10.0;
	K1[0] = a1;
	a0 = floor((rand() / (double) RAND_MAX) * 10.0) / 10.0;
	K1[1] = a0;
	b1 = floor((rand() / (double) RAND_MAX) * 10.0) / 10.0;
	K1[2] = b1;
	b0 = floor((rand() / (double) RAND_MAX) * 10.0) / 10.0;
	K1[3] = b0;

	#define digit(d)	((int)(d * 10.0 + 0.001) + '0')
	printf("%c%c, %c%c:    ", digit(a1), digit(a0), digit(b1), digit(b0));

	K1[4] = 0.0;
	K1[5] = 0.0;
	K1[6] = 0.0;
	K1[7] = 0.0;
	K1[8] = 0.0;
	K1[9] = 0.0;

LOOP:
	// call the transition
	transition(K1, K2);

	// get result
	if (K2[8] > 0.5) // result ready?
		{
		bool correct = true;

		// correct answer
		int a = floor(a1 * 10) * 10 + a0 * 10;
		int b = floor(b1 * 10) * 10 + b0 * 10;
		printf("%d - ", a);
		printf("%d = ", b);

		int c = a - b;
		printf("%d\n", c);
		double c1 = (double) (c / 10) / 10.0;
		double c0 = (double) (c % 10) / 10.0;

		if (c < 0) // result is negative
			{
			if (K2[9] < 0.5) // underflow should be set but is not
				correct = false;
			}
		else
			{
			if (K2[9] > 0.5) // underflow should be clear but is set
				correct = false;

			double err1 = fabs(K2[6] - c1);
			double err2 = fabs(K2[7] - c0);
			if (err1 > 0.001)
				correct = false;
			if (err2 > 0.001)
				correct = false;
			}

		printf(" answer = %c%c\t", digit(c1), digit(c0));
		printf(" genifer = %c%c\n", digit(K2[6]), digit(K2[7]));

		if (correct)
			printf("\x1b[32m**** Yes!!!!!!! \x1b[39;49m\n");
		else
			{
			printf("\x1b[31mWrong!!!!!! ");
			printf("K2[9] = %f \x1b[39;49m\n", K2[9]);
			// beep();
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

void arithmetic_testA()
	{
	for (int n = 0; n < 100; ++n)
		{
		printf("(%d) ", n);
		arithmetic_testA_1();
		}
	}

// At this point we are able to generate training examples for the transition operator.
// Perhaps now get to the main code to see if R can approximate this operator well?
// The learning algorithm would be to learn the transition operator as one single step.
// This should be very simple and back-prop would do.
#define ForwardPropMethod	forward_prop_ReLU
#define ErrorThreshold		0.001

// ************************* EXPERIMENT RESULTS ************************
// Topology = {8, 13, 10, 6} (4 layers)
// Success:  time = 30:33, back_prop with sigmoid, steepness = 3.0, learning rate = 0.05
// Fail:  time > 123:00, back_prop_SP, slope = 1.5, learning rate = 0.05

// Soft-max success:  topology = {8, 19, 19, 19, 19, 6}
// Slope = 1.0, learning rate = 0.05, time < 5 mins produced 100% correct results
// (under this setting, zero-weights "death" occurs in the majority of cases,
// convergence occurs is observed only rarely (~ 1 in 5-10 times).
// For topology {8, 33, 33, 33, 6}, seems always fail.

// ReLU success:  topology = {8, 19, 19, 19, 19, 6}  descension is rapid!!!
// time 2.45, learning rate = 0.01, leakage = 0.05 or 0.1.
// rectifier(v) = if (v < 0) L*v else v.
// *********************************************************************

/* How to calculate "online" linear regression:
init(): meanX = 0, meanY = 0, varX = 0, covXY = 0, n = 0

update(x,y):
n += 1
dx = x - meanX
dy = y - meanY
varX += (((n-1)/n)*dx*dx - varX)/n
covXY += (((n-1)/n)*dx*dy - covXY)/n
meanX += dx/n
meanY += dy/n

getA(): return covXY/varX
getB(): return meanY - getA()*meanX
*/

void arithmetic_testB()
	{
	// int neuronsOfLayer[] = {8, 13, 10, 6}; // first = input layer, last = output layer
	// int neuronsOfLayer[] = {8, 13, 10, 13, 10, 6};
	// int neuronsOfLayer[] = {8, 19, 19, 19, 19, 6};
	int neuronsOfLayer[] = {8, 13, 10, 6};
	int dimK = 8;
	int numLayers = sizeof(neuronsOfLayer) / sizeof(int);
	NNET *Net = create_NN(numLayers, neuronsOfLayer);
	LAYER lastLayer = Net->layers[numLayers - 1];
	double errors[dimK];

	#define M	50			// how many errors to record for averaging
	double errors1[M], errors2[M]; // two arrays for recording errors

	int userKey = 0;
	double sum_err1 = 0.0, sum_err2 = 0.0; // sums of errors
	int tail = 0; // index for cyclic arrays (last-in, first-out)
	for (int i = 0; i < M; ++i) // clear errors to 0.0
		errors1[i] = errors2[i] = 0.0;

	// start_NN_plot();
	start_W_plot();
	start_LogErr_plot();
	// start_K_plot();
	// start_output_plot();
	// plot_ideal();
	start_timer();
	printf("[Q] quit\n\n");

	char status[1000], *s;
	for (int i = 1; true; ++i)
		{
		s = status + sprintf(status, "[%05d] ", i);

		// Create random K vector (4 + 2 + 2 elements)
		for (int k = 0; k < 4; ++k)
			K[k] = floor((rand() / (double) RAND_MAX) * 10.0) / 10.0;
		for (int k = 4; k < 6; ++k)
			K[k] = (rand() / (double) RAND_MAX) > 0.5 ? 1.0 : 0.0;
		for (int k = 6; k < 8; ++k)
			K[k] = floor((rand() / (double) RAND_MAX) * 10.0) / 10.0;
		// printf("*** K = <%lf, %lf>\n", K[0], K[1]);

		ForwardPropMethod(Net, dimK, K); // dim K = 8 (dimension of input-layer vector)

		// Desired value = K_star
		double K_star[10];
		transition(K, K_star);

		// Difference between actual outcome and desired value:
		double training_err = 0.0;
		for (int k = 4; k < 10; ++k)
			{
			double error = K_star[k] - lastLayer.neurons[k - 4].output;
			errors[k - 4] = error; // record this for back-prop

			training_err += fabs(error); // record sum of errors
			}
		s += sprintf(s, "|e|=%lf, ", training_err);

		// Update error arrays cyclically
		// (This is easier to understand by referring to the next block of code)
		sum_err2 -= errors2[tail];
		sum_err2 += errors1[tail];
		sum_err1 -= errors1[tail];
		sum_err1 += training_err;
		// printf("sum1, sum2 = %lf %lf\n", sum_err1, sum_err2);

		double mean_err = (i < M) ? (sum_err1 / i) : (sum_err1 / M);
		s += sprintf(s, "mean |e|=%lf, ", mean_err);

		if (mean_err < ErrorThreshold)
			break;
		if (mean_err > 1e10 || mean_err < 0.0 || (i > 5000 && isnan(mean_err)))
			{
			printf("%s\n", status);
			printf("Error overflow.\n");
			beep();
			pause_key();
			userKey = 3;
			}

		// record new error in cyclic arrays
		errors2[tail] = errors1[tail];
		errors1[tail] = training_err;
		++tail;
		if (tail == M) // loop back in cycle
			tail = 0;

		back_prop(Net, errors); // train the network!

		// Testing set
		if ((i % 5000) == 0)
			{
			#define NumTrials 20
			double test_err = 0.0;
			for (int j = 0; j < NumTrials; ++j)
				{
				// Create random K vector (4 + 2 + 2 elements)
				for (int k = 0; k < 4; ++k)
					K[k] = floor((rand() / (double) RAND_MAX) * 10.0) / 10.0;
				for (int k = 4; k < 6; ++k)
					K[k] = (rand() / (double) RAND_MAX) > 0.5 ? 1.0 : 0.0;
				for (int k = 6; k < 8; ++k)
					K[k] = floor((rand() / (double) RAND_MAX) * 10.0) / 10.0;
				// plot_tester(K[0], K[1]);

				ForwardPropMethod(Net, dimK, K); // input vector dimension = 8

				// Desired value = K_star
				double K_star[10];
				transition(K, K_star);

				double single_err = 0.0;
				for (int k = 4; k < 10; ++k)
					{
					double error = K_star[k] - lastLayer.neurons[k - 4].output;
					single_err += fabs(error); // record sum of errors
					}
				test_err += single_err;
				}
			test_err /= (float) NumTrials;
			s += sprintf(s, "random test e=%1.06lf, ", test_err);

			double ratio = (sum_err2 - sum_err1) / sum_err1;
			if (ratio > 0)
				s += sprintf(s, "|e| ratio=%f", ratio);
			else
				s += sprintf(s, "|e| ratio=\x1b[31m%f\x1b[39;49m", ratio);

			// if (isnan(ratio))
			//	break;
			// if (ratio - 0.5f < 0.0000001)	// ratio == 0.5 means stationary
			// if (test_err < 0.01)
			if (test_err < ErrorThreshold)
				break;
			}

		if ((i % 500) == 0) // display status periodically
			{
			printf("%s\r", status);
			// plot_output(Net);
			// flush_output();
			plot_W(Net);
			plot_LogErr(mean_err, ErrorThreshold);
			// plot_NN(Net);
			// plot_trainer(0);		// required to clear the window
			// plot_K();
			userKey = delay_vis(0);
			}

		if (userKey == 1)
			break;
		else if (userKey == 3)			// Re-start with new random weights
			{
			re_randomize(Net, numLayers, neuronsOfLayer);
			userKey = 0;
			sum_err1 = 0.0; sum_err2 = 0.0;
			tail = 0;
			for (int j = 0; j < M; ++j) // clear errors to 0.0
				errors1[j] = errors2[j] = 0.0;
			i = 1;

			restart_LogErr_plot();
			start_timer();
			printf("\n****** Network re-randomized.\n");
			userKey = 0;
			beep();
			// pause_key();
			}
		else if (userKey == 2)
			// Test learned operator
			{
			printf("\n");
			int ans_correct = 0, ans_negative = 0, ans_wrong = 0, ans_non_term = 0;
			#define P 100
			for (int i = 0; i < P; ++i)
				{
				printf("(%d) ", i);
				switch (arithmetic_testC_1(Net, lastLayer))
					{
					case 1:
						++ans_correct;
						break;
					case 2:
						++ans_negative;
						break;
					case 3:
						++ans_wrong;
						break;
					case 4:
						++ans_non_term;
						break;
					default:
						printf("Answer error!\n");
						break;
					}
				}

			printf("\n=======================\n");
			printf("Answers correct  = %d (%.1f%%)\n", ans_correct, ans_correct * 100 / (float) P);
			printf("Answers negative = %d (%.1f%%)\n", ans_negative, ans_negative * 100 / (float) P);
			printf("Answers wrong    = %d (%.1f%%)\n", ans_wrong, ans_wrong * 100 / (float) P);
			printf("Answers non-term = %d (%.1f%%)\n", ans_non_term, ans_non_term * 100 / (float) P);
			printf("\n");
			userKey = 0;
			pause_key();
			}
		}

	printf("Terminated....\n");
	printf("%s\n", status);
	end_timer(NULL);
	if (userKey == 0)		// terminated successfully? (not 'quit' key)
		beep();
	// plot_output(Net);
	// flush_output();
	plot_W(Net);

	/****
	printf("\n\nTest with: 73 - 37 = 36.\n");
	K[0] = 0.7;		// A1
	K[1] = 0.3;		// A0
	K[2] = 0.3;		// B1
	K[3] = 0.7;		// B0
	K[4] = 0.0;		// carry
	K[5] = 0.0;		// current digit
	K[6] = 0.0;		// C1
	K[7] = 0.0;		// C0
	K[8] = 0.0;		// ready
	K[9] = 0.0;		// overflow
	forward_prop(Net, 8, K);
	printf("carry [1.0] = %f\n", lastLayer.neurons[0].output);
	printf("current-digit [1.0] = %f\n", lastLayer.neurons[1].output);
	printf("C1 [0.0] = %f\n", lastLayer.neurons[2].output);
	printf("C0 [0.6] = %f\n", lastLayer.neurons[3].output);
	printf("ready [0.0] = %f\n", lastLayer.neurons[4].output);
	printf("overflow [0.0] = %f\n", lastLayer.neurons[5].output);

	// copy output back to K;  second iteration
	for (int k = 0; k < 6; ++k)
		K[k + 4] = lastLayer.neurons[k].output;
	forward_prop(Net, 8, K);
	printf("\ncarry [0.0] = %f\n", lastLayer.neurons[0].output);
	printf("current-digit [1.0] = %f\n", lastLayer.neurons[1].output);
	printf("C1 [0.3] = %f\n", lastLayer.neurons[2].output);
	printf("C0 [0.6] = %f\n", lastLayer.neurons[3].output);
	printf("ready [1.0] = %f\n", lastLayer.neurons[4].output);
	printf("overflow [0.0] = %f\n", lastLayer.neurons[5].output);
	 ****/

	if (userKey == 0)
		pause_graphics();
	else
		quit_graphics();

	extern void saveNet();
	printf("Saving network data....\n");
	saveNet(Net, numLayers, neuronsOfLayer, "");
	free_NN(Net, neuronsOfLayer);
	}

void saveNet(NNET *net, int numLayers, int *neuronsOfLayer, char *comments, char *defaultName)
	{
	FILE *fp;
	if (strlen(defaultName) > 0)
		{
		fp = fopen(defaultName, "w");
		}
	else
		{
		char fileName[1024];
		printf("Enter file name [default = %s] :", defaultName);
		int c;
		while ( (c = getchar()) != EOF && c != '\n' )
			;
		fgets(fileName, sizeof(fileName), stdin);
		fileName[strlen(fileName) - 1] = '\0';
		if (strlen(fileName) == 0)
			return;
		fp = fopen(fileName, "w");
		}
	fprintf(fp, "%s\n", comments);
	#define EndOfComments	"**************\n"
	fprintf(fp, EndOfComments);
	fprintf(fp, "%d\n", numLayers);

	for (int l = 0; l < numLayers; ++l)
		fprintf(fp, "%d ", neuronsOfLayer[l]);
	fprintf(fp, "\n");

	for (int l = 1; l < numLayers; ++l) // for each layer
		for (int n = 0; n < neuronsOfLayer[l]; ++n) // for each neuron
			{
			for (int i = 0; i <= neuronsOfLayer[l - 1]; ++i) // for each weight
				fprintf(fp, "%f ", (float) net->layers[l].neurons[n].weights[i]);
			fprintf(fp, "\n");
			}
	fclose(fp);
	printf("File saved.");
	}

NNET *loadNet(int *pNumLayers, int *pNeuronsOfLayer[], char *defaultName)
	{
	printf("Existing network files:\n");
	system("ls *.net");
	char fileName[1024];
	printf("\nEnter file name, default = [%s] :", defaultName);
	int c;
	while ( (c = getchar()) != EOF && c != '\n' )
		;
	fgets(fileName, sizeof(fileName), stdin);
	fileName[strlen(fileName) - 1] = '\0';
	if (strlen(fileName) == 0)
		strcpy(fileName, defaultName);
	FILE *fp = fopen(fileName, "r");

	// skip comments
	char s[2048];
	do
		fscanf(fp, "%s\n", s);
	while (!strcmp(s, EndOfComments));

	fscanf(fp, "%d\n", pNumLayers);
	*pNeuronsOfLayer = (int *) malloc(*pNumLayers * sizeof(int));

	for (int l = 0; l < *pNumLayers; ++l)
		fscanf(fp, "%d ", &((*pNeuronsOfLayer)[l]));
	fscanf(fp, "\n");

	NNET *net = create_NN(*pNumLayers, *pNeuronsOfLayer);

	for (int l = 1; l < *pNumLayers; ++l) // for each layer
		for (int n = 0; n < (*pNeuronsOfLayer)[l]; ++n) // for each neuron
			{
			for (int i = 0; i <= (*pNeuronsOfLayer)[l - 1]; ++i) // for each weight
				{
				float x;
				fscanf(fp, "%f ", &x);
				// printf("%f ", x);
				net->layers[l].neurons[n].weights[i] = (double) x;
				}
			fscanf(fp, "\n");
			}
	fclose(fp);
	return net;
	}

void arithmetic_testC()		// verify results for testB
	{
	NNET *Net;
	int numLayers;
	int *neuronsOfLayer;
	Net = loadNet(&numLayers, &neuronsOfLayer, "operator.net");
	LAYER lastLayer = Net->layers[numLayers - 1];

	/****
	printf("\n\nTest with: 73 - 37 = 36.\n");
	K[0] = 0.7;		// A1
	K[1] = 0.3;		// A0
	K[2] = 0.3;		// B1
	K[3] = 0.7;		// B0
	K[4] = 0.0;		// carry
	K[5] = 0.0;		// current digit
	K[6] = 0.0;		// C1
	K[7] = 0.0;		// C0
	K[8] = 0.0;		// ready
	K[9] = 0.0;		// overflow
	forward_prop(Net, 8, K);
	printf("carry [1.0] = %f\n", lastLayer.neurons[0].output);
	printf("current-digit [1.0] = %f\n", lastLayer.neurons[1].output);
	printf("C1 [0.0] = %f\n", lastLayer.neurons[2].output);
	printf("C0 [0.6] = %f\n", lastLayer.neurons[3].output);
	printf("ready [0.0] = %f\n", lastLayer.neurons[4].output);
	printf("overflow [0.0] = %f\n", lastLayer.neurons[5].output);

	// copy output back to K;  second iteration
	for (int k = 0; k < 6; ++k)
		K[k + 4] = lastLayer.neurons[k].output;
	forward_prop(Net, 8, K);
	printf("\ncarry [0.0] = %f\n", lastLayer.neurons[0].output);
	printf("current-digit [1.0] = %f\n", lastLayer.neurons[1].output);
	printf("C1 [0.3] = %f\n", lastLayer.neurons[2].output);
	printf("C0 [0.6] = %f\n", lastLayer.neurons[3].output);
	printf("ready [1.0] = %f\n", lastLayer.neurons[4].output);
	printf("overflow [0.0] = %f\n", lastLayer.neurons[5].output);
	 ****/

	int ans_correct = 0, ans_negative = 0, ans_wrong = 0, ans_non_term = 0;
	#define P 100
	for (int i = 0; i < P; ++i)
		{
		printf("(%d) ", i);
		switch (arithmetic_testC_1(Net, lastLayer))
			{
			case 1:
				++ans_correct;
				break;
			case 2:
				++ans_negative;
				break;
			case 3:
				++ans_wrong;
				break;
			case 4:
				++ans_non_term;
				break;
			default:
				printf("Answer error!\n");
				break;
			}
		}

	printf("\n=======================\n");
	printf("Answers correct  = %d (%.1f%%)\n", ans_correct, ans_correct * 100 / (float) P);
	printf("Answers negative = %d (%.1f%%)\n", ans_negative, ans_negative * 100 / (float) P);
	printf("Answers wrong    = %d (%.1f%%)\n", ans_wrong, ans_wrong * 100 / (float) P);
	printf("Answers non-term = %d (%.1f%%)\n", ans_non_term, ans_non_term * 100 / (float) P);

	free_NN(Net, neuronsOfLayer);
	free(neuronsOfLayer);
	}

int arithmetic_testC_1(NNET *Net, LAYER lastLayer)
	{
	double K1[10], K2[10];
	double a1, a0, b1, b0;
	int ans = 0;

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
	printf("%c%c, %c%c: ", digit(a1), digit(a0), digit(b1), digit(b0));

	// correct answer
	int a = floor(a1 * 10) * 10 + a0 * 10;
	int b = floor(b1 * 10) * 10 + b0 * 10;
	printf("%d - ", a);
	printf("%d = ", b);

	int c = a - b;
	printf("%d\n", c);
	double c1 = (double) (c / 10) / 10.0;
	double c0 = (double) (c % 10) / 10.0;

	//	double carryFlag = rand() / (double) RAND_MAX;
	//	K1[4] = carryFlag;
	//	double currentDigit = rand() / (double) RAND_MAX;
	//	K1[5] = currentDigit;

	K1[4] = 0.0; // carry flag
	K1[5] = 0.0; // current digit (0 or 1)
	K1[6] = 0.0; // C1
	K1[7] = 0.0; // C0
	K1[8] = 0.0; // result ready flag
	K1[9] = 0.0; // underflow flag

	int looped = 0;
LOOP:

	// call the transition operator
	ForwardPropMethod(Net, 8, K1); // input vector dimension = 8

	for (int k = 4; k < 10; ++k) // 4..10 = output vector
		K2[k] = lastLayer.neurons[k - 4].output;

	// get result
	if (K2[8] > 0.5) // result ready?
		{
		double err1 = 0.0, err2 = 0.0;
		bool correct = true;
		if (c < 0) // answer is negative
			{
			if (K2[9] < 0.5) // underflow is clear but should be set
				correct = false;
			}
		else
			{
			if (K2[9] >= 0.5) // underflow is set but should be clear
				correct = false;

			err1 = fabs(K2[6] - c1);
			err2 = fabs(K2[7] - c0);
			printf(" err1, err2 = %f, %f\n", err1, err2);
			if (err1 > 0.099999)
				correct = false;
			if (err2 > 0.099999)
				correct = false;
			}

		printf(" answer = %c%c    ", digit(c1), digit(c0));
		printf(" genifer = %c%c\n", digit(K2[6]), digit(K2[7]));
		// printf(" C1*,C0* = %f, %f   ", c1, c0);
		// printf(" C1,C0 = %f, %f\n", K2[6], K2[7]);

		if (correct && c > 0)
			{
			ans = 1;
			printf("\x1b[32m***************** Yes!!!! ****************\x1b[39;49m\n");
			}
		else if (correct)
			{
			ans = 2;
			printf("\x1b[34mNegative YES \x1b[39;49m\n");
			}
		else
			{
			ans = 3;
			printf("\x1b[31mWrong!!!! ");
			if (c < 0)
				printf(" underflow = %f \x1b[39;49m\n", K2[9]);
			else
				printf("\x1b[35m err1, err2 = %f, %f \x1b[39;49m\n", err1, err2);

			// beep();
			}
		}
	else
		{
		for (int k = 4; k < 10; ++k)
			K1[k] = K2[k];

		if (looped < 1)
			{
			++looped;
			goto LOOP;
			}
		else
			{
			ans = 4;
			printf("\x1b[31mNon-termination: ");
			printf("result ready = %f \x1b[39;49m\n", K2[8]);
			// beep();
			}
		}
	return ans;
	}

// Now we combine the 2 arithmetic steps into 1 step, as a single operator.
// This operator is supposedly very hard to learn.
#define ForwardPropMethod	forward_prop_ReLU
#define ErrorThreshold		0.001

void arithmetic_testD()		// same as testB, except learns entire operator in 1 step
	{
	// first = input layer, last = output layer
	int neuronsOfLayer[] = {8, 19, 19, 19, 19, 6};
	int dimK = 8;
	int numLayers = sizeof(neuronsOfLayer) / sizeof(int);
	NNET *Net = create_NN(numLayers, neuronsOfLayer);
	LAYER lastLayer = Net->layers[numLayers - 1];
	double errors[dimK];

	#define M	50			// how many errors to record for averaging
	double errors1[M], errors2[M]; // two arrays for recording errors

	int userKey = 0;
	double sum_err1 = 0.0, sum_err2 = 0.0; // sums of errors
	int tail = 0; // index for cyclic arrays (last-in, first-out)
	for (int i = 0; i < M; ++i) // clear errors to 0.0
		errors1[i] = errors2[i] = 0.0;

	// start_NN_plot();
	start_W_plot();
	start_LogErr_plot();
	// start_K_plot();
	// start_output_plot();
	// plot_ideal();
	start_timer();
	printf("[Q] quit\n\n");

	char status[1000], *s;
	for (int i = 1; true; ++i)
		{
		s = status + sprintf(status, "[%05d] ", i);

		// Create random K vector (4 + 2 + 2 elements)
		for (int k = 0; k < 4; ++k)
			K[k] = floor((rand() / (double) RAND_MAX) * 10.0) / 10.0;
		for (int k = 4; k < 6; ++k)
			K[k] = (rand() / (double) RAND_MAX) > 0.5 ? 1.0 : 0.0;
		for (int k = 6; k < 8; ++k)
			K[k] = floor((rand() / (double) RAND_MAX) * 10.0) / 10.0;
		// printf("*** K = <%lf, %lf>\n", K[0], K[1]);

		ForwardPropMethod(Net, dimK, K); // dim K = 8 (dimension of input-layer vector)

		// Desired value = K_star
		double K_star[10];
		transition(K, K_star);
		memcpy(K, K_star, sizeof(K_star));		// order = (target, source) !
		transition(K, K_star);					// transition again!

		// Difference between actual outcome and desired value:
		double training_err = 0.0;
		for (int k = 4; k < 10; ++k)
			{
			double error = K_star[k] - lastLayer.neurons[k - 4].output;
			errors[k - 4] = error; // record this for back-prop

			training_err += fabs(error); // record sum of errors
			}
		s += sprintf(s, "|e|=%lf, ", training_err);

		// Update error arrays cyclically
		// (This is easier to understand by referring to the next block of code)
		sum_err2 -= errors2[tail];
		sum_err2 += errors1[tail];
		sum_err1 -= errors1[tail];
		sum_err1 += training_err;
		// printf("sum1, sum2 = %lf %lf\n", sum_err1, sum_err2);

		double mean_err = (i < M) ? (sum_err1 / i) : (sum_err1 / M);
		s += sprintf(s, "mean |e|=%lf, ", mean_err);

		if (mean_err < ErrorThreshold)
			break;
		if (mean_err > 1e10 || mean_err < 0.0 || (i > 5000 && isnan(mean_err)))
			{
			printf("%s\n", status);
			printf("Error overflow.\n");
			beep();
			pause_key();
			userKey = 3;
			}

		// record new error in cyclic arrays
		errors2[tail] = errors1[tail];
		errors1[tail] = training_err;
		++tail;
		if (tail == M) // loop back in cycle
			tail = 0;

		back_prop(Net, errors); // train the network!

		// Testing set
		if ((i % 5000) == 0)
			{
			#define NumTrials 20
			double test_err = 0.0;
			for (int j = 0; j < NumTrials; ++j)
				{
				// Create random K vector (4 + 2 + 2 elements)
				for (int k = 0; k < 4; ++k)
					K[k] = floor((rand() / (double) RAND_MAX) * 10.0) / 10.0;
				for (int k = 4; k < 6; ++k)
					K[k] = (rand() / (double) RAND_MAX) > 0.5 ? 1.0 : 0.0;
				for (int k = 6; k < 8; ++k)
					K[k] = floor((rand() / (double) RAND_MAX) * 10.0) / 10.0;
				// plot_tester(K[0], K[1]);

				ForwardPropMethod(Net, dimK, K); // input vector dimension = 8

				// Desired value = K_star
				double K_star[10];
				transition(K, K_star);

				double single_err = 0.0;
				for (int k = 4; k < 10; ++k)
					{
					double error = K_star[k] - lastLayer.neurons[k - 4].output;
					single_err += fabs(error); // record sum of errors
					}
				test_err += single_err;
				}
			test_err /= (float) NumTrials;
			s += sprintf(s, "random test |e|=%1.06lf, ", test_err);

			double ratio = (sum_err2 - sum_err1) / sum_err1;
			if (ratio > 0)
				s += sprintf(s, "|e| ratio=%f", ratio);
			else
				s += sprintf(s, "|e| ratio=\x1b[31m%f\x1b[39;49m", ratio);

			// if (isnan(ratio))
			//	break;
			// if (ratio - 0.5f < 0.0000001)	// ratio == 0.5 means stationary
			// if (test_err < 0.01)
			if (test_err < ErrorThreshold)
				break;
			}

		if ((i % 50) == 0) // display status periodically
			{
			printf("%s\r", status);
			// plot_output(Net);
			// flush_output();
			plot_W(Net);
			plot_LogErr(mean_err, ErrorThreshold);
			// plot_NN(Net);
			// plot_trainer(0);		// required to clear the window
			// plot_K();
			userKey = delay_vis(0);
			}

		if (userKey == 1)
			break;
		else if (userKey == 3)			// Re-start with new random weights
			{
			re_randomize(Net, numLayers, neuronsOfLayer);
			userKey = 0;
			sum_err1 = 0.0; sum_err2 = 0.0;
			tail = 0;
			for (int j = 0; j < M; ++j) // clear errors to 0.0
				errors1[j] = errors2[j] = 0.0;
			i = 1;

			restart_LogErr_plot();
			start_timer();
			printf("\n****** Network re-randomized.\n");
			userKey = 0;
			beep();
			// pause_key();
			}
		else if (userKey == 2)
			// Test learned operator
			{
			printf("\n");

			int ans_correct = 0, ans_negative = 0, ans_wrong = 0, ans_non_term = 0;
			#define P 100
			for (int i = 0; i < P; ++i)
				{
				printf("(%d) ", i);
				switch (arithmetic_testE_1(Net, lastLayer))
					{
					case 1:
						++ans_correct;
						break;
					case 2:
						++ans_negative;
						break;
					case 3:
						++ans_wrong;
						break;
					case 4:
						++ans_non_term;
						break;
					default:
						printf("Answer error!\n");
						break;
					}
				}

			printf("\n=======================\n");
			printf("Answers correct  = %d (%.1f%%)\n", ans_correct, ans_correct * 100 / (float) P);
			printf("Answers negative = %d (%.1f%%)\n", ans_negative, ans_negative * 100 / (float) P);
			printf("Answers wrong    = %d (%.1f%%)\n", ans_wrong, ans_wrong * 100 / (float) P);
			printf("Answers non-term = %d (%.1f%%)\n", ans_non_term, ans_non_term * 100 / (float) P);
			printf("\n");
			userKey = 0;
			pause_key();
			}
		else if (userKey == 4)			// load weights file
			{
			// TO-DO...

			}
		}

	printf("Terminated....\n");
	printf("%s\n", status);
	end_timer(NULL);
	if (userKey == 0)		// terminated successfully? (not 'quit' key)
		beep();
	// plot_output(Net);
	// flush_output();
	plot_W(Net);

	if (userKey == 0)
		pause_graphics();
	else
		quit_graphics();

	extern void saveNet();
	printf("Saving network data....\n");
	saveNet(Net, numLayers, neuronsOfLayer, "", "operator.net");
	free_NN(Net, neuronsOfLayer);
	}

void arithmetic_testE()		// verify results for testD
	{
	int numLayers, *neuronsOfLayer;
	NNET *Net = loadNet(&numLayers, &neuronsOfLayer, "operator.net");
	LAYER lastLayer = Net->layers[numLayers - 1];

	int ans_correct = 0, ans_negative = 0, ans_wrong = 0, ans_non_term = 0;
	#define P 100
	for (int i = 0; i < P; ++i)
		{
		printf("(%d) ", i);
		switch (arithmetic_testE_1(Net, lastLayer))
			{
			case 1:
				++ans_correct;
				break;
			case 2:
				++ans_negative;
				break;
			case 3:
				++ans_wrong;
				break;
			case 4:
				++ans_non_term;
				break;
			default:
				printf("Answer error!\n");
				break;
			}
		}

	printf("\n=======================\n");
	printf("Answers correct  = %d (%.1f%%)\n", ans_correct, ans_correct * 100 / (float) P);
	printf("Answers negative = %d (%.1f%%)\n", ans_negative, ans_negative * 100 / (float) P);
	printf("Answers wrong    = %d (%.1f%%)\n", ans_wrong, ans_wrong * 100 / (float) P);
	printf("Answers non-term = %d (%.1f%%)\n", ans_non_term, ans_non_term * 100 / (float) P);

	free_NN(Net, neuronsOfLayer);
	free(neuronsOfLayer);
	}

int arithmetic_testE_1(NNET *Net, LAYER lastLayer)
	{
	double K1[10], K2[10];
	double a1, a0, b1, b0;
	int ans = 0;

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
	printf("%c%c, %c%c: ", digit(a1), digit(a0), digit(b1), digit(b0));

	// correct answer
	int a = floor(a1 * 10) * 10 + a0 * 10;
	int b = floor(b1 * 10) * 10 + b0 * 10;
	printf("%d - ", a);
	printf("%d = ", b);

	int c = a - b;
	printf("%d\n", c);
	double c1 = (double) (c / 10) / 10.0;
	double c0 = (double) (c % 10) / 10.0;

	//	double carryFlag = rand() / (double) RAND_MAX;
	//	K1[4] = carryFlag;
	//	double currentDigit = rand() / (double) RAND_MAX;
	//	K1[5] = currentDigit;

	K1[4] = 0.0; // carry flag
	K1[5] = 0.0; // current digit (0 or 1)
	K1[6] = 0.0; // C1
	K1[7] = 0.0; // C0
	K1[8] = 0.0; // result ready flag
	K1[9] = 0.0; // underflow flag

	// call the transition operator twice
	ForwardPropMethod(Net, 8, K1); // input vector dimension = 8
	for (int k = 4; k < 10; ++k) // 4..10 = output vector
		K1[k] = lastLayer.neurons[k - 4].output;
	ForwardPropMethod(Net, 8, K1); // input vector dimension = 8

	for (int k = 4; k < 10; ++k) // 4..10 = output vector
		K2[k] = lastLayer.neurons[k - 4].output;

	// get result
	if (K2[8] > 0.5) // result ready?
		{
		double err1 = 0.0, err2 = 0.0;
		bool correct = true;
		if (c < 0) // answer is negative
			{
			if (K2[9] < 0.5) // underflow is clear but should be set
				correct = false;
			}
		else
			{
			if (K2[9] >= 0.5) // underflow is set but should be clear
				correct = false;

			err1 = fabs(K2[6] - c1);
			err2 = fabs(K2[7] - c0);
			printf(" err1, err2 = %f, %f\n", err1, err2);
			if (err1 > 0.099999)
				correct = false;
			if (err2 > 0.099999)
				correct = false;
			}

		printf(" answer = %c%c    ", digit(c1), digit(c0));
		printf(" genifer = %c%c\n", digit(K2[6]), digit(K2[7]));
		// printf(" C1*,C0* = %f, %f   ", c1, c0);
		// printf(" C1,C0 = %f, %f\n", K2[6], K2[7]);

		if (correct && c > 0)
			{
			ans = 1;
			printf("\x1b[32m***************** Yes!!!! ****************\x1b[39;49m\n");
			}
		else if (correct)
			{
			ans = 2;
			printf("\x1b[34mNegative YES \x1b[39;49m\n");
			}
		else
			{
			ans = 3;
			printf("\x1b[31mWrong!!!! ");
			if (c < 0)
				printf(" underflow = %f \x1b[39;49m\n", K2[9]);
			else
				printf("\x1b[35m err1, err2 = %f, %f \x1b[39;49m\n", err1, err2);

			// beep();
			}
		}
	else
		{
		ans = 4;
		printf("\x1b[31mNon-termination: ");
		printf("result ready = %f \x1b[39;49m\n", K2[8]);
		// beep();
		}
	return ans;
	}

// ************************************************************
// At this point, it seems demonstrable that the 1-step operator cannot be easily learned
// by a simple feedforward network.  Now the question is how to learn in multiple steps.
// Perhaps we start by forcing it to learn in 2 steps.

// This function seems redundant (use BPTT_arithmetic_test() instead)
void arithmetic_testF()		// same as testD, except learns entire operator in 2 steps
	{
	// first = input layer, last = output layer
	int neuronsOfLayer[] = {8, 19, 19, 19, 19, 6};
	int dimK = 8;
	int numLayers = sizeof(neuronsOfLayer) / sizeof(int);
	NNET *Net = create_NN(numLayers, neuronsOfLayer);
	LAYER lastLayer = Net->layers[numLayers - 1];
	double errors[dimK];

	#define M	50			// how many errors to record for averaging
	double errors1[M], errors2[M]; // two arrays for recording errors

	int userKey = 0;
	double sum_err1 = 0.0, sum_err2 = 0.0; // sums of errors
	int tail = 0; // index for cyclic arrays (last-in, first-out)
	for (int i = 0; i < M; ++i) // clear errors to 0.0
		errors1[i] = errors2[i] = 0.0;

	// start_NN_plot();
	start_W_plot();
	start_LogErr_plot();
	// start_K_plot();
	// start_output_plot();
	// plot_ideal();
	start_timer();
	printf("[Q] quit\n\n");

	char status[1000], *s;
	for (int i = 1; true; ++i)
		{
		s = status + sprintf(status, "[%05d] ", i);

		// Create random K vector (4 + 2 + 2 elements)
		for (int k = 0; k < 4; ++k)
			K[k] = floor((rand() / (double) RAND_MAX) * 10.0) / 10.0;
		for (int k = 4; k < 6; ++k)
			K[k] = (rand() / (double) RAND_MAX) > 0.5 ? 1.0 : 0.0;
		for (int k = 6; k < 8; ++k)
			K[k] = floor((rand() / (double) RAND_MAX) * 10.0) / 10.0;
		// printf("*** K = <%lf, %lf>\n", K[0], K[1]);

		ForwardPropMethod(Net, dimK, K); // dim K = 8 (dimension of input-layer vector)

		// Desired value = K_star
		double K_star[10];
		transition(K, K_star);
		memcpy(K, K_star, sizeof(K_star));		// order = (target, source) !
		transition(K, K_star);					// transition again!

		// Difference between actual outcome and desired value:
		double training_err = 0.0;
		for (int k = 4; k < 10; ++k)
			{
			double error = K_star[k] - lastLayer.neurons[k - 4].output;
			errors[k - 4] = error; // record this for back-prop

			training_err += fabs(error); // record sum of errors
			}
		s += sprintf(s, "|e|=%lf, ", training_err);

		// Update error arrays cyclically
		// (This is easier to understand by referring to the next block of code)
		sum_err2 -= errors2[tail];
		sum_err2 += errors1[tail];
		sum_err1 -= errors1[tail];
		sum_err1 += training_err;
		// printf("sum1, sum2 = %lf %lf\n", sum_err1, sum_err2);

		double mean_err = (i < M) ? (sum_err1 / i) : (sum_err1 / M);
		s += sprintf(s, "mean |e|=%lf, ", mean_err);

		if (mean_err < ErrorThreshold)
			break;
		if (mean_err > 1e10 || mean_err < 0.0 || (i > 5000 && isnan(mean_err)))
			{
			printf("%s\n", status);
			printf("Error overflow.\n");
			beep();
			pause_key();
			userKey = 3;
			}

		// record new error in cyclic arrays
		errors2[tail] = errors1[tail];
		errors1[tail] = training_err;
		++tail;
		if (tail == M) // loop back in cycle
			tail = 0;

		back_prop(Net, errors); // train the network!

		// Testing set
		if ((i % 5000) == 0)
			{
			#define NumTrials 20
			double test_err = 0.0;
			for (int j = 0; j < NumTrials; ++j)
				{
				// Create random K vector (4 + 2 + 2 elements)
				for (int k = 0; k < 4; ++k)
					K[k] = floor((rand() / (double) RAND_MAX) * 10.0) / 10.0;
				for (int k = 4; k < 6; ++k)
					K[k] = (rand() / (double) RAND_MAX) > 0.5 ? 1.0 : 0.0;
				for (int k = 6; k < 8; ++k)
					K[k] = floor((rand() / (double) RAND_MAX) * 10.0) / 10.0;
				// plot_tester(K[0], K[1]);

				ForwardPropMethod(Net, dimK, K); // input vector dimension = 8

				// Desired value = K_star
				double K_star[10];
				transition(K, K_star);

				double single_err = 0.0;
				for (int k = 4; k < 10; ++k)
					{
					double error = K_star[k] - lastLayer.neurons[k - 4].output;
					single_err += fabs(error); // record sum of errors
					}
				test_err += single_err;
				}
			test_err /= (float) NumTrials;
			s += sprintf(s, "random test |e|=%1.06lf, ", test_err);

			double ratio = (sum_err2 - sum_err1) / sum_err1;
			if (ratio > 0)
				s += sprintf(s, "|e| ratio=%f", ratio);
			else
				s += sprintf(s, "|e| ratio=\x1b[31m%f\x1b[39;49m", ratio);

			// if (isnan(ratio))
			//	break;
			// if (ratio - 0.5f < 0.0000001)	// ratio == 0.5 means stationary
			// if (test_err < 0.01)
			if (test_err < ErrorThreshold)
				break;
			}

		if ((i % 50) == 0) // display status periodically
			{
			printf("%s\r", status);
			// plot_output(Net);
			// flush_output();
			plot_W(Net);
			plot_LogErr(mean_err, ErrorThreshold);
			// plot_NN(Net);
			// plot_trainer(0);		// required to clear the window
			// plot_K();
			userKey = delay_vis(0);
			}

		if (userKey == 1)
			break;
		else if (userKey == 3)			// Re-start with new random weights
			{
			re_randomize(Net, numLayers, neuronsOfLayer);
			userKey = 0;
			sum_err1 = 0.0; sum_err2 = 0.0;
			tail = 0;
			for (int j = 0; j < M; ++j) // clear errors to 0.0
				errors1[j] = errors2[j] = 0.0;
			i = 1;

			restart_LogErr_plot();
			start_timer();
			printf("\n****** Network re-randomized.\n");
			userKey = 0;
			beep();
			// pause_key();
			}
		else if (userKey == 2)
			// Test learned operator
			{
			printf("\n");

			int ans_correct = 0, ans_negative = 0, ans_wrong = 0, ans_non_term = 0;
			#define P 100
			for (int i = 0; i < P; ++i)
				{
				printf("(%d) ", i);
				switch (arithmetic_testE_1(Net, lastLayer))
					{
					case 1:
						++ans_correct;
						break;
					case 2:
						++ans_negative;
						break;
					case 3:
						++ans_wrong;
						break;
					case 4:
						++ans_non_term;
						break;
					default:
						printf("Answer error!\n");
						break;
					}
				}

			printf("\n=======================\n");
			printf("Answers correct  = %d (%.1f%%)\n", ans_correct, ans_correct * 100 / (float) P);
			printf("Answers negative = %d (%.1f%%)\n", ans_negative, ans_negative * 100 / (float) P);
			printf("Answers wrong    = %d (%.1f%%)\n", ans_wrong, ans_wrong * 100 / (float) P);
			printf("Answers non-term = %d (%.1f%%)\n", ans_non_term, ans_non_term * 100 / (float) P);
			printf("\n");
			userKey = 0;
			pause_key();
			}
		else if (userKey == 4)			// load weights file
			{
			// TO-DO...

			}
		}

	printf("Terminated....\n");
	printf("%s\n", status);
	end_timer(NULL);
	if (userKey == 0)		// terminated successfully? (not 'quit' key)
		beep();
	// plot_output(Net);
	// flush_output();
	plot_W(Net);

	if (userKey == 0)
		pause_graphics();
	else
		quit_graphics();

	extern void saveNet();
	printf("Saving network data....\n");
	saveNet(Net, numLayers, neuronsOfLayer, "", "operator.net");
	free_NN(Net, neuronsOfLayer);
	}

void save_RNN(RNN *net, int numLayers, int *neuronsOfLayer, char *comments)
	{
	char fileName[1024];
	printf("Enter file name [default = don't save] :");
	int c;
	while ( (c = getchar()) != EOF && c != '\n' )
		;
	fgets(fileName, sizeof(fileName), stdin);
	fileName[strlen(fileName) - 1] = '\0';
	if (strlen(fileName) == 0)
		return;

	FILE *fp = fopen(fileName, "w");
	fprintf(fp, "%s\n", comments);
	fprintf(fp, EndOfComments);				// defined in saveNet()
	fprintf(fp, "%d\n", numLayers);

	for (int l = 0; l < numLayers; ++l)
		fprintf(fp, "%d ", neuronsOfLayer[l]);
	fprintf(fp, "\n");

	for (int l = 1; l < numLayers; ++l) // for each layer
		for (int n = 0; n < neuronsOfLayer[l]; ++n) // for each neuron
			{
			for (int i = 0; i <= neuronsOfLayer[l - 1]; ++i) // for each weight
				fprintf(fp, "%f ", (float) net->layers[l].neurons[n].weights[i]);
			fprintf(fp, "\n");
			}
	fclose(fp);
	}

RNN *load_RNN(int *pNumLayers, int *pNeuronsOfLayer[])
	{
	printf("Existing network files:\n");
	system("ls *.rnn");
	char fileName[1024];
	printf("\nEnter file name, default = [operator.net] :");
	int c;
	while ( (c = getchar()) != EOF && c != '\n' )
		;
	fgets(fileName, sizeof(fileName), stdin);
	fileName[strlen(fileName) - 1] = '\0';
	if (strlen(fileName) == 0)
		strcpy(fileName, "operator.net");
	FILE *fp = fopen(fileName, "r");

	// skip comments
	char s[2048];
	do
		fscanf(fp, "%s\n", s);
	while (!strcmp(s, EndOfComments));

	fscanf(fp, "%d\n", pNumLayers);
	printf("# of layers = %d\n", *pNumLayers);
	*pNeuronsOfLayer = (int *) malloc(*pNumLayers * sizeof(int));

	for (int l = 0; l < *pNumLayers; ++l)
		fscanf(fp, "%d ", &((*pNeuronsOfLayer)[l]));
	fscanf(fp, "\n");

	RNN *net = create_BPTT_NN(*pNumLayers, *pNeuronsOfLayer);

	for (int l = 1; l < *pNumLayers; ++l) // for each layer
		for (int n = 0; n < (*pNeuronsOfLayer)[l]; ++n) // for each neuron
			{
			for (int i = 0; i <= (*pNeuronsOfLayer)[l - 1]; ++i) // for each weight
				{
				float x;
				fscanf(fp, "%f ", &x);
				// printf("%f ", x);
				net->layers[l].neurons[n].weights[i] = (double) x;
				}
			fscanf(fp, "\n");
			}
	fclose(fp);
	return net;
	}

#define ErrorThreshold		0.001
void BPTT_arithmetic_test()
	{
	int neuronsOfLayer[4] = {8, 13, 10, 8}; // first = input layer, last = output layer
	// (first- and last-layer dimensions must match because network needs to be recurrent)
	int dimK = 8;		// dim K = 8 (dimension of input-layer vector)
	int numLayers = sizeof(neuronsOfLayer) / sizeof(int);
	// create BPTT_NN
	RNN *Net = create_BPTT_NN(numLayers, neuronsOfLayer);
	rLAYER lastLayer = Net->layers[numLayers - 1];
	double errors[dimK];

	int userKey = 0;
	#define M	50			// how many errors to record for averaging
	double errors1[M], errors2[M]; // two arrays for recording errors
	double sum_err1 = 0.0, sum_err2 = 0.0; // sums of errors
	int tail = 0; // index for cyclic arrays (last-in, first-out)

	for (int i = 0; i < M; ++i) // clear errors to 0.0
		errors1[i] = errors2[i] = 0.0;

	// start_NN_plot();
	start_W_plot();
	start_LogErr_plot();
	// start_K_plot();
	// start_output_plot();
	// plot_ideal();
	start_timer();
	printf("[P] to pause, [R] to resume, [Q] to quit\n\n");

	int t = 0;		// t is the final time step

	char status[512], *s;
	for (int i = 0; true; ++i)
		{
		s = status + sprintf(status, "[%05d] ", i);

		// Create random K vector (4 + 2 + 2 elements)
		for (int k = 0; k < 4; ++k)
			K[k] = floor((rand() / (double) RAND_MAX) * 10.0) / 10.0;
		K[4] = 0.0; // carry flag
		K[5] = 0.0; // current digit (0 or 1)
		K[6] = 0.0; // C1
		K[7] = 0.0; // C0
		K[8] = 0.0; // result ready flag
		K[9] = 0.0; // underflow flag
		// printf("*** K = <%lf, %lf>\n", K[0], K[1]);

		forward_BPTT(Net, 8, K, 1);			// unfold 1 time
		// Note that only 6 of 8 dimensions of the output is significant
		// ...the last 2 dimensions are ignored
		// printf("successfully forward propagated\n");

		// Desired value = K_star
		double K_star[10];
		transition(K, K_star);
		memcpy(K, K_star, sizeof(K_star));		// order = (target, source) !
		transition(K, K_star);					// transition again!

		// Difference between actual outcome and desired value:
		double training_err = 0.0;
		for (int k = 4; k < 10; ++k)		// 6 components
			{
			double error = K_star[k] - lastLayer.neurons[k - 4].output[t];
			errors[k - 4] = error;			// record this for back-prop

			training_err += fabs(error);	// record sum of errors
			}
		for (int k = 6; k < 8; ++k)			// last 2 errors are always 0
			errors[k] = 0.0;

		// printf("sum of squared error = %lf  ", training_err);

		// Update error arrays cyclically
		// (This is easier to understand by referring to the next block of code)
		sum_err2 -= errors2[tail];
		sum_err2 += errors1[tail];
		sum_err1 -= errors1[tail];
		sum_err1 += training_err;
		// printf("sum1, sum2 = %lf %lf\n", sum_err1, sum_err2);

		double mean_err = (i < M) ? (sum_err1 / i) : (sum_err1 / M);
		s += sprintf(s, "mean |e|=%lf, ", mean_err);

		if (training_err < ErrorThreshold)
			break;

		// record new error in cyclic arrays
		errors2[tail] = errors1[tail];
		errors1[tail] = training_err;
		++tail;
		if (tail == M) // loop back in cycle
			tail = 0;

		backprop_through_time(Net, errors, 1); // train the network, n-fold = 1

		// Testing set
		if ((i % 5000) == 0)
			{
			double test_err = 0.0;
			for (int j = 0; j < 10; ++j)
				{
				// Create random K vector (4 + 2 + 2 elements)
				for (int k = 0; k < 4; ++k)
					K[k] = floor((rand() / (double) RAND_MAX) * 10.0) / 10.0;
				K[4] = 0.0; // carry flag
				K[5] = 0.0; // current digit (0 or 1)
				K[6] = 0.0; // C1
				K[7] = 0.0; // C0
				K[8] = 0.0; // result ready flag
				K[9] = 0.0; // underflow flag
				// plot_tester(K[0], K[1]);

				forward_BPTT(Net, 8, K, 1);		// 1-fold backprop

				// Desired value = K_star
				double K_star[10];
				transition(K, K_star);
				memcpy(K, K_star, sizeof(K_star));	// order = (target, source) !
				transition(K, K_star);				// transition again!

				double single_err = 0.0;
				for (int k = 4; k < 10; ++k)
					{
					double error = K_star[k] - lastLayer.neurons[k - 4].output[t];
					single_err += fabs(error); // record sum of errors
					}
				test_err += single_err;
				}
			test_err /= 10.0;
			s += sprintf(s, "random test e=%1.06lf, ", test_err);

			double ratio = (sum_err2 - sum_err1) / sum_err1;
			if (ratio > 0)
				s += sprintf(s, "e ratio=%f", ratio);
			else
				s += sprintf(s, "e ratio=\x1b[31m%f\x1b[39;49m", ratio);

			if (isnan(ratio))
				break;
			// if (ratio - 0.5f < 0.0000001)	// ratio == 0.5 means stationary
			// if (test_err < 0.01)
			if (test_err < ErrorThreshold)
				break;
			}

		if (i % 500 == 0) printf("%s\r", status);

		if ((i % 500) == 0) // display status periodically
			{
			// plot_output(Net);
			// flush_output();
			plot_W_BPTT(Net);
			plot_LogErr(training_err, ErrorThreshold);
			// plot_NN(Net);
			// plot_trainer(0);		// required to clear the window
			// plot_K();
			userKey = delay_vis(0);
			}

		if (userKey == 1)
			break;
		else if (userKey == 3)			// Re-start with new random weights
			{
			BPTT_re_randomize(Net, numLayers, neuronsOfLayer);
			userKey = 0;
			sum_err1 = 0.0; sum_err2 = 0.0;
			tail = 0;
			for (int j = 0; j < M; ++j) // clear errors to 0.0
				errors1[j] = errors2[j] = 0.0;
			i = 1;

			restart_LogErr_plot();
			start_timer();
			printf("\n****** Network re-randomized.\n");
			userKey = 0;
			beep();
			// pause_key();
			}
		else if (userKey == 2)
			// Test learned operator
			{
			printf("\n");

			int ans_correct = 0, ans_negative = 0, ans_wrong = 0, ans_non_term = 0;
			#define P 100
			for (int i = 0; i < P; ++i)
				{
				printf("(%d) ", i);
				switch (BPTT_arithmetic_testB_1(Net, lastLayer))
					{
					case 1:
						++ans_correct;
						break;
					case 2:
						++ans_negative;
						break;
					case 3:
						++ans_wrong;
						break;
					case 4:
						++ans_non_term;
						break;
					default:
						printf("Answer error!\n");
						break;
					}
				}

			printf("\n=======================\n");
			printf("Answers correct  = %d (%.1f%%)\n", ans_correct, ans_correct * 100 / (float) P);
			printf("Answers negative = %d (%.1f%%)\n", ans_negative, ans_negative * 100 / (float) P);
			printf("Answers wrong    = %d (%.1f%%)\n", ans_wrong, ans_wrong * 100 / (float) P);
			printf("Answers non-term = %d (%.1f%%)\n", ans_non_term, ans_non_term * 100 / (float) P);
			printf("\n");
			userKey = 0;
			pause_key();
			}
		}

	printf("\n%s\n", status);
	end_timer();
	beep();
	// plot_output(Net);
	// flush_output();
	plot_W_BPTT(Net);

	/****
	printf("\n\nTest with: 73 - 37 = 36.\n");
	K[0] = 0.7;		// A1
	K[1] = 0.3;		// A0
	K[2] = 0.3;		// B1
	K[3] = 0.7;		// B0
	K[4] = 0.0;		// carry
	K[5] = 0.0;		// current digit
	K[6] = 0.0;		// C1
	K[7] = 0.0;		// C0
	K[8] = 0.0;		// ready
	K[9] = 0.0;		// overflow
	forward_prop(Net, 8, K);
	printf("carry [1.0] = %f\n", lastLayer.neurons[0].output);
	printf("current-digit [1.0] = %f\n", lastLayer.neurons[1].output);
	printf("C1 [0.0] = %f\n", lastLayer.neurons[2].output);
	printf("C0 [0.6] = %f\n", lastLayer.neurons[3].output);
	printf("ready [0.0] = %f\n", lastLayer.neurons[4].output);
	printf("overflow [0.0] = %f\n", lastLayer.neurons[5].output);

	// copy output back to K;  second iteration
	for (int k = 0; k < 6; ++k)
		K[k + 4] = lastLayer.neurons[k].output;
	forward_prop(Net, 8, K);
	printf("\ncarry [0.0] = %f\n", lastLayer.neurons[0].output);
	printf("current-digit [1.0] = %f\n", lastLayer.neurons[1].output);
	printf("C1 [0.3] = %f\n", lastLayer.neurons[2].output);
	printf("C0 [0.6] = %f\n", lastLayer.neurons[3].output);
	printf("ready [1.0] = %f\n", lastLayer.neurons[4].output);
	printf("overflow [0.0] = %f\n", lastLayer.neurons[5].output);
	 ****/

	if (userKey == 0)
		pause_graphics();
	else
		quit_graphics();

	extern void save_RNN(RNN *, int, int *, char *);
	save_RNN(Net, numLayers, neuronsOfLayer, "");
	free_BPTT_NN(Net, neuronsOfLayer);
	}

void BPTT_arithmetic_testB()		// verify results for BPTT_test
	{
	int numLayers, *neuronsOfLayer;
	RNN *Net = load_RNN(&numLayers, &neuronsOfLayer);
	rLAYER lastLayer = Net->layers[numLayers - 1];

	int ans_correct = 0, ans_negative = 0, ans_wrong = 0, ans_non_term = 0;
	#define P 100
	for (int i = 0; i < P; ++i)
		{
		printf("(%d) ", i);
		switch (BPTT_arithmetic_testB_1(Net, lastLayer))
			{
			case 1:
				++ans_correct;
				break;
			case 2:
				++ans_negative;
				break;
			case 3:
				++ans_wrong;
				break;
			case 4:
				++ans_non_term;
				break;
			default:
				printf("Answer error!\n");
				break;
			}
		}

	printf("\n=======================\n");
	printf("Answers correct  = %d (%.1f%%)\n", ans_correct, ans_correct * 100 / (float) P);
	printf("Answers negative = %d (%.1f%%)\n", ans_negative, ans_negative * 100 / (float) P);
	printf("Answers wrong    = %d (%.1f%%)\n", ans_wrong, ans_wrong * 100 / (float) P);
	printf("Answers non-term = %d (%.1f%%)\n", ans_non_term, ans_non_term * 100 / (float) P);

	free_BPTT_NN(Net, neuronsOfLayer);
	free(neuronsOfLayer);
	}

int BPTT_arithmetic_testB_1(RNN *Net, rLAYER lastLayer)
	{
	double K1[10], K2[10];
	double a1, a0, b1, b0;
	int ans = 0;

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
	printf("%c%c, %c%c: ", digit(a1), digit(a0), digit(b1), digit(b0));

	// correct answer
	int a = floor(a1 * 10) * 10 + a0 * 10;
	int b = floor(b1 * 10) * 10 + b0 * 10;
	printf("%d - ", a);
	printf("%d = ", b);

	int c = a - b;
	printf("%d\n", c);
	double c1 = (double) (c / 10) / 10.0;
	double c0 = (double) (c % 10) / 10.0;

	//	double carryFlag = rand() / (double) RAND_MAX;
	//	K1[4] = carryFlag;
	//	double currentDigit = rand() / (double) RAND_MAX;
	//	K1[5] = currentDigit;

	K1[4] = 0.0; // carry flag
	K1[5] = 0.0; // current digit (0 or 1)
	K1[6] = 0.0; // C1
	K1[7] = 0.0; // C0
	K1[8] = 0.0; // result ready flag
	K1[9] = 0.0; // underflow flag

	// call the transition operator twice
	forward_BPTT(Net, 8, K1, 1);			// unfold 1 time, input dimension = 8

	/* for (int k = 4; k < 10; ++k)			// 4..10 = output vector
		K1[k] = lastLayer.neurons[k - 4].output;
	ForwardPropMethod(Net, 8, K1); // input vector dimension = 8
	*/

	for (int k = 4; k < 10; ++k)			// 4..10 = output vector
		K2[k] = lastLayer.neurons[k - 4].output[0];

	// get result
	if (K2[8] > 0.5) // result ready?
		{
		double err1 = 0.0, err2 = 0.0;
		bool correct = true;
		if (c < 0) // answer is negative
			{
			if (K2[9] < 0.5) // underflow is clear but should be set
				correct = false;
			}
		else
			{
			if (K2[9] >= 0.5) // underflow is set but should be clear
				correct = false;

			err1 = fabs(K2[6] - c1);
			err2 = fabs(K2[7] - c0);
			printf(" err1, err2 = %f, %f\n", err1, err2);
			if (err1 > 0.099999)
				correct = false;
			if (err2 > 0.099999)
				correct = false;
			}

		printf(" answer = %c%c    ", digit(c1), digit(c0));
		printf(" genifer = %c%c\n", digit(K2[6]), digit(K2[7]));
		// printf(" C1*,C0* = %f, %f   ", c1, c0);
		// printf(" C1,C0 = %f, %f\n", K2[6], K2[7]);

		if (correct && c > 0)
			{
			ans = 1;
			printf("\x1b[32m***************** Yes!!!! ****************\x1b[39;49m\n");
			}
		else if (correct)
			{
			ans = 2;
			printf("\x1b[34mNegative YES \x1b[39;49m\n");
			}
		else
			{
			ans = 3;
			printf("\x1b[31mWrong!!!! ");
			if (c < 0)
				printf(" underflow = %f \x1b[39;49m\n", K2[9]);
			else
				printf("\x1b[35m err1, err2 = %f, %f \x1b[39;49m\n", err1, err2);

			// beep();
			}
		}
	else
		{
		ans = 4;
		printf("\x1b[31mNon-termination: ");
		printf("result ready = %f \x1b[39;49m\n", K2[8]);
		// beep();
		}
	return ans;
	}

