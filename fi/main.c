//
//  main.c
//  fi
//
//  Created by Alerk on 2/28/17.
//  Copyright © 2017 Alerk. All rights reserved.
//

#include <stdio.h>
#include "FFMpegInterface.h"

static void callbackHandle(int arg) {
    printf("\r %d %% - Completed", arg);
    return;
}

int main(int argc, char* argv[])
{
    if(argc<3)
    {
        fprintf(stderr, "Please call with enough param\n");
        fprintf(stderr, "inputFile1 inputFile2 outputFile");
        return -1;
    }
    char *inputFiles[] = {argv[1], argv[2]};
    
    // params: inputFiles, outputFile, width, height, codec = 0, callbackHandle;
    concatenateVideo(inputFiles, 2, argv[3], 640, 480, 0, &callbackHandle);
    
    printf("Program ended!\n");
    return 0;
}
