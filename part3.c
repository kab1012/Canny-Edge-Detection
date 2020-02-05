#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "address_map_arm.h"
#include "physical.h"

void *LW_virtual;
void *SDRAM_virtual;

struct pixel {
   unsigned char b;
   unsigned char g;
   unsigned char r;
};

// Read BMP file and extract the pixel values (store in data) and header (store in header)
// data is data[0] = BLUE, data[1] = GREEN, data[2] = RED, etc...
int read_bmp(char* filename, unsigned char** header, struct pixel** data) {
   FILE* file = fopen(filename, "rb");

   if (!file) return -1;

   // read the 54-byte header
   unsigned char * header_ = malloc(54*sizeof(unsigned char));
   fread(header_, sizeof(unsigned char), 54, file);

   // get height and width of image
   int width = *(int*)&header_[18];
   int height = *(int*)&header_[22];

   // Read in the image
   int size = width * height;
   struct pixel * data_ = malloc(size*sizeof(struct pixel));
   fread(data_, sizeof(struct pixel), size, file); // read the rest of the data at once
   fclose(file);

   *header = header_;
   *data = data_;

   return 0;
}

void write_bmp(char* filename, unsigned char* header, struct pixel* data) {
   FILE* file = fopen(filename, "wb");

   // write the 54-byte header
   fwrite(header, sizeof(unsigned char), 54, file);

   // extract image height and width from header
   int width = *(int*)&header[18];
   int height = *(int*)&header[22];

   int size = width * height;
   fwrite(data, sizeof(struct pixel), size, file); // read the rest of the data at once
   fclose(file);
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
void memcpy_consecutive_to_padded(void * from, volatile unsigned int * to, int pixels){
   int i;
   for (i=0;i<pixels;i++){
      *(to+i) = (((unsigned int)*((char*)(from+i*3+0)))&0xff) |
         ((((unsigned int)*((char*)(from+i*3+1)))<<8)&0xff00) |
         ((((unsigned int)*((char*)(from+i*3+2)))<<16)&0xff0000);
   }
}
void memcpy_padded_to_consecutive(volatile unsigned int * from, void * to, int pixels){
   int i;
   for (i=0;i<pixels;i++){
      *((char*)(to+i*3+0)) = (*(from+i));
      *((char*)(to+i*3+1)) = (*(from+i))>>8;
      *((char*)(to+i*3+2)) = (*(from+i))>>16;
   }
}

int main(int argc, char *argv[]){
   struct pixel * data;
   unsigned char * header;
   int fd = -1;
   time_t start, end;
   volatile unsigned int *mem_to_stream_dma=NULL;
   volatile unsigned int *stream_to_mem_dma=NULL;
   volatile unsigned int *mem_to_stream_dma_buffer = NULL;
   volatile unsigned int *stream_to_mem_dma_buffer = NULL;

   // Check inputs
   if (argc < 2){
      printf("Usage: edgedetect <BMP filename>\n");
      return 0;
   }

   // Open input image file (24-bit bitmap image)
   if (read_bmp(argv[1], &header, &data) < 0){
      printf("Failed to read BMP\n");
      return 0;
   }

   // Image dimensions
   int width = *(int*)&header[18];
   int height = *(int*)&header[22];

   // Create access to the FPGA light-weight bridge and
   // to sdram region (through HPS-FPGA non-lightweight bridge)
   if ((fd = open_physical (fd)) == -1)   // Open /dev/mem
      return (-1);
   LW_virtual = map_physical (fd, LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
   SDRAM_virtual = map_physical (fd, SDRAM_BASE, SDRAM_SPAN);
	if ((LW_virtual == NULL) || (SDRAM_virtual == NULL))
      return (-1);

   mem_to_stream_dma = (volatile unsigned int *)(LW_virtual + 0x3100);
   stream_to_mem_dma = (volatile unsigned int *)(LW_virtual + 0x3120);
   mem_to_stream_dma_buffer = (volatile unsigned int *)(SDRAM_virtual);
   stream_to_mem_dma_buffer = (volatile unsigned int *)(SDRAM_virtual + 0x2000000);

   /********************************************
   *        IMAGE PROCESSING STAGES        *
   ********************************************/

    flip(data, width ,height);

   // Start measuring time
   start = clock();

   // Turn off DMAs
   *(mem_to_stream_dma+3) = 0;
   *(stream_to_mem_dma+3) = 0;

   // Write the image to the mem-to-stream buffer
   memcpy_consecutive_to_padded((void*)data, mem_to_stream_dma_buffer, width*height );

   // Turn on the DMAs
   *(stream_to_mem_dma+3) = 4;
   *(mem_to_stream_dma+3) = 4;

   // Write to the buffer reg to start swap
   *(mem_to_stream_dma) = 1;
   *(stream_to_mem_dma) = 1;

   // wait until the swap is complete
   while((*(mem_to_stream_dma+3))&0x1 || (*(stream_to_mem_dma+3))&0x1 ){}

   // Turn off DMAs
   *(mem_to_stream_dma+3) = 0;
   *(stream_to_mem_dma+3) = 0;

   // Copy the edge detected image from the stream-to-mem buffer
   memcpy_padded_to_consecutive(stream_to_mem_dma_buffer, (void*)data, width*height );

    // Write the image to the memory used for video-out and edge-detection
   memcpy_consecutive_to_padded (data, SDRAM_virtual, width*height);

   end = clock();

   printf("TIME ELAPSED: %.0f ms\n", ((double) (end - start)) * 1000 / CLOCKS_PER_SEC);

   // Turn off DMAs
   *(mem_to_stream_dma+3) = 0;
   *(stream_to_mem_dma+3) = 0;

    flip(data, width ,height);
   // Write out the edge detected image as a bmp
   write_bmp("edges.bmp", header, data);




   free(header);
   free(data);

	unmap_physical (LW_virtual, LW_BRIDGE_SPAN);	// release the physical-memory mapping
	unmap_physical (SDRAM_virtual, SDRAM_SPAN);	// release the physical-memory mapping
	close_physical (fd);	// close /dev/mem

   return 0;
}

