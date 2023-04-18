#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
#include <time.h>
#include <math.h>
#include <random>
#include <thread>

#include <iostream> // for debugging only

#define M_PI 3.14159265358979323846

const int size_x = 100;
const int size_y = 100;
const int upscale_x = 400;
const int upscale_y = 400;

const int blobs_x = 3;
const int blobs_y = 3;

const bool use_optimized_pixel_calc = 1;
const double optimization_multiplier = 1;

const bool use_multithreading = 0;
const int max_threads = 4;

const double gamma_setting = 0.7;
const double gamma = 1 / gamma_setting;
const int shades_per_channel = 64; // use 0 for full range

const double min_size_multiplier = 1;
const double max_size_multiplier = 2;
const double min_speed = 0.5;
const double max_speed = 4;
const double min_color_speed = 2;
const double max_color_speed = 5;

const double step_x = size_x / blobs_x;
const double offset_x = step_x / 2;
const double step_y = size_y / blobs_y;
const double offset_y = step_y / 2;
const double min_step = step_x < step_y ? step_x : step_y;
const double min_size = min_step * min_size_multiplier;
const double max_size = min_step * max_size_multiplier;

const int thread_column_width = size_x % max_threads > 0 ? size_x / max_threads + 1 : size_x / max_threads;

class Color{
	public:
		double r = 0;
		double g = 0;
		double b = 0;
		double h = 0;
		double s = 0;
		double v = 0;
};

Color colorRGB(double r, double g, double b){
	double cmin = 1;
	if(r < cmin){cmin = r;}
	if(g < cmin){cmin = g;}
	if(b < cmin){cmin = b;}

	double cmax = 0;
	char max_color = 'r';
	if(r > cmax){cmax = r; max_color = 'r';}
	if(g > cmax){cmax = g; max_color = 'g';}
	if(b > cmax){cmax = b; max_color = 'b';}

	double d = cmax - cmin;
	if(d == 0){max_color = '0';}

	double h = 0;
	switch(max_color){
		case 'r':
			h = std::fmod((g - b) / d, 6);
			break;
		case 'g':
			h = (b - r) / d + 2;
			break;
		case 'b':
			h = (r - g) / d + 4;
			break;
		default:
			break;
	}
	h /= 6; // to keep in 0 - 1 range

	double s = 0;
	if(cmax != 0){s = d / cmax;}

	Color color;
	color.r = r;
	color.g = g;
	color.b = b;
	color.h = h;
	color.s = s;
	color.v = cmax;
	return color;
}

Color colorHSV(double h, double s, double v){
	double c,h6,m,x;
	c = v * s;

	h6 = h * 6;
	x = (1 - std::abs( std::fmod(h6,2) - 1 ) ) * c;

	double r,g,b;
	int h6_int = (int) h6;
	switch(h6_int){
		case 0:
			r = c;
			g = x;
			b = 0;
			break;
		case 1:
			r = x;
			g = c;
			b = 0;
			break;
		case 2:
			r = 0;
			g = c;
			b = x;
			break;
		case 3:
			r = 0;
			g = x;
			b = c;
			break;
		case 4:
			r = x;
			g = 0;
			b = c;
			break;
		case 5:
			r = c;
			g = 0;
			b = x;
			break;
		default:
			r = 0;
			g = 0;
			b = 0;
			break;
	}

	m = v - c;
	r += m;
	g += m;
	b += m;

	Color color;
	color.r = r;
	color.g = g;
	color.b = b;
	color.h = h;
	color.s = s;
	color.v = v;
	return color;
}

