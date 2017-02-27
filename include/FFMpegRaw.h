//
//  FFMpegRaw.h
//  ffmpeg_test
//
//  Created by Alerk on 2/27/17.
//  Copyright © 2017 Alerk. All rights reserved.
//

#ifndef FFMpegRaw_h
#define FFMpegRaw_h
#include "FFMpegInterface.h"

class FFMpegRaw : public FFMpegInterface {
    FFMpegRaw();
    static FFMpegRaw* instance;
    static void (*callback)(int);
public:
    
    static FFMpegRaw* getInstance();
    virtual ~FFMpegRaw(){};
    
    FFMpegInterface* getFFMpegWorker();
    void releaseWorker();
    int concatenateVideo(char* inputFiles[], int numOfInputFiles, char* outputFile, int width, int height, int codec,
                         void (*callback)(int));
    int overlayVideo(char* inputFile, char* effect, int width, int height, int codec,
                     void (*callback)(int));
    
    
};

#endif /* FFMpegRaw_h */
