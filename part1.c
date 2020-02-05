#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <intelfpgaup/video.h>

typedef unsigned char byte;
struct pixel { // Pixel data structure. Ordering of pixel colors in BMP files is b, g, r
   byte b;
   byte g;
   byte r;
};

// Read BMP file and extract the header (store in header) and pixel values (store in data)
int read_bmp(char* filename, byte** header, struct pixel** data, int *width, int *height) {
   struct pixel * data_;	// temporary pointer to pixel data
   byte * header_;			// temporary pointer to header data
	int width_, height_;		// temporary variables for width and height

   FILE* file = fopen (filename, "rb");
   if (!file) return -1;

   // read the 54-byte header
   header_ = malloc (54);
   fread (header_, sizeof(byte), 54, file);

   // get height and width of image
   width_ = *(int*) &header_[18];	// width is given by four bytes starting at offset 18
   height_ = *(int*) &header_[22];	// height is given by four bytes starting at offset 22

   // Read in the image
   int size = width_ * height_;
   data_= malloc (size * sizeof(struct pixel));
   fread (data_, sizeof(struct pixel), size, file); // read the rest of the data
   fclose (file);

   *header = header_;	// return pointer to caller
   *data = data_;			// return pointer to caller
	*width = width_;		// return value to caller
	*height = height_;	// return value to caller

   return 0;
}



// Determine the grayscale 8 bit value by averaging the r, g, and b channel values.
// Store the 8 bit grayscale value in the r channel.
void convert_to_grayscale(struct pixel *data, int width, int height) {
   int x, y;

   for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
      	// Just use the 8 bits in the r field to hold the entire grayscale image
         (*(data + y*width + x)).r = ((*(data + y*width + x)).r + (*(data + y*width + x)).g +
            (*(data + y*width + x)).b)/3;
      }
   }
}

// Write the grayscale image to disk. The 8-bit grayscale values should be inside the
// r channel of each pixel.
void write_grayscale_bmp(char *bmp, byte *header, struct pixel *data, int width, int height) {
   FILE* file = fopen (bmp, "wb");

   // write the 54-byte header
   fwrite (header, sizeof(byte), 54, file);
   int y, x;

   // the r field of the pixel has the grayscale value. Copy to g and b.
   for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
         (*(data + y*width + x)).b = (*(data + y*width + x)).r;
         (*(data + y*width + x)).g = (*(data + y*width + x)).r;
      }
   }
   int size = width * height;
   fwrite (data, sizeof(struct pixel), size, file); // write the rest of the data
   fclose (file);
}

// Draw the image pixels on the VGA display
void draw_image (struct pixel *data, int width, int height, int screen_x, int screen_y) {
	int x,y;
	short color;

    for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {

          color = ((((*(data + y*width + x)).r) >> 3) <<11) | ((((*(data + y*width + x)).g) >> 2) << 5)
                        | (((*(data + y*width + x)).b) >> 3);

    video_pixel(x,y, color);

      	// Just use the 8 bits in the r field to hold the entire grayscale image
      }
    }

video_show();
}


// Gaussian blur. Operate on the .r fields of the pixels only.
void gaussian_blur(struct pixel *data, int width, int height) {
   unsigned int gaussian_filter[5][5] = {
      { 2, 4, 5, 4, 2 },
      { 4, 9,12, 9, 4 },
      { 5,12,15,12, 5 },
      { 4, 9,12, 9, 4 },
      { 2, 4, 5, 4, 2 }
   };
    int i,j, k = 0;
    int sum[height*width];

    memset(sum, 0, sizeof(sum));

    int x,y;

    for (y = 0; y < height; y++) {

        for (x = 0; x < width; x++) {

            for (i = 0; i < 5; i++) {

                for (j = 0; j < 5; j++) {

                    if ((y-2+i) >= 0 && (y-2+i) < height){

                        if ((x-2+j) >= 0 && (x-2+j) < width){

                            sum[k] += ((*(data + (y - 2 + i)*width + (x - 2 + j))).r) * gaussian_filter[i][j];
                        }
                    }
                }
            }

            k++;
        }
    }

    for(i = 0; i < k+2; i++){

        (*(data + i)).r = sum[i] / 159;

    }

}

