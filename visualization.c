#include <stdbool.h>
#include <SDL2/SDL.h>
#include <stdlib.h>				// For playing system "beep"
// #include <SDL2/SDL_mixer.h>	// no longer needed
#include <sys/timeb.h>		// For timing operations

#include "feedforward-NN.h"
#include "BPTT-RNN.h"

SDL_Renderer *gfx_LogErr = NULL; // For log-scale error visualizer
SDL_Window *win_LogErr = NULL;

SDL_Renderer *gfx_Ideal = NULL; // For ideal output visualizer
SDL_Window *win_Ideal = NULL;

SDL_Renderer *gfx_Out = NULL; // For YKY's output visualizer
SDL_Window *win_Out = NULL;

SDL_Renderer *gfx_W = NULL; // For YKY's weights visualizer
SDL_Window *win_W = NULL;

SDL_Renderer *gfx_NN = NULL; // For YKY's NN visualizer
SDL_Window *win_NN = NULL;

SDL_Renderer *gfx_NN2 = NULL; // For Seh's NN visualizer
SDL_Window *win_NN2 = NULL;

SDL_Renderer *gfx_K = NULL; // For K-vector visualizer
SDL_Window *win_K = NULL;

#define f2i(v) ((int)(255.0f * v))		// for converting color values

// ************************* YKY's log-scale error visualizer ***************************

#define LogErr_box_width 1000
#define LogErr_box_height 400

bool new_LogErr_plot = false;

