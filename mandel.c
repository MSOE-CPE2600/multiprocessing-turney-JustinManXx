/// 
//  mandel.c
//  Based on example code found here:
//  https://users.cs.fiu.edu/~cpoellab/teaching/cop4610_fall22/project3.html
//  
//  Converted to use jpg instead of BMP and other minor changes
//  
// Modified by:
// Justin Mahr
// The program can generate multiple frames in parallel using child processes 
// and optionally manages synchronization with semaphores for more efficient execution.
// It now can use threading and processes at the same time
///

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <string.h>
#include "jpegrw.h"
#include <pthread.h>


// Function prototypes
static void generateImage(const char *outfile, double xcenter, double ycenter,
                          double width, double height, int xres, int yres, 
                          int max_iter, int colorscheme);

static void show_help();

// ThreadArgs Structure
typedef struct {
    int startRow;
    int endRow;
    double xmin;
    double xmax;
    double ymax;
    double ymin;
    int imageWidth;
    int imageHeight;
    int max;
    imgRawImage* img;
} threading;

//  Run the multiprocessing and command line interface
int main(int argc, char *argv[]) {
    char c;
    int numChildren = 1;
    const char *outfilePrefix = "mandel";
    double xcenter = 0;
    double ycenter = 0; 
    double xscale = 4;
    double yscale = 0;
    int imageWidth = 1000;
    int imageHeight = 1000;
    int max = 1000;
    int numFrames = 50;
    int useSemaphore = 0;
    int numThreads = 1;


    // Parse command-line arguments
    while ((c = getopt(argc, argv, "t:n:x:y:s:W:H:m:f:p:Sh")) != -1) {
        switch (c) {
            case 't':
                numThreads = atoi(optarg);
                if (numThreads < 1 || numThreads > 20) {
                    fprintf(stderr, "Number of threads must be between 1 and 20.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'n': 
				numChildren = atoi(optarg); 
				break;
            case 'x': 
				xcenter = atof(optarg); 
				break;
            case 'y': 
				ycenter = atof(optarg); 
				break;
            case 's': 
				xscale = atof(optarg); 
				break;
            case 'W': 
				imageWidth = atoi(optarg); 
				break;
            case 'H': 
				imageHeight = atoi(optarg); 
				break;
            case 'm': 
				max = atoi(optarg); 
				break;
            case 'f': 
				numFrames = atoi(optarg); 
				break;
            case 'p': 
				outfilePrefix = optarg; 
				break;
            case 'S': 
				useSemaphore = 1; 
				break;
            case 'h': 
				show_help(); 
				exit(1);
				break;
        }
    }

    // Sets yscale incase command line input was entered
    yscale = xscale / imageWidth * imageHeight;

    // Initializes the semaphores
    sem_t *sem = NULL;

    // Creates the semaphores if use is on
    if (useSemaphore) {
        sem = sem_open("/mandelSem", O_CREAT, 0644, numChildren);
        if (sem == SEM_FAILED) {
            perror("sem_open");
            exit(EXIT_FAILURE);
        }
    }

    // User interface and debug line
    printf("Generating %d frames using %d child processes & %d threads\n", numFrames, numChildren, numThreads);

    // This is the multiprossessing framework
    // Forks until the number of images are created that was need
    for (int i = 0; i < numFrames; i++) {
        double scaleFactor = 1.0 - 0.02 * i; 
        double currentXScale = xscale * scaleFactor;
        double currentYScale = yscale * scaleFactor;
        char outfile[256];
        snprintf(outfile, sizeof(outfile), "%s_%03d.jpg", outfilePrefix, i);

      	if (useSemaphore) sem_wait(sem);

        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            generateImage(outfile, xcenter, ycenter, 
			currentXScale, currentYScale, max, imageWidth, imageHeight, numThreads);
            if (useSemaphore) sem_post(sem);
            exit(0);
        } else if (pid < 0) {
            perror("fork");
            if (useSemaphore) sem_post(sem);
        }
    }

    // Wait for all children to complete
    while (wait(NULL) > 0);

    if (useSemaphore) {
        sem_close(sem);
        sem_unlink("/mandelSem");
    }

    printf("All frames generated successfully.\n");
    return 0;
}

// Splits up the Regions of the Mandel
void* generateRegion(void* args) {
    threading* threadArgs = (threading*)args;
    for (int j = threadArgs->startRow; j < threadArgs->endRow; j++) {
        for (int p = 0; p < threadArgs->imageWidth; p++) {
            double x = threadArgs->xmin + p * (threadArgs->xmax - threadArgs->xmin) / threadArgs->imageWidth;
            double y = threadArgs->ymin + j * (threadArgs->ymax - threadArgs->ymin) / threadArgs->imageHeight;
            int iter = 0;
            double x0 = x;
            double y0 = y;

            while ((x * x + y * y <= 4) && iter < threadArgs->max) {
                double xt = x * x - y * y + x0;
                double yt = 2 * x * y + y0;
                x = xt;
                y = yt;
                iter++;
            }
            int color = 0xFFFFFF * iter / (double)threadArgs->max;
            setPixelCOLOR(threadArgs->img, p, j, color);
        }
    }
    return NULL;
}

// Function to generate a single Mandelbrot image
static void generateImage(const char *outfile, double xcenter, double ycenter,
                          double xscale, double yscale, int max,
                          int imageWidth, int imageHeight, int numThreads) {
    printf("Generating image: %s\n", outfile);

    imgRawImage* img = initRawImage(imageWidth, imageHeight);
    setImageCOLOR(img, 0);

    double xmin = xcenter - xscale / 2;
    double xmax = xcenter + xscale / 2;
    double ymin = ycenter - yscale / 2;
    double ymax = ycenter + yscale / 2;

    pthread_t threads[numThreads];
    threading threadArgs[numThreads];

    int rowsPerThread = imageHeight / numThreads;
    for (int i = 0; i < numThreads; i++) {
        threadArgs[i].startRow = i * rowsPerThread;
        threadArgs[i].endRow = (i == numThreads - 1) ? imageHeight : (i + 1) * rowsPerThread;
        threadArgs[i].xmin = xmin;
        threadArgs[i].xmax = xmax;
        threadArgs[i].ymin = ymin;
        threadArgs[i].ymax = ymax;
        threadArgs[i].imageWidth = imageWidth;
        threadArgs[i].imageHeight = imageHeight;
        threadArgs[i].max = max;
        threadArgs[i].img = img;

        if (pthread_create(&threads[i], NULL, generateRegion, &threadArgs[i]) != 0) {
            perror("Error creating thread");
            exit(EXIT_FAILURE);
        }
    }

    // Wait for all threads to complete
    for (int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    storeJpegImageFile(img, outfile);
    freeRawImage(img);
}

// Show help message
static void show_help() {
    printf("Usage: mandelmovie [options]\n");
    printf("Options:\n");
    printf("  -n <num>    Number of child processes to use (default=1)\n");
    printf("  -x <coord>  X center coordinate of image (default=0)\n");
    printf("  -y <coord>  Y center coordinate of image (default=0)\n");
    printf("  -s <scale>  Scale of the image in Mandelbrot coordinates (X-axis). (default=4)\n");
    printf("  -W <pixels> Image width in pixels (default=1000)\n");
    printf("  -H <pixels> Image height in pixels (default=1000)\n");
    printf("  -m <max>    Maximum iterations per point (default=1000)\n");
    printf("  -f <frames> Number of frames to generate (default=50)\n");
    printf("  -p <prefix> Output file prefix (default='mandel')\n");
    printf("  -S          Use semaphore to manage child processes\n");
    printf("  -h          Show this help message\n");
    printf("  -t <numThreads> Number of threads to use (default=1)\n");
}
