//
//  FFmpegManager.h
//  FFmpegDemoTND
//
//  Created by Le Bac Duong on 2/21/17.
//  Copyright © 2017 Eastgate Software Ltd. All rights reserved.
//

#ifndef FFmpegManager_h
#define FFmpegManager_h

#include <stdio.h>
extern "C"
{
#include <libavformat/avformat.h>
}
AVFormatContext* openInputFile(const char* input_filename);

void concatenateVideos(char** inputFiles, char* outputFile, int numberFile = 2);
#endif /* FFmpegManager_h */