void sobel_filter(struct pixel *data, int width, int height) {
   // Definition of Sobel filter in horizontal and veritcal directions
   int horizontal_operator[3][3] = {
      { -1,  0,  1 },
      { -2,  0,  2 },
      { -1,  0,  1 }
   };
   int vertical_operator[3][3] = {
      { -1,  -2,  -1 },
      { 0,  0,  0 },
      { 1,  2,  1 }
   };

	int i,j,k = 0;
    int sum_x[height*width];
    int sum_y[height*width];
    int x,y;

     memset(sum_x, 0, sizeof(sum_x));
     memset(sum_y, 0, sizeof(sum_y));


    for (y = 0; y < height; y++) {

        for (x = 0; x < width; x++) {

            for (i = 0; i < 3; i++) {

                for (j = 0; j < 3; j++) {

                    if ((y-1+i) >= 0 && (y-1+i) < height){

                        if ((x-1+j) >= 0 && (x-1+j) < width){
                            sum_x[k] += ((*(data + (y - 1 + i)*width + (x - 1 + j))).r) * horizontal_operator[i][j];
                            sum_y[k] += ((*(data + (y - 1 + i)*width + (x - 1 + j))).r) * vertical_operator[i][j];

                        }
                    }
                }
            }

            k++;
        }
    }

    for(i = 0; i < k; i++){

        (*(data + i)).r = (abs(sum_x[i])/2) + (abs(sum_y[i])/2);

    }
}

void non_maximum_suppressor(struct pixel *data, int width, int height) {

    //go through the pixels setting boundary pixels as 0, find the grayscale sum of each line
    //which ever line has the highest value check pixels next to it and if not greater than set to zero

    int x,y;
    int line_A = 0, line_B = 0, line_C = 0, line_D = 0;
    int pixel_val[height][width];

    memset(pixel_val, 0, sizeof(pixel_val));

    for (y = 0; y < height; y++) {

        for (x = 0; x < width; x++) {

            if(x == 0 || y == 0 || x == (width - 1) || y == (height - 1)){

                (*(data + y *width + x)).r = 0;
            }

            else{

                line_A = (*(data + y *width + x)).r + (*(data + (y-1) *width + x)).r + (*(data + (y+1) *width + x)).r;
                line_B = (*(data + y *width + x)).r + (*(data + (y-1) *width + (x+1))).r + (*(data + (y+1) *width + (x-1))).r;
                line_C = (*(data + y *width + x)).r + (*(data + y *width + (x-1))).r + (*(data + y *width + (x+1))).r;
                line_D = (*(data + y *width + x)).r + (*(data + (y-1) *width + (x-1))).r + (*(data + (y+1) *width + (x+1))).r;

                if(line_A > line_B && line_A > line_C && line_A > line_D){

                    if ((*(data + y *width + x)).r > (*(data + y *width + (x-1))).r
                        && (*(data + y *width + x)).r > (*(data + y *width + (x+1))).r){

                        pixel_val[y][x] = (*(data + y *width + x)).r;

                        }

                    else{

                        pixel_val[y][x] = 0;

                        }

                }

                if(line_B > line_A && line_B > line_C && line_B > line_D){

                    if ((*(data + y *width + x)).r > (*(data + (y-1) *width + (x-1))).r
                        && (*(data + y *width + x)).r > (*(data + (y+1) *width + (x+1))).r){

                        pixel_val[y][x] = (*(data + y *width + x)).r;
                        }

                    else{

                        pixel_val[y][x] = 0;

                    }

                }

                if(line_C > line_A && line_C > line_B && line_C > line_D){

                    if ((*(data + y *width + x)).r > (*(data + (y+1) *width + x)).r
                        && (*(data + y *width + x)).r > (*(data + (y-1) *width + x)).r){

                        pixel_val[y][x] = (*(data + y *width + x)).r;
                        }

                    else{

                        pixel_val[y][x] = 0;

                    }

                }

                if(line_D > line_A && line_D > line_B && line_D > line_C){

                    if ((*(data + y *width + x)).r > (*(data + (y-1) *width + (x+1))).r
                        && (*(data + y *width + x)).r > (*(data + (y+1) *width + (x-1))).r){

                        pixel_val[y][x] = (*(data + y *width + x)).r;
                        }

                    else{

                      pixel_val[y][x] = 0;

                    }
                }

            }
        }
    }

    for (y = 0; y < height; y++) {

        for (x = 0; x < width; x++) {


            (*(data + y *width + x)).r = pixel_val[y][x];

        }
    }

}