Color correctColor(double r, double g, double b){
	double bleed_r = r - 1;
	double bleed_g = g - 1;
	double bleed_b = b - 1;
	if(bleed_r < 0){bleed_r = 0;}
	if(bleed_g < 0){bleed_g = 0;}
	if(bleed_b < 0){bleed_b = 0;}

	r += bleed_g + bleed_b;
	g += bleed_b + bleed_r;
	b += bleed_r + bleed_g;

	if(r > 1){r = 1;}
	if(g > 1){g = 1;}
	if(b > 1){b = 1;}
	if(r < 0){r = 0;}
	if(g < 0){g = 0;}
	if(b < 0){b = 0;}

	if(shades_per_channel > 0){
		r = (int)(r * shades_per_channel) / (double)shades_per_channel;
		g = (int)(g * shades_per_channel) / (double)shades_per_channel;
		b = (int)(b * shades_per_channel) / (double)shades_per_channel;
	}

	Color final_color = colorRGB(r,g,b);
	return final_color;
}

// Color blend3colors(Color color1, Color color2, Color color3){
// 	int r,g,b;
// 	r = color1.r + color2.r + color3.r;
// 	g = color1.g + color2.g + color3.g;
// 	b = color1.b + color2.b + color3.b;
// 	if(r > 1){r = 1;}
// 	if(g > 1){g = 1;}
// 	if(b > 1){b = 1;}
// 	if(r < 0){r = 0;}
// 	if(g < 0){g = 0;}
// 	if(b < 0){b = 0;}
// 	Color final_color = colorRGB(r,g,b);
// 	return final_color;
// }

double distance(double x1, double y1, double x2, double y2){
	double x = x2 - x1;
	double y = y2 - y1;
	return std::sqrt(x * x + y * y);
}

double randomDouble(double min, double max){
	//std::srand(std::time(NULL));
	double value = static_cast <double> (rand()) / static_cast <double> (RAND_MAX);
	//std::cout << value <<"\n";
	return value;
}

class Blob{
	public:
		double x;
		double y;
		double min_size;
		double max_size;
		double size_offset;
		double size;
		double speed;
		double hue_offset;
		double hue_shift_speed;
		Color color;
};

Blob blob[blobs_x][blobs_y];
std::thread thread_pool[max_threads];
int x1_for_thread[max_threads];
int x2_for_thread[max_threads];
Color screen[size_x][size_y];

Color calculatePixel(int x, int y, int id = -1){
	double r = 0;
	double g = 0;
	double b = 0;
	
	int min_blob_x, min_blob_y;
	int max_blob_x, max_blob_y;
	if(use_optimized_pixel_calc){
		int closest_blob_x = x / step_x;
		int closest_blob_y = y / step_y;
		min_blob_x = closest_blob_x - max_size_multiplier * optimization_multiplier;
		min_blob_y = closest_blob_y - max_size_multiplier * optimization_multiplier;
		max_blob_x = closest_blob_x + max_size_multiplier * optimization_multiplier + 1;
		max_blob_y = closest_blob_y + max_size_multiplier * optimization_multiplier + 1;
		if(min_blob_x < 0){min_blob_x = 0;}
		if(min_blob_y < 0){min_blob_y = 0;}
		if(max_blob_x > blobs_x){max_blob_x = blobs_x;}
		if(max_blob_y > blobs_y){max_blob_y = blobs_y;}
		//std::cout << min_blob_x << " " << min_blob_y << " - " << max_blob_x << " " << max_blob_y << "\n";
	}else{
		min_blob_x = 0;
		min_blob_y = 0;
		max_blob_x = blobs_x;
		max_blob_y = blobs_y;
	}

	for(int by = min_blob_y; by < max_blob_y; by++){
		for(int bx = min_blob_x; bx < max_blob_x; bx++){
			Blob bl = blob[bx][by]; //for readability
			double d = distance(x, y, bl.x, bl.y);
			if(d < bl.size){
				r += std::pow((bl.size - d) / bl.size * bl.color.r, gamma);
				g += std::pow((bl.size - d) / bl.size * bl.color.g, gamma);
				b += std::pow((bl.size - d) / bl.size * bl.color.b, gamma);
			}
		}
	}
	if(r > 2){r = 2;}
	if(g > 2){g = 2;}
	if(b > 2){b = 2;}
	Color final_color = correctColor(r,g,b);
	return final_color;
}

