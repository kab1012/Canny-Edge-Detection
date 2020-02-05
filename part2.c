#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "address_map_arm.h"
#include "physical.h"

void *mem_virtual;
typedef unsigned char byte;

// Declared data structure for a pixel. The ordering of pixel colors in BMP files is b, g, r
struct pixel {
   byte b;
   byte g;
   byte r;
};

// Read BMP file and extract the pixel values (store in data) and header (store in header)
// data is data[0] = BLUE, data[1] = GREEN, data[2] = RED, etc...
int read_bmp(char *bmp, byte **header, struct pixel **data, int *width, int *height) {
   FILE *file = fopen(bmp, "rb");
   if (!file) return -1;

   // read the 54-byte header
   byte * header_ = malloc (54);
   fread (header_, sizeof(byte), 54, file);

   // get height and width of image
   int width_ = *(int*) &header_[18];	// width is given by four bytes starting at offset 18
   int height_ = *(int*) &header_[22];	// height is given by four bytes starting at offset 22

   // Read in the image
   int size = width_ * height_;
   struct pixel *data_ = malloc (size * sizeof(struct pixel));
   fread (data_, sizeof(struct pixel), size, file);	// read the rest of the data
   fclose(file);

   *header = header_;
   *data = data_;
	*width = width_;
	*height = height_;

   return 0;
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

// The video IP cores used for edge detection require the RGB 24 bits of each pixel to be
// word aligned (aka 1 byte of padding per pixel):
// | unused 8 bits  | red 8 bits | green 8 bits | blue 8 bits |
void memcpy_consecutive_to_padded(struct pixel *from, volatile unsigned int *to, int pixels){

    int i;

    for(i=0; i < pixels; i++){

        *(to + i) = ((*(from + i)).r << 16) |  ((*(from + i)).g << 8) | ((*(from + i)).b);

        *(to + i) &= ~0xFF000000;

    }
}

int main(int argc, char *argv[]){
   struct pixel *data;		// used to hold the image pixels
   byte *header;				// used to hold the image header
	int width, height;		// image size
   int fd = -1;				// used to open/read/etc. the image file
	void *LW_virtual;
	void *SDRAM_virtual;

	// Pointer to the DMA controller for the original image
   volatile unsigned int *mem_to_stream_dma = NULL;

   // Check inputs
   if (argc < 2){
      printf ("Usage: edgedetect <BMP filename>\n");
      return 0;
   }
   // Open input image file (24-bit bitmap image)
   if (read_bmp (argv[1], &header, &data, &width, &height) < 0){
      printf ("Failed to read BMP\n");
      return 0;
   }
	printf ("Image width = %d pixels, Image height = %d pixels\n", width, height);

   if ((fd = open_physical (fd)) == -1)   // Open /dev/mem
      return (-1);
   LW_virtual = map_physical (fd, LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
   SDRAM_virtual = map_physical (fd, SDRAM_BASE, SDRAM_SPAN);
	if ((LW_virtual == NULL) || (SDRAM_virtual == NULL))
      return (0);

	// Set up pointer to edge-detection DMA controller using address in the FPGA system
   mem_to_stream_dma = (volatile unsigned int *)(LW_virtual + 0x3100);
   *(mem_to_stream_dma+3) = 0; // Turn off edge-detection hardware DMA

    flip(data, width ,height);

   // Write the image to the memory used for video-out and edge-detection
   memcpy_consecutive_to_padded (data, SDRAM_virtual, width*height);

   free(header);
   free(data);

	unmap_physical (SDRAM_virtual, SDRAM_SPAN);	// release physical-memory mapping
	unmap_physical (LW_virtual, LW_BRIDGE_SPAN);
	close_physical (fd);	// close /dev/mem

   return 0;
}

