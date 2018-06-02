/*
Chris Robertson
Parallel Programming
Heat Transfer
11-20-17
*/

#include<stdio.h>
#include<stdlib.h>
#include <algorithm>
#include <thread>
#include <cmath>
#include <string>
#include <iostream>
#include <mutex>
#include <sys\timeb.h>
#include "time.h";

using std::string;

const int ITERATIONS = 4000;

typedef struct {
	unsigned char red, green, blue;
} PPMPixel;

typedef struct {
	int x, y;
	PPMPixel *data;
} PPMImage;

#define RGB_COMPONENT_COLOR 255

/********************************************************************************
| Following barrier code sourced from: https://stackoverflow.com/a/27118537		|
|																				|
| This barrier is compatible with multiple generations of programming languages |
| and is immune safe from spurious wake-ups.									|
********************************************************************************/

class Barrier {
public:
	explicit Barrier(std::size_t iCount) :
		mThreshold(iCount),
		mCount(iCount),
		mGeneration(0) {
	}

	void Wait() {
		std::unique_lock<std::mutex> lLock{ mMutex };
		auto lGen = mGeneration;
		if (!--mCount) {
			mGeneration++;
			mCount = mThreshold;
			mCond.notify_all();
		}
		else {
			mCond.wait(lLock, [this, lGen] { return lGen != mGeneration; });
		}
	}

private:
	std::mutex mMutex;
	std::condition_variable mCond;
	std::size_t mThreshold;
	std::size_t mCount;
	std::size_t mGeneration;
};

/************************
| End of sourced code	|
***********************/

Barrier barrier(5);

static PPMImage *readPPM(const char *filename)
{
	char buff[16];
	PPMImage *img;
	FILE *fp;
	int c, rgb_comp_color;
	//open PPM file for reading
	//reading a binary file
	fopen_s(&fp, filename, "rb");
	if (!fp) {
		fprintf(stderr, "Unable to open file '%s'\n", filename);
		exit(1);
	}

	//read image format
	if (!fgets(buff, sizeof(buff), fp)) {
		perror(filename);
		exit(1);
	}

	//check the image format can be P3 or P6. P3:data is in ASCII format    P6: data is in byte format
	if (buff[0] != 'P' || buff[1] != '6') {
		fprintf(stderr, "Invalid image format (must be 'P6')\n");
		exit(1);
	}

	//alloc memory form image
	img = (PPMImage *)malloc(sizeof(PPMImage));
	if (!img) {
		fprintf(stderr, "Unable to allocate memory\n");
		exit(1);
	}

	//check for comments
	c = getc(fp);
	while (c == '#') {
		while (getc(fp) != '\n');
		c = getc(fp);
	}

	ungetc(c, fp);//last character read was out back
				  //read image size information
	if (fscanf_s(fp, "%d %d", &img->x, &img->y) != 2) {//if not reading widht and height of image means if there is no 2 values
		fprintf(stderr, "Invalid image size (error loading '%s')\n", filename);
		exit(1);
	}

	//read rgb component
	if (fscanf_s(fp, "%d", &rgb_comp_color) != 1) {
		fprintf(stderr, "Invalid rgb component (error loading '%s')\n", filename);
		exit(1);
	}

	//check rgb component depth
	if (rgb_comp_color != RGB_COMPONENT_COLOR) {
		fprintf(stderr, "'%s' does not have 8-bits components\n", filename);
		exit(1);
	}

	while (fgetc(fp) != '\n');
	//memory allocation for pixel data for all pixels
	img->data = (PPMPixel*)malloc(img->x * img->y * sizeof(PPMPixel));

	if (!img) {
		fprintf(stderr, "Unable to allocate memory\n");
		exit(1);
	}

	//read pixel data from file
	if (fread(img->data, 3 * img->x, img->y, fp) != img->y) { //3 channels, RGB
		fprintf(stderr, "Error loading image '%s'\n", filename);
		exit(1);
	}

	fclose(fp);
	return img;
}