void start_LogErr_plot(void)
	{
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		{
		printf("SDL_Init Error: %s \n", SDL_GetError());
		return;
		}

	win_LogErr = SDL_CreateWindow("Errors (automatic log-scaled)", 10, 500, LogErr_box_width, LogErr_box_height, SDL_WINDOW_SHOWN);
	if (win_LogErr == NULL)
		{
		printf("SDL_CreateWindow Error: %s \n", SDL_GetError());
		SDL_Quit();
		return;
		}

	gfx_LogErr = SDL_CreateRenderer(win_LogErr, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (gfx_LogErr == NULL)
		{
		SDL_DestroyWindow(win_LogErr);
		printf("SDL_CreateRenderer Error: %s \n", SDL_GetError());
		SDL_Quit();
		return;
		}
	
	new_LogErr_plot = true;
	}

void plot_LogErr(double err, double target)
	{
	static int errGain = 100.0;
	#define binSize 1000
	static double bin[binSize]; // stores plot data in log-scale
	static int index = 1; // true index of current datum

	if (new_LogErr_plot)
		{
		index = 1;
		errGain = 500.0;
		new_LogErr_plot = false;
		// clear window
		SDL_SetRenderDrawColor(gfx_LogErr, 0, 0, 0, 0xFF);
		SDL_RenderClear(gfx_LogErr); //Clear screen
		}
	
	bin[index++] = err; // record the error

	// Overflow?
	if (index == binSize)
		{
		// Set gain
		errGain = LogErr_box_height / bin[index - 1] / 2;

		// squeeze existing data into first half
		for (int i = 0; i < binSize / 2; ++i)
			bin[i] = (bin[i * 2] + bin[i * 2 + 1]) / 2;

		// new index appears in the mid-point
		index = binSize / 2;

		// clear data from beyond middle
		for (int i = binSize / 2 + 1; i < binSize; ++i)
			bin[i] = 0.0;

		SDL_SetRenderDrawColor(gfx_LogErr, 0, 0, 0, 0xFF);
		SDL_RenderClear(gfx_LogErr); //Clear screen

		// Plot the mid axis
		SDL_SetRenderDrawColor(gfx_LogErr, 0, 0, 0xFF, 0xFF); // blue
		int baseline_y = LogErr_box_height / 2;
		SDL_RenderDrawLine(gfx_LogErr, 0,
						baseline_y,
						LogErr_box_width,
						baseline_y);

		// Plot the error target
		SDL_SetRenderDrawColor(gfx_LogErr, 0xFF, 0, 0, 0xFF); // red
		baseline_y = LogErr_box_height - target * errGain;
		SDL_RenderDrawLine(gfx_LogErr, 0,
						baseline_y,
						LogErr_box_width,
						baseline_y);

		// Re-plot the graph
		SDL_SetRenderDrawColor(gfx_LogErr, 0x40, 0x90, 0, 0x70); // red + green
		for (int i = 1; i < binSize / 2; ++i)
			{
			SDL_RenderDrawLine(gfx_LogErr, i - 1,
							LogErr_box_height - errGain * bin[i - 1],
							i,
							LogErr_box_height - errGain * bin[i]);
			}
		}

	// Plot the new datum
	SDL_SetRenderDrawColor(gfx_LogErr, 0x40, 0x90, 0, 0x70); // red + green
	SDL_RenderDrawLine(gfx_LogErr, index - 1,
					LogErr_box_height - errGain * bin[index - 2],
					index,
					LogErr_box_height - errGain * err);

	// Display time and current error on window title bar
	char s[100];
	extern void end_timer(char *);
	end_timer(s + sprintf(s, "ē = %.04f @ ", err));
	SDL_SetWindowTitle(win_LogErr, s);

	SDL_RenderPresent(gfx_LogErr);
	}

// **************************** Trainer function visualizer *****************************

#define Ideal_box_width 500
#define Ideal_box_height 500

void plot_ideal(void)
	{
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		{
		printf("SDL_Init Error: %s \n", SDL_GetError());
		return;
		}

	win_Ideal = SDL_CreateWindow("Ideal output", 500, 500, Ideal_box_width, Ideal_box_height, SDL_WINDOW_SHOWN);
	if (win_Ideal == NULL)
		{
		printf("SDL_CreateWindow Error: %s \n", SDL_GetError());
		SDL_Quit();
		return;
		}

	gfx_Ideal = SDL_CreateRenderer(win_Ideal, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (gfx_Ideal == NULL)
		{
		SDL_DestroyWindow(win_Ideal);
		printf("SDL_CreateRenderer Error: %s \n", SDL_GetError());
		SDL_Quit();
		return;
		}

	SDL_SetRenderDrawColor(gfx_Out, 0, 0, 0, 0xFF);
	SDL_RenderClear(gfx_Ideal); //Clear screen

	#define GridPoints 30
	#define Square_width	((Ideal_box_width - 20) / GridPoints)

	// For each grid point:
	for (int i = 0; i < GridPoints; ++i)
		for (int j = 0; j < GridPoints; ++j)
			{
			double input[2];
			input[0] = ((double) i) / (GridPoints - 1);
			input[1] = ((double) j) / (GridPoints - 1);

			extern void forward_prop(NNET*, int, double []);
			// double ideal = 1.0f - (0.5f - input[0]) * (0.5f - input[1]);
			// double ideal = input[0];				/* identity function */
			#define f2b(x) (x > 0.5f ? 1 : 0)	// convert float to binary
			double ideal = ((double) (f2b(input[0]) ^ f2b(input[1]))); // ^ f2b(K[2]) ^ f2b(K[3])))

			if (ideal > 1.0f || ideal < 0.0f)
				printf("error:  %fl, %fl = %fl\n", input[0], input[1], ideal);

			int c3 = 0x00;
			float c1 = (ideal < 0.0) ? -ideal : 0.0;
			if (c1 > 1.0)
				{
				c3 = 0xFF;
				c1 = 1.0;
				}
			if (c1 < 0.0)
				{
				c3 = 0xFF;
				c1 = 0.0;
				}
			float c2 = (ideal > 0.0) ? ideal : 0.0;
			if (c2 > 1.0)
				{
				c3 = 0xFF;
				c2 = 1.0;
				}
			if (c2 < 0.0)
				{
				c3 = 0xFF;
				c2 = 0.0;
				}
			SDL_SetRenderDrawColor(gfx_Ideal, f2i(c2), f2i(c1), c3, 0xFF);

			// Plot little square
			SDL_Rect fillRect = {11 + Square_width * i, 11 + Square_width * j,
								Square_width - 1, Square_width - 1};
			SDL_RenderFillRect(gfx_Ideal, &fillRect);
			}

	SDL_RenderPresent(gfx_Ideal);
	}


// **************************** YKY's 2D output visualizer ******************************

#define Out_box_width 300
#define Out_box_height 300

void start_output_plot(void)
	{

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		{
		printf("SDL_Init Error: %s \n", SDL_GetError());
		return;
		}

	win_Out = SDL_CreateWindow("Output", 10, 10, Out_box_width, Out_box_height, SDL_WINDOW_SHOWN);
	if (win_Out == NULL)
		{
		printf("SDL_CreateWindow Error: %s \n", SDL_GetError());
		SDL_Quit();
		return;
		}

	gfx_Out = SDL_CreateRenderer(win_Out, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (gfx_Out == NULL)
		{
		SDL_DestroyWindow(win_Out);
		printf("SDL_CreateRenderer Error: %s \n", SDL_GetError());
		SDL_Quit();
		return;
		}
	}

void plot_output(NNET *net, void prop(NNET*, int, double []))
	{
	SDL_SetRenderDrawColor(gfx_Out, 0, 0, 0, 0xFF);
	SDL_RenderClear(gfx_Out); //Clear screen

	#define GridPoints 30
	#define Square_width	((Out_box_width - 20) / GridPoints)

	// For each grid point:
	for (int i = 0; i < GridPoints; ++i)
		for (int j = 0; j < GridPoints; ++j)
			{
			extern double K[];
			K[0] = ((double) i) / (GridPoints - 1);
			K[1] = ((double) j) / (GridPoints - 1);

			prop(net, 2, K); // get output
			double output = net->layers[net->numLayers - 1].neurons[0].output;

			/* Set color
			int b = 0x00;
			float r = output <= 0.5f ? -2 * output + 1.0f : 0.0f;
			if (r > 1.0f || r < 0.0f) b = 0xFF;
			float g = output >= 0.5f ? (output - 0.5f) * 2 : 0.0f;
			if (g > 1.0f || g < 0.0f) b = 0xFF; */

			float c3 = 0.0;
			#define C3gain 0.5
			float c1 = (output < 0.0) ? -output : 0.0;
			if (c1 > 1.0)
				{
				c1 = 1.0;
				c3 = (c1 - 1.0) + C3gain;
				}
			if (c1 < 0.0)
				{
				c1 = 0.0;
				c3 = -c1 + C3gain;
				}
			float c2 = (output > 0.0) ? output : 0.0;
			if (c2 > 1.0)
				{
				c2 = 1.0;
				c3 = (c2 - 1.0) + C3gain;
				}
			if (c2 < 0.0)
				{
				c2 = 0.0;
				c3 = -c2 + C3gain;
				}
			SDL_SetRenderDrawColor(gfx_Out, f2i(c2), f2i(c1), f2i(c3), 0xFF);

			// Plot little square
			SDL_Rect fillRect = {11 + Square_width * i, 11 + Square_width * j,
								Square_width - 1, Square_width - 1};
			SDL_RenderFillRect(gfx_Out, &fillRect);
			}
	}

void plot_tester(double x, double y)
	{
	int i = (int) (x * (GridPoints - 1));
	int j = (int) (y * (GridPoints - 1));

	SDL_SetRenderDrawColor(gfx_Out, 0x80, 0x50, 0xB0, 0xFF);

	// Plot little square
	SDL_Rect fillRect = {11 + Square_width * i, 11 + Square_width * j,
						Square_width - 1, Square_width - 1};
	SDL_RenderFillRect(gfx_Out, &fillRect);
	}

void flush_output() // This is to allow plotting some dots on the graph before displaying
	{
	SDL_RenderPresent(gfx_Out);
	}

// *************************** YKY's weights visualizer ********************************

#define W_box_width 900
#define W_box_height 1000

void start_W_plot(void)
	{

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		{
		printf("SDL_Init Error: %s \n", SDL_GetError());
		return;
		}

	win_W = SDL_CreateWindow("Weights (auto gain-adjusted)", 80, 20, W_box_width, W_box_height, SDL_WINDOW_SHOWN);
	if (win_W == NULL)
		{
		printf("SDL_CreateWindow Error: %s \n", SDL_GetError());
		SDL_Quit();
		return;
		}

	gfx_W = SDL_CreateRenderer(win_W, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (gfx_W == NULL)
		{
		SDL_DestroyWindow(win_W);
		printf("SDL_CreateRenderer Error: %s \n", SDL_GetError());
		SDL_Quit();
		return;
		}
	}

void plot_W(NNET *net)
	{
	SDL_SetRenderDrawColor(gfx_W, 0, 0, 0, 0xFF);
	SDL_RenderClear(gfx_W); //Clear screen

	// SDL_SetRenderDrawBlendMode(gfx_W, SDL_BLENDMODE_BLEND);

	int numLayers = net->numLayers;
	int Y_step = W_box_height / numLayers;

	for (int l = 1; l < numLayers; l++) // Note: layer 0 has no weights
		{
		int nn = net->layers[l].numNeurons;
		int neuronWidth = (W_box_width - 20) / nn;

		// ***** Automatic gain-adjust
		// find min and max weights
		double gain = 1.0f;
		double min_W, max_W;
		min_W = max_W = net->layers[l].neurons[0].weights[0];
		for (int n = 0; n < nn; n++) // for all neurons
			{
			int numWeights = net->layers[l - 1].numNeurons + 1; // always >= 2

			for (int m = 0; m < numWeights; ++m)
				{
				double weight = net->layers[l].neurons[n].weights[m];
				if (weight > max_W) max_W = weight;
				if (weight < min_W) min_W = weight;
				}
			}
		double peak = fmax(fabs(max_W), fabs(min_W));
		gain = ((double) Y_step) / peak;
		// printf("Y step = %d\n", Y_step);
		// printf("min W = %f\n", min_W);
		// printf("max W = %f\n", max_W);
		// printf("gain = %f\n", gain);
	
		// draw baseline
		SDL_SetRenderDrawColor(gfx_W, 0x00, 0x00, 0xFF, 0xFF); // blue
		int baseline_y = l * Y_step;
		SDL_RenderDrawLine(gfx_W, 10, baseline_y, \
			W_box_width - 10, baseline_y);

		// **** set color
		// The weights for each layer is scaled to fit in the horizontal strip (Y_step)
		// But if the weights are large, their line color will move towards red-purple
		// Otherwise they are green.
		// float c1 = ((float) l) / (numLayers - 1);
		// float c2 = 1.0f - ((float) l) / (numLayers - 1);
		float c1 = peak / 2.0f;		// peak starts from ~1.0 and may increase indefinitely
		float c3 = 0.0f;
		if (c1 > 1.0f)
			{
			c3 = c1 / 10.0f;
			c1 = 1.0f;
			}
		float c2 = 1.0f - c1;
		SDL_SetRenderDrawColor(gfx_W, f2i(c1), f2i(c2), f2i(c3), 0xFF);

		for (int n = 0; n < nn; n++) // for each neuron on layer l
			{
			int numWeights = net->layers[l - 1].numNeurons + 1; // always >= 2
			int basepoint_x = 10 + neuronWidth * n;
			int gap = (neuronWidth - 10) / (numWeights - 1);

			for (int m = 0; m < numWeights - 1; ++m)
				// for each weight including bias, but minus one because # line segments
				// is 1 less than # of weights
				{
				int weight0 = gain * net->layers[l].neurons[n].weights[m];
				int weight1 = gain * net->layers[l].neurons[n].weights[m + 1];
				SDL_RenderDrawLine(gfx_W, basepoint_x + 5 + gap * m,
								baseline_y - weight0,
								basepoint_x + 5 + gap * (m + 1),
								baseline_y - weight1);
				}
			}
		}

	SDL_RenderPresent(gfx_W);
	}

void plot_W_BPTT(RNN *net)
	{
	SDL_SetRenderDrawColor(gfx_W, 0, 0, 0, 0xFF);
	SDL_RenderClear(gfx_W); //Clear screen

	// SDL_SetRenderDrawBlendMode(gfx_W, SDL_BLENDMODE_BLEND);

	int numLayers = net->numLayers;
	int Y_step = W_box_height / numLayers;
	double Gain = Y_step / 1.0;
	for (int l = 1; l < numLayers; l++) // Note: layer 0 has no weights
		{
		double gain = 1.0f;
		// increase amplitude for hidden layers
		if (l > 0 && l < numLayers - 1)
			gain = 5.0f;
		else
			gain = 1.0f;

		// set color
		float r = ((float) l) / (numLayers - 1);
		float b = 1.0f - ((float) l) / (numLayers - 1);
		SDL_SetRenderDrawColor(gfx_W, 0x00, 0x00, 0xFF, 0xFF); // blue

		int nn = net->layers[l].numNeurons;

		int neuronWidth = (W_box_width - 20) / nn;

		// draw baseline

		int baseline_y = l * Y_step;
		SDL_RenderDrawLine(gfx_W, 10, baseline_y, \
			W_box_width - 10, baseline_y);

		SDL_SetRenderDrawColor(gfx_W, 0xFF, 0x00, 0x00, 0xFF); // red

		for (int n = 0; n < nn; n++) // for each neuron on layer l
			{
			int numWeights = net->layers[l - 1].numNeurons + 1; // always >= 2
			int basepoint_x = 10 + neuronWidth * n;
			int gap = (neuronWidth - 10) / (numWeights - 1);

			for (int m = 0; m < numWeights - 1; ++m)
				// for each weight including bias, but minus one because # line segments
				// is 1 less than # of weights
				{
				double weight0 = Gain * net->layers[l].neurons[n].weights[m];
				double weight1 = Gain * net->layers[l].neurons[n].weights[m + 1];
				SDL_RenderDrawLine(gfx_W, basepoint_x + 5 + gap * m,
								baseline_y - weight0,
								basepoint_x + 5 + gap * (m + 1),
								baseline_y - weight1);
				}
			}
		}

	SDL_RenderPresent(gfx_W);
	}

// *************************** YKY's NN visualizer *************************************

#define NN_box_width 600
#define NN_box_height 400

void start_NN_plot(void)
	{

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		{
		printf("SDL_Init Error: %s \n", SDL_GetError());
		return;
		}

	win_NN = SDL_CreateWindow("NN activity", 400, 600, NN_box_width, NN_box_height, SDL_WINDOW_SHOWN);
	if (win_NN == NULL)
		{
		printf("SDL_CreateWindow Error: %s \n", SDL_GetError());
		SDL_Quit();
		return;
		}

	gfx_NN = SDL_CreateRenderer(win_NN, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (gfx_NN == NULL)
		{
		SDL_DestroyWindow(win_NN);
		printf("SDL_CreateRenderer Error: %s \n", SDL_GetError());
		SDL_Quit();
		return;
		}
	}

void plot_NN(NNET *net)
	{
	SDL_SetRenderDrawColor(gfx_NN, 0, 0, 0, 0xFF);
	SDL_RenderClear(gfx_NN); //Clear screen

	SDL_SetRenderDrawBlendMode(gfx_NN, SDL_BLENDMODE_BLEND);

	#define Volume 20.0f

	#define numLayers (net->numLayers)
	for (int l = 0; l < numLayers; l++)
		{
		double gain = 1.0f;
		// increase amplitude for hidden layers
		if (l > 0 && l < numLayers - 1)
			gain = 4.0f;
		else
			gain = 2.0f;

		// draw baselines (blue = base level, faint blue = 1.0 level)
		#define Y_step2 ((NN_box_height - (int) Volume * 4) / (numLayers - 1))
		int baseline_y = (int) Volume * 2 + l * Y_step2;
		SDL_SetRenderDrawColor(gfx_NN, 0x00, 0x00, 0xFF, 0xFF); // blue
		SDL_RenderDrawLine(gfx_NN, 10, baseline_y, \
			NN_box_width - 10, baseline_y);
		SDL_SetRenderDrawColor(gfx_NN, 0x00, 0x00, 0xFF, 0x80); // faint blue
		SDL_RenderDrawLine(gfx_NN, 10, baseline_y - (int) Volume * gain, \
			NN_box_width - 10, baseline_y - (int) Volume * gain);

		// set color
		// float r = ((float) l ) / (numLayers);
		// float b = 1.0f - ((float) l ) / (numLayers);
		SDL_SetRenderDrawColor(gfx_NN, 0x00, 0xFF, 0x00, 0xFF); // green

		int numNeurons = net->layers[l].numNeurons;
		// numNeurons = actual number of neurons, not counting the bias neuron
		if (numNeurons == 1) // only 1 neuron in the layer
			{
			double output = Volume * gain * net->layers[l].neurons[0].output;

			int basepoint_x = NN_box_width / 2;
			SDL_RenderDrawLine(gfx_NN, basepoint_x, baseline_y, \
					basepoint_x, baseline_y - output);
			}
		else // > 1 neurons in the layer
			{
			// The line is divided into (numNeurons - 1) parts
			int neuronWidth = (NN_box_width - 20) / (numNeurons - 1);

			// for each neuron except the last
			// (because # of line segments = 1 less than # of neurons)
			// (note that "output" does not have a bias element)
			for (int n = 0; n < numNeurons - 1; n++)
				{
				double output0 = Volume * gain * net->layers[l].neurons[n].output;
				double output1 = Volume * gain * net->layers[l].neurons[n + 1].output;

				int basepoint_x = 10 + neuronWidth * n;
				SDL_RenderDrawLine(gfx_NN, basepoint_x, baseline_y - output0, \
					basepoint_x + neuronWidth, baseline_y - output1);
				}
			}
		}

	SDL_RenderPresent(gfx_NN);
	}

// Older version with vertical lines, suitable for many layers

void plot_NN_old(NNET *net)
	{
	SDL_SetRenderDrawColor(gfx_NN, 0, 0, 0, 0xFF);
	SDL_RenderClear(gfx_NN); //Clear screen

	#define Volume 20.0f
	#define NeuronWidth 20

	#define numLayers (net->numLayers)
	for (int l = 0; l < numLayers; l++)
		{
		double gain = 1.0f;
		// increase amplitude for hidden layers
		if (l > 0 && l < numLayers - 1)
			gain = 5.0f;
		else
			gain = 1.0f;

		// set color
		float r = ((float) l) / numLayers;
		float b = 1.0f - ((float) l) / numLayers;
		SDL_SetRenderDrawColor(gfx_NN, f2i(r), 0x60, f2i(b), 0xFF);

		int nn = net->layers[l].numNeurons;

		// draw baseline
		#define X_step ((NN_box_width - 20 - nn * NeuronWidth) / (numLayers - 1))
		int baseline_x = 10 + l * X_step;
		#define Y_step3 ((NN_box_height - (int) Volume * 14) / (numLayers - 1))
		int baseline_y = (int) Volume * 7 + l * Y_step3;
		SDL_RenderDrawLine(gfx_NN, baseline_x, baseline_y, \
			baseline_x + nn * NeuronWidth, baseline_y);

		SDL_SetRenderDrawColor(gfx_NN, f2i(r), 0xB0, f2i(b), 0xFF);

		for (int n = 0; n < nn; n++)
			{
			double output = gain * net->layers[l].neurons[n].output;

			int basepoint_x = baseline_x + NeuronWidth * n;
			SDL_RenderDrawLine(gfx_NN, basepoint_x, baseline_y, \
				basepoint_x, baseline_y - output * Volume);
			}
		}

	SDL_RenderPresent(gfx_NN);
	}

// *************************** Seh's NN visualizer *************************************

#define NN2_box_width 150
#define NN2_box_height 400

void start_NN2_plot(void)
	{

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		{
		printf("SDL_Init Error: %s \n", SDL_GetError());
		return;
		}

	win_NN2 = SDL_CreateWindow("NN activity", 800, 650, NN2_box_width, NN2_box_height, SDL_WINDOW_SHOWN);
	if (win_NN2 == NULL)
		{
		printf("SDL_CreateWindow Error: %s \n", SDL_GetError());
		SDL_Quit();
		return;
		}

	gfx_NN2 = SDL_CreateRenderer(win_NN2, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (gfx_NN2 == NULL)
		{
		SDL_DestroyWindow(win_NN2);
		printf("SDL_CreateRenderer Error: %s \n", SDL_GetError());
		SDL_Quit();
		return;
		}
	}

void rectI(int x, int y, int w, int h, int r, int g, int b)
	{
	SDL_Rect fillRect = {x, y, w, h};
	SDL_SetRenderDrawColor(gfx_NN2, r, g, b, 0xFF);
	SDL_RenderFillRect(gfx_NN2, &fillRect);
	}

void rect(int x, int y, int w, int h, float r, float g, float b)
	{
	rectI(x, y, w, h, f2i(r), f2i(g), f2i(b));
	}

void plot_NN2(NNET *net)
	{
	SDL_SetRenderDrawColor(gfx_NN2, 0, 0, 0, 0xFF);
	SDL_RenderClear(gfx_NN2); //Clear screen

	int bwh = 20; /* neuron block width,height*/
	#define numLayers (net->numLayers)
	#define L_margin ((NN2_box_width - (numLayers - 1) * bwh) / 2)
	#define T_margin ((NN2_box_height - nn * bwh) / 2)

	for (int l = 0; l < numLayers - 1; l++)
		{
		int nn = net->layers[l].numNeurons;
		for (int n = 0; n < nn; n++)
			{
			NEURON neuron = net->layers[l].neurons[n];
			double output = neuron.output;

			float r = output < 0 ? -output : 0;
			if (r < -1) r = -1;

			float g = output > 0 ? output : 0;
			if (g < +1) g = +1;

			// float b = neuron.input;	// ?? seems nothing in here
			float b = 0.0f;

			rect(L_margin + l*bwh, T_margin + n*bwh, bwh, bwh, r, g, b);
			}
		}

	SDL_RenderPresent(gfx_NN2);
	}

//******************************* K vector visualizer ******************************

#define K_box_width 600
#define K_box_height 200

void start_K_plot(void)
	{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
		{
		printf("SDL_Init Error: %s \n", SDL_GetError());
		return;
		}

	win_K = SDL_CreateWindow("K vector", 400, 200, K_box_width, K_box_height, SDL_WINDOW_SHOWN);
	if (win_K == NULL)
		{
		printf("SDL_CreateWindow Error: %s \n", SDL_GetError());
		SDL_Quit();
		return;
		}

	gfx_K = SDL_CreateRenderer(win_K, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (gfx_K == NULL)
		{
		SDL_DestroyWindow(win_K);
		printf("SDL_CreateRenderer Error: %s \n", SDL_GetError());
		SDL_Quit();
		return;
		}
	}

void line(int x1, double y1, int x2, double y2)
	{
	#define TopX 20
	#define TopY (K_box_height / 2)
	SDL_RenderDrawLine(gfx_K, x1 + TopX, (int) y1 + TopY, x2 + TopX, (int) y2 + TopY);
	}

// Show components of K vector as a line graph
extern double K[];

void plot_K()
	{
	// Draw base line
	#define K_Width ((K_box_width - TopX * 2) / dim_K)
	SDL_SetRenderDrawColor(gfx_K, 0xFF, 0x00, 0x00, 0xFF); // red line
	SDL_RenderDrawLine(gfx_K, 0, TopY, K_box_width, TopY);

	SDL_SetRenderDrawColor(gfx_K, 0x1E, 0xD3, 0xEB, 0xFF); // blue-smurf blue
	#define Amplitude 40.0f
	for (int k = 1; k < dim_K; ++k)
		line(k * K_Width, -Amplitude * K[k - 1],
			(k + 1) * K_Width, -Amplitude * K[k]);

	SDL_RenderPresent(gfx_K);
	}

void plot_trainer(double val)
	{
	//Clear screen
	SDL_SetRenderDrawColor(gfx_K, 0, 0, 0, 0xFF);
	SDL_RenderClear(gfx_K);

	int y = (int) (Amplitude * val);

	SDL_SetRenderDrawColor(gfx_K, 0xEB, 0xCC, 0x1E, 0xFF);
	SDL_Rect fillRect = {TopX, TopY - y, 5, y};
	SDL_RenderFillRect(gfx_K, &fillRect);
	SDL_RenderPresent(gfx_K);
	}

int delay_vis(int delay)
	{
	const Uint8 *keys = SDL_GetKeyboardState(NULL); // keyboard states

	SDL_Delay(delay);

	// SDL_Event event;
	// SDL_PollEvent(&event);
	// if (event.type == SDL_Quit)
	//	return 1;

	// Read keyboard state, if "Q" is pressed, return 1
	SDL_PumpEvents();

	// 'T' --- display time
	extern void end_timer(char *);
	if (keys[SDL_SCANCODE_T])
		end_timer(NULL);

	// 'P' --- pause
	if (keys[SDL_SCANCODE_P])
		while (!keys[SDL_SCANCODE_R])		// 'R' to resume
			SDL_PumpEvents();

	// 'Q' --- quit
	if (keys[SDL_SCANCODE_Q] || keys[SDL_SCANCODE_SPACE])
		return 1;
	else
		return 0;
	}

void pause_graphics()
	{
	const Uint8 *keys = SDL_GetKeyboardState(NULL); // keyboard states
	bool quit = NULL;
	SDL_Event e;

	//Update screen
	SDL_RenderPresent(gfx_K);

	//While application is running
	while (!quit)
		{
		//Handle events on queue
		while (SDL_PollEvent(&e) != 0)
			{
			//User requests quit
			if (e.type == SDL_QUIT) // This seems to be close-window event
				quit = true;
			if (keys[SDL_SCANCODE_Q] || keys[SDL_SCANCODE_SPACE]) // press 'Q' to quit
				quit = true;
			}

		if (gfx_LogErr != NULL)
			SDL_RenderPresent(gfx_LogErr);

		if (gfx_Ideal != NULL)
			SDL_RenderPresent(gfx_Ideal);

		if (gfx_Out != NULL)
			SDL_RenderPresent(gfx_Out);

		if (gfx_W != NULL)
			SDL_RenderPresent(gfx_W);

		if (gfx_NN != NULL)
			SDL_RenderPresent(gfx_NN);

		if (gfx_NN2 != NULL)
			SDL_RenderPresent(gfx_NN2);

		if (gfx_K != NULL)
			SDL_RenderPresent(gfx_K);
		}

	SDL_DestroyRenderer(gfx_NN);
	SDL_DestroyRenderer(gfx_NN2);
	SDL_DestroyRenderer(gfx_K);

	SDL_DestroyWindow(win_NN);
	SDL_DestroyWindow(win_NN2);
	SDL_DestroyWindow(win_K);

	SDL_Quit();
	}

void beep()
	{
	system("beep");
	/*
	Mix_Music *music;
	Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096);
	music = Mix_LoadMUS("beep.wav");
	Mix_PlayMusic(music, -1);
	SDL_PauseAudio(0);		// start playing device
	SDL_Delay(300);
	// fprintf(stderr, "Sound played\n");
	// SDL_PauseAudio(1);
	Mix_FreeMusic(music);
	Mix_CloseAudio();
	*/
	}

void quit_graphics()
	{
	SDL_DestroyRenderer(gfx_NN);
	SDL_DestroyRenderer(gfx_NN2);
	SDL_DestroyRenderer(gfx_K);

	SDL_DestroyWindow(win_NN);
	SDL_DestroyWindow(win_NN2);
	SDL_DestroyWindow(win_K);

	SDL_Quit();
	}

struct timeb startTime, endTime;

void start_timer()
	{
	ftime(&startTime);
	}

void end_timer(char *s)
	{
	ftime(&endTime);
	int duration = endTime.time - startTime.time;
	int minutes = duration / 60;
	int seconds = duration % 60;
	if (s == NULL)
		printf("\nTime elapsed = %d:%d\n\n", minutes, seconds);
	else
		sprintf(s, "%d:%02d", minutes, seconds);
	}
