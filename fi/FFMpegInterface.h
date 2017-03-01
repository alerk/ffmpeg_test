//
//  FFMpegInterface.h
//  ffmpeg_test
//
//  Created by Alerk on 2/27/17.
//  Copyright © 2017 Alerk. All rights reserved.
//

#ifndef FFMpegInterface_h
#define FFMpegInterface_h

int concatenateVideo(char* inputFiles[], int numOfInputFiles, char* outputFile, int width, int height, int codec,
                                void (*callback)(int));
int overlayVideo(char* inputFile, char* effect, int width, int height, int codec,
                            void (*callback)(int));


#endif /* FFMpegInterface_h */
