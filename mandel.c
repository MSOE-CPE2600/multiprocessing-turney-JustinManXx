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
///

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <string.h>
#include "jpegrw.h"

// Function prototypes
static void generate_image(const char *outfile, double xcenter, double ycenter, 
	double xscale, double yscale, int max, int imageWidth, int imageHeight);
static void show_help();

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


    // Parse command-line arguments
    while ((c = getopt(argc, argv, "n:x:y:s:W:H:m:f:p:Sh")) != -1) {
        switch (c) {
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
    printf("Generating %d frames using %d child processes...\n", numFrames, numChildren);

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
            generate_image(outfile, xcenter, ycenter, 
			currentXScale, currentYScale, max, imageWidth, imageHeight);
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
	

// Function to generate a single Mandelbrot image
static void generate_image(const char *outfile, double xcenter, double ycenter,
double xscale, double yscale, int max, int imageWidth, int imageHeight) {
    printf("Generating image: %s\n", outfile);

    imgRawImage* img = initRawImage(imageWidth, imageHeight);
    setImageCOLOR(img, 0);

    double xmin = xcenter - xscale / 2;
    double xmax = xcenter + xscale / 2;
    double ymin = ycenter - yscale / 2;
    double ymax = ycenter + yscale / 2;

    int width = img->width;
    int height = img->height;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            double x = xmin + i * (xmax - xmin) / width;
            double y = ymin + j * (ymax - ymin) / height;
            int iter = 0;
            double x0 = x, y0 = y;

            while ((x * x + y * y <= 4) && iter < max) {
                double xt = x * x - y * y + x0;
                double yt = 2 * x * y + y0;
                x = xt;
                y = yt;
                iter++;
            }

            int color = 0xFFFFFF * iter / (double)max;
            setPixelCOLOR(img, i, j, color);
        }
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
}