// Only keep pixels that are next to at least one strong pixel.
void hysteresis_filter(struct pixel *data, int width, int height) {
   #define strong_pixel_threshold 32	// example value

    int x,y;
    int i,j;
    int pixel_val[height][width];
    int strength_flag = 0;

    for (y = 0; y < height; y++) {

        for (x = 0; x < width; x++) {

            strength_flag = 0;
//
//            if(x == 0 || y == 0 || x == (width - 1) || y == (height - 1)){
//
//                pixel_val[y][x] = 0;
//            }
//
//            else{

                //if the pixel is greater than strong_pixel_threshold, check each adjacent pixel and if even one is greater keep pixel value
                //else set pixel to zero

                if ((*(data + y *width + x)).r > strong_pixel_threshold){

                    for (i = 0; i < 3; i++) {

                        for (j = 0; j < 3; j++) {

                            if ((y-1+i) >= 0 && (y-1+i) < height){

                                if ((x-1+j) >= 0 && (x-1+j) < width){

                                    if (i!=1 && j!=1){

                                        if ((*(data + (y - 1 + i)*width + (x - 1 + j))).r > strong_pixel_threshold){

                                            strength_flag = 1;
                                        }

                                    }
                                }
                            }

                        }
                    }

                    if (!strength_flag){

                        pixel_val[y][x] = 0;

                    }

                    if (strength_flag){

                        pixel_val[y][x] = (*(data + y*width + x)).r;

                    }
                }

                else{

                    pixel_val[y][x] = 0;
                }

            //}

           //strength_flag = 0;
        }
    }

    for (y = 0; y < height; y++) {

        for (x = 0; x < width; x++) {


            (*(data + y *width + x)).r = pixel_val[y][x];

        }
    }

}

//function to flip image
void flip(struct pixel *data, int width, int height){

    int x,y;
    int pixel_val_r[height][width];
    int pixel_val_g[height][width];
    int pixel_val_b[height][width];

    memset(pixel_val_r, 0, sizeof(pixel_val_r));
    memset(pixel_val_g, 0, sizeof(pixel_val_g));
    memset(pixel_val_b, 0, sizeof(pixel_val_b));

    for(y = height-1; y >= 0; y--){

        for(x = 0; x < width; x++){

            pixel_val_r[(height - 1) - y][x] = ((*(data + y*width + x)).r);
            pixel_val_g[(height - 1) - y][x] = ((*(data + y*width + x)).g);
            pixel_val_b[(height - 1) - y][x] = ((*(data + y*width + x)).b);
        }

    }

    for(y = 0; y < height; y++){

        for(x = 0; x < width; x++){

            ((*(data + y*width + x)).r) = pixel_val_r[y][x];
            ((*(data + y*width + x)).g) = pixel_val_g[y][x];
            ((*(data + y*width + x)).b) = pixel_val_b[y][x];

        }

    }


}

int main(int argc, char *argv[]) {
   struct pixel *data;								// used to hold the image pixels
   byte *header;										// used to hold the image header
	int width, height;								// the dimensions of the image
	int screen_x, screen_y, char_x, char_y;	// VGA screen dimensions
   time_t start, end;                       // used to measure the program's run-time


   // Check inputs
   if (argc < 2) {
      printf ("Usage: edgedetect <BMP filename>\n");
      return 0;
   }

   // Open input image file (24-bit bitmap image)
   if (read_bmp (argv[1], &header, &data, &width, &height) < 0) {
      printf ("Failed to read BMP\n");
      return 0;
   }

   if (!video_open ())	// open the VGA display driver
   {
      printf ("Error: could not open video device\n");
      return -1;
   }
   video_read (&screen_x, &screen_y, &char_x, &char_y);   // get VGA screen size

   flip(data, width, height);
   draw_image (data, width, height, screen_x, screen_y);

   /********************************************
   *          IMAGE PROCESSING STAGES          *
   ********************************************/

   // Start measuring time
   start = clock();

   convert_to_grayscale(data, width, height);
   gaussian_blur (data, width, height);
    sobel_filter (data, width, height);
   non_maximum_suppressor (data, width, height);
  hysteresis_filter (data, width, height);

   end = clock();

   printf ("TIME ELAPSED: %.0f ms\n", ((double) (end - start)) * 1000 / CLOCKS_PER_SEC);

    write_grayscale_bmp ("edges.bmp", header, data, width, height);

   printf ("Press return to continue");
   getchar ();
   draw_image (data, width, height, screen_x, screen_y);



   video_close ( );
   return 0;
}