void writePPM(const char *filename, PPMImage *img)
{
	FILE *fp;
	//open file for output
	//writing in binary format
	fopen_s(&fp, filename, "wb");
	if (!fp) {
		fprintf(stderr, "Unable to open file '%s'\n", filename);
		exit(1);
	}

	//write the header file
	//image format
	fprintf(fp, "P6\n");

	//image size
	fprintf(fp, "%d %d\n", img->x, img->y);

	// rgb component depth
	fprintf(fp, "%d\n", RGB_COMPONENT_COLOR);

	// pixel data
	fwrite(img->data, 3 * img->x, img->y, fp);
	fclose(fp);
}

void heat(PPMImage *src, PPMImage *dest, int slice)
{
	int i,
		start;
	double rate = 0.4;
	PPMImage *tempT;
	if (slice == 1)
	{
		start = 0;
	}
	else if (slice == 2)
	{
		start = (src->x * src->y) * 0.25;
	}
	else if (slice == 3)
	{
		start = (src->x * src->y) * 0.5;
	}
	else if (slice == 4)
	{
		start = (src->x * src->y) * 0.75;
	}
	else
	{
		return;
	}

	int topL,
		top,
		topR,
		left,
		right,
		bottomL,
		bottom,
		bottomR,
		min,
		max,
		pix;

	for (int idx = 0; idx < ITERATIONS; idx++)
	{
		i = start;
		tempT = dest;
		dest = src;
		src = tempT;
		topL = top = topR = left = right = bottomL = bottom = bottomR =  min = max = pix = 0;

		for (int x = 0; x < ((src->x * src->y) * 0.25); x++)
		{
			if ((i + x) == (((src->x * src->y) * 0.25) + (src->x * 0.25)) || (i + x) == (((src->x * src->y) * 0.25) + (src->x * 0.75)) || (i + x) == (((src->x * src->y) * 0.75) + (src->x * 0.25)) || (i + x) == (((src->x * src->y) * 0.75) + (src->x * 0.75)))
			{
				dest->data[i + x].red = dest->data[i + x].green = dest->data[i + x].blue = (unsigned char)255;
			}
			else if ((i + x) == 0) //top left corner
			{
				right = (int)src->data[(i + x) + 1].red;
				bottom = (int)src->data[(i + x) + src->x].red;
				bottomR = (int)src->data[((i + x) + src->x) + 1].red;
				
				min = right;
				if (bottom < min)
				{
					min = bottom;
				}
				if (bottomR < min)
				{
					min = bottomR;
				}

				max = right;
				if (bottom > max)
				{
					max = bottom;
				}
				if (bottomR > max)
				{
					max = bottomR;
				}
				
				pix = ((int)src->data[i + x].red + rate * ((right + bottom + bottomR) - (3 * (int)src->data[i + x].red)));
				
				if (pix > max || pix < min)
				{
					pix = (right + bottom + bottomR + (int)src->data[i + x].red) / 4;
				}
				
				dest->data[i + x].red = dest->data[i + x].green = dest->data[i + x].blue = (unsigned char)pix;
			}
			else if ((i + x) == src->x - 1) //top right corner
			{
				left = (int)src->data[(i + x) - 1].red;
				bottomL = (int)src->data[((i + x) + src->x) - 1].red;
				bottom = (int)src->data[(i + x) + src->x].red;
				
				min = left;
				if (bottomL < min)
				{
					min = bottomL;
				}
				if (bottom < min)
				{
					min = bottom;
				}

				max = left;
				if (bottomL > max)
				{
					max = bottomL;
				}
				if (bottom > max)
				{
					max = bottom;
				}
				
				pix = ((int)src->data[i + x].red + rate * ((left + bottomL + bottom) - (3 * (int)src->data[i + x].red)));
				
				if (pix > max || pix < min)
				{
					pix = (left + bottomL + bottom + (int)src->data[i + x].red) / 4;
				}
				
				dest->data[i + x].red = dest->data[i + x].green = dest->data[i + x].blue = (unsigned char)pix;
			}
			else if ((i + x) == (src->y - 1) * src->x) //bottom left corner
			{
				top = (int)src->data[(i + x) - src->x].red;
				topR = (int)src->data[((i + x) - src->x) + 1].red;
				right = (int)src->data[(i + x) + 1].red;
				
				min = top;
				if (topR < min)
				{
					min = topR;
				}
				if (right < min)
				{
					min = right;
				}

				max = top;
				if (topR > max)
				{
					max = topR;
				}
				if (right > max)
				{
					max = right;
				}
				
				pix = ((int)src->data[i + x].red + rate * ((top + topR + right) - (3 * (int)src->data[i + x].red)));
				
				if (pix > max || pix < min)
				{
					pix = (top + topR + right + (int)src->data[i + x].red) / 4;
				}
				
				dest->data[i + x].red = dest->data[i + x].green = dest->data[i + x].blue = (unsigned char)pix;
			}
			else if ((i + x) == (src->y * src->x) - 1) //bottom right corner
			{
				topL = (int)src->data[((i + x) - src->x) - 1].red;
				top = (int)src->data[(i + x) - src->x].red;
				left = (int)src->data[(i + x) - 1].red;
				
				min = topL;
				if (top < min)
				{
					min = top;
				}
				if (left < min)
				{
					min = left;
				}

				max = topL;
				if (top > max)
				{
					max = top;
				}
				if (left > max)
				{
					max = left;
				}
				
				pix = ((int)src->data[i + x].red + rate * ((topL + top + left) - (3 * (int)src->data[i + x].red)));
				
				if (pix > max || pix < min)
				{
					pix = (topL + top + left + (int)src->data[i + x].red) / 4;
				}
				
				dest->data[i + x].red = dest->data[i + x].green = dest->data[i + x].blue = (unsigned char)pix;
			}
			else if ((i + x) < src->x) //top row
			{
				left = (int)src->data[(i + x) - 1].red;
				right = (int)src->data[(i + x) + 1].red;
				bottomL = (int)src->data[((i + x) + src->x) - 1].red;
				bottom = (int)src->data[(i + x) + src->x].red;
				bottomR = (int)src->data[((i + x) + src->x) + 1].red;
				
				min = left;
				if (right < min)
				{
					min = right;
				}
				if (bottomL < min)
				{
					min = bottomL;
				}
				if (bottom < min)
				{
					min = bottom;
				}
				if (bottomR < min)
				{
					min = bottomR;
				}

				max = left;
				if (right > max)
				{
					max = right;
				}
				if (bottomL > max)
				{
					max = bottomL;
				}
				if (bottom > max)
				{
					max = bottom;
				}
				if (bottomR > max)
				{
					max = bottomR;
				}
				
				pix = ((int)src->data[i + x].red + rate * ((left + right + bottomL + bottom + bottomR) - (5 * (int)src->data[i + x].red)));
				
				if (pix > max || pix < min)
				{
					pix = (left + right + bottomL + bottom + bottomR + (int)src->data[i + x].red) / 6;
				}
				
				dest->data[i + x].red = dest->data[i + x].green = dest->data[i + x].blue = (unsigned char)pix;
			}
			else if ((i + x) > (src->y - 1) * src->x) //bottom row
			{
				topL = (int)src->data[((i + x) - src->x) - 1].red;
				top = (int)src->data[(i + x) - src->x].red;
				topR = (int)src->data[((i + x) - src->x) + 1].red;
				left = (int)src->data[(i + x) - 1].red;
				right = (int)src->data[(i + x) + 1].red;
				
				min = topL;
				if (top < min)
				{
					min = top;
				}
				if (topR < min)
				{
					min = topR;
				}
				if (left < min)
				{
					min = left;
				}
				if (right < min)
				{
					min = right;
				}

				max = topL;
				if (top > max)
				{
					max = top;
				}
				if (topR > max)
				{
					max = topR;
				}
				if (left > max)
				{
					max = left;
				}
				if (right > max)
				{
					max = right;
				}
				
				pix = ((int)src->data[i + x].red + rate * ((topL + top + topR + left + right) - (5 * (int)src->data[i + x].red)));
				
				if (pix > max || pix < min)
				{
					pix = (topL + top + topR + left + right + (int)src->data[i + x].red) / 6;
				}
				
				dest->data[i + x].red = dest->data[i + x].green = dest->data[i + x].blue = (unsigned char)pix;
			}
			else if ((i + x) % src->x == 0) //left border
			{
				top = (int)src->data[(i + x) - src->x].red;
				topR = (int)src->data[((i + x) - src->x) + 1].red;
				right = (int)src->data[(i + x) + 1].red;
				bottom = (int)src->data[(i + x) + src->x].red;
				bottomR = (int)src->data[((i + x) + src->x) + 1].red;
				
				min = top;
				if (topR < min)
				{
					min = topR;
				}
				if (right < min)
				{
					min = right;
				}
				if (bottom < min)
				{
					min = bottom;
				}
				if (bottomR < min)
				{
					min = bottomR;
				}

				max = top;
				if (topR > max)
				{
					max = topR;
				}
				if (right > max)
				{
					max = right;
				}
				if (bottom > max)
				{
					max = bottom;
				}
				if (bottomR > max)
				{
					max = bottomR;
				}
				
				pix = ((int)src->data[i + x].red + rate * ((top + topR + right + bottom + bottomR) - (5 * (int)src->data[i + x].red)));
				
				if (pix > max || pix < min)
				{
					pix = (top + topR + right + bottom + bottomR + (int)src->data[i + x].red) / 6;
				}
				
				dest->data[i + x].red = dest->data[i + x].green = dest->data[i + x].blue = (unsigned char)pix;
			}
			else if (((i + x) + 1) % src->x == 0) //right border
			{
				topL = (int)src->data[((i + x) - src->x) - 1].red;
				top = (int)src->data[(i + x) - src->x].red;
				left = (int)src->data[(i + x) - 1].red;
				bottomL = (int)src->data[((i + x) + src->x) - 1].red;
				bottom = (int)src->data[(i + x) + src->x].red;
				
				min = topL;
				if (top < min)
				{
					min = top;
				}
				if (left < min)
				{
					min = left;
				}
				if (bottomL < min)
				{
					min = bottomL;
				}
				if (bottom < min)
				{
					min = bottom;
				}

				max = topL;
				if (top > max)
				{
					max = top;
				}
				if (left > max)
				{
					max = left;
				}
				if (bottomL > max)
				{
					max = bottomL;
				}
				if (bottom > max)
				{
					max = bottom;
				}
				
				pix = ((int)src->data[i + x].red + rate * ((topL + top + left + bottomL + bottom) - (5 * (int)src->data[i + x].red)));
				
				if (pix > max || pix < min)
				{
					pix = (topL + top + left + bottomL + bottom + (int)src->data[i + x].red) / 6;
				}
				
				dest->data[i + x].red = dest->data[i + x].green = dest->data[i + x].blue = (unsigned char)pix;
			}
			else //not in corners or on borders
			{
				topL = (int)src->data[((i + x) - src->x) - 1].red;
				top = (int)src->data[(i + x) - src->x].red;
				topR = (int)src->data[((i + x) - src->x) + 1].red;
				left = (int)src->data[(i + x) - 1].red;
				right = (int)src->data[(i + x) + 1].red;
				bottomL = (int)src->data[((i + x) + src->x) - 1].red;
				bottom = (int)src->data[(i + x) + src->x].red;
				bottomR = (int)src->data[((i + x) + src->x) + 1].red;
				
				min = topL;
				if (top < min)
				{
					min = top;
				}
				if (topR < min)
				{
					min = topR;
				}
				if (left < min)
				{
					min = left;
				}
				if (right < min)
				{
					min = right;
				}
				if (bottomL < min)
				{
					min = bottomL;
				}
				if (bottom < min)
				{
					min = bottom;
				}
				if (bottomR < min)
				{
					min = bottomR;
				}

				max = topL;
				if (top > max)
				{
					max = top;
				}
				if (topR > max)
				{
					max = topR;
				}
				if (left > max)
				{
					max = left;
				}
				if (right > max)
				{
					max = right;
				}
				if (bottomL > max)
				{
					max = bottomL;
				}
				if (bottom > max)
				{
					max = bottom;
				}
				if (bottomR > max)
				{
					max = bottomR;
				}
				
				pix = ((int)src->data[i + x].red + rate * ((topL + top + topR + left + right + bottomL + bottom + bottomR) - (8 * (int)src->data[i + x].red)));
				
				if (pix > max || pix < min)
				{
					pix = (topL + top + topR + left + right + bottomL + bottom + bottomR + (int)src->data[i + x].red) / 9;
				}
				
				dest->data[i + x].red = dest->data[i + x].green = dest->data[i + x].blue = (unsigned char)pix;
			}
		}
		barrier.Wait();
	}
}