class BlobMain : public olc::PixelGameEngine{

	public:
		void drawRegion(int id){
			//std::cout << id << "\n";
			int x1,y1,x2,y2;
			if(id < 0){
				x1 = 0;
				x2 = size_x;
				y1 = 0;
				y2 = size_y;
			}else{
				x1 = x1_for_thread[id];
				x2 = x1_for_thread[id];
				y1 = 0;
				y2 = size_y;
			}
			for (int y = y1; y < y2; y++){
				for (int x = x1; x < x2; x++){
					Color final_color = calculatePixel(x,y);
					screen[x][y] = final_color;
				}
			}
		}

		bool OnUserCreate() override{
			sAppName = "Blobs";
			std::srand(std::time(NULL));
			for(int y = 0; y < blobs_y; y++){
				for(int x = 0; x < blobs_x; x++){
					blob[x][y].x = offset_x + step_x * x;
					blob[x][y].y = offset_y + step_y * y;
					blob[x][y].min_size = min_size;
					blob[x][y].max_size = max_size;
					blob[x][y].size_offset = randomDouble(0, 2 * M_PI);
					blob[x][y].speed = randomDouble(min_speed, max_speed);
					blob[x][y].hue_offset = randomDouble(0, 1);
					blob[x][y].hue_shift_speed = randomDouble(min_color_speed, max_color_speed);
				}
			}
			return true;
		}

		bool OnUserUpdate(float fElapsedTime) override{
			for(int y = 0; y < blobs_y; y++){
				for(int x = 0; x < blobs_x; x++){
					blob[x][y].size_offset += blob[x][y].speed * fElapsedTime;
					blob[x][y].size_offset = std::fmod(blob[x][y].size_offset, 2 * M_PI);
					blob[x][y].size = (std::sin(blob[x][y].size_offset) + 1) / 2 * (max_size - min_size) + min_size;
					blob[x][y].hue_offset += blob[x][y].hue_shift_speed * fElapsedTime;
					blob[x][y].hue_offset = std::fmod(blob[x][y].hue_offset, 1);
					blob[x][y].color = colorHSV(blob[x][y].hue_offset,1,1);
				}
			}
			if(use_multithreading){
				for(int thread = 0; thread < max_threads; thread++){
					// WHY WON'T YOU JUST WORK?!?!!
					//thread_pool[thread] = std::thread(drawRegion, x1_for_thread[thread], 0, x2_for_thread[thread], size_y);
					//thread_pool[thread] = std::thread(&BlobMain::drawRegion, BlobMain(), x1_for_thread[thread], 0, x2_for_thread[thread], size_y);
					thread_pool[thread] = std::thread(&BlobMain::drawRegion, BlobMain(), thread);
				}
				for(int thread = 0; thread < max_threads; thread++){
					thread_pool[thread].join();
				}
			}else{
				drawRegion(-1);
			}
			for(int y = 0; y < size_y; y++){
				for(int x = 0; x < size_x; x++){
					Draw(x, y, olc::Pixel(screen[x][y].r * 255, screen[x][y].g * 255, screen[x][y].b * 255));
					//std::cout << "Plotted pixel " << x << ":" << y << "\n";
				}
			}
			return true;
		}
};

int main(){
	for(int thread = 0; thread < max_threads; thread++){
		x1_for_thread[thread] = thread * thread_column_width;
		x2_for_thread[thread] = x1_for_thread[thread] + thread_column_width;
		if(x2_for_thread[thread] > size_x){x2_for_thread[thread] > size_x;}
	}
	BlobMain blob_main;
	if (blob_main.Construct(size_x, size_y, upscale_x / size_x, upscale_y / size_y))
		blob_main.Start();
	return 0;
}
