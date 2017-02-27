//
//  FFMpegInterface.h
//  ffmpeg_test
//
//  Created by Alerk on 2/27/17.
//  Copyright © 2017 Alerk. All rights reserved.
//

#ifndef FFMpegInterface_h
#define FFMpegInterface_h

class FFMpegInterface {
    FFMpegInterface();
public:
    
    virtual int concatenateVideo(char* inputFiles[], int numOfInputFiles, char* outputFile, int width, int height, int codec,
                                void* (callback)(void*)) = 0;
    virtual int overlayVideo(char* inputFile, char* effect, int width, int height, int codec,
                            void* (callback)(void*)) = 0;
};

FFMpegInterface* getFFMpegWorker();
void releaseWorker();



#endif /* FFMpegInterface_h */