int main()
{
	PPMImage *imgEven,
		*imgOdd,
		*temp;
	string outname;

	struct timeb start, finish;
	int elapsed;

	imgEven = readPPM("edgeModel.ppm");
	imgOdd = readPPM("edgeModel.ppm");
	temp = readPPM("edgeModel.ppm");

	ftime(&start);

	std::thread t1(heat, imgEven, imgOdd, 1);
	std::thread t2(heat, imgEven, imgOdd, 2);
	std::thread t3(heat, imgEven, imgOdd, 3);
	std::thread t4(heat, imgEven, imgOdd, 4);

	std::cout << "Progress: [          ] 0% ";

	for (int i = 0; i < ITERATIONS; i++)
	{
		outname = "./iterations/heatModel" + std::to_string(i) + ".ppm";
		if ((i + 1) % (ITERATIONS / 100) == 0)
		{
			std::cout << "\rProgress: [";
			for (int j = 0; j < (i + 1) / (ITERATIONS / 10); j++)
			{
				std::cout << (unsigned char)254;
			}
			for (int j = 0; j < 10 - ((i + 1) / (ITERATIONS / 10)); j++)
			{
				std::cout << " ";
			}
			std::cout << "] " << (i + 1) / (double)(ITERATIONS / 100.0) << "% ";
		}
		barrier.Wait();
		if (i % 2 == 0)
		{
			for (int i = 0; i < temp->x * temp->y; i++)
			{
				if ((int)(imgEven->data[i].red) < 85)
				{
					temp->data[i].red = temp->data[i].green = 0;
					temp->data[i].blue = imgEven->data[i].red + 170;
				}
				else if ((int)(imgEven->data[i].red) < 170)
				{
					temp->data[i].red = temp->data[i].blue = 0;
					temp->data[i].green = imgEven->data[i].red + 85;
				}
				else
				{
					temp->data[i].green = temp->data[i].blue = 0;
					temp->data[i].green = imgEven->data[i].red;
				}
			}
			writePPM(outname.c_str(), temp);
		}
		else
		{
			for (int i = 0; i < imgOdd->x * imgOdd->y; i++)
			{
				temp->data[i].red = temp->data[i].green = temp->data[i].blue = imgOdd->data[i].red;
			}
			for (int i = 0; i < temp->x * temp->y; i++)
			{
				if ((int)(imgOdd->data[i].red) < 85)
				{
					temp->data[i].red = temp->data[i].green = 0;
					temp->data[i].blue = imgOdd->data[i].red + 170;
				}
				else if ((int)(imgOdd->data[i].red) < 170)
				{
					temp->data[i].red = temp->data[i].blue = 0;
					temp->data[i].green = imgOdd->data[i].red + 85;
				}
				else
				{
					temp->data[i].green = temp->data[i].blue = 0;
					temp->data[i].green = imgOdd->data[i].red;
				}
			}
			writePPM(outname.c_str(), temp);
		}
	}

	t1.join();
	t2.join();
	t3.join();
	t4.join();

	ftime(&finish);

	elapsed = (int)(1000.0 * (finish.time - start.time) + (finish.millitm - start.millitm));

	std::cout << "\n\nTotal time: " << (elapsed / 1000.00) << " seconds\n\n";

	system("pause");
	return 0;
}