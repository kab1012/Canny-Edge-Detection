# Canny Edge Detection

This mplement two versions of an image processing application that detects theedges of an image.  
The first version will run entirely on the CPU. The second version of the application will make use of hardware acceleration, 
by offloading most of the image processing operations to a hardware accelerator implemented insidean FPGA. 
We will assume that image files are represented in thebitmap(BMP) file format, using24-bit true color.

//part 1 
The code present in part1.c is the implementation of the five stages of the canny edge detector and run your code on the DE1-SoC Computer. 

//part 2
The code present in part2.c is used as this system include an edge-detection mechanism that isimplemented as a hardware circuit in the FPGA device.

//part3 

For this part we will extend our program from Part II so that it makes use of the hardware edge-detectionmechanism.  
You will need to access the programming registers of the DMA controllers.  
The register at the Base address is called the Buffer register, and the one at address_Base + 4is the Back buffer register. 
Each of these registers stores the address of a memory buffer. 
The Buffer register stores the address of the buffer that is currently being used by the DMA controller.
