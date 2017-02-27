//
//  FFMpegRaw.cpp
//  ffmpeg_test
//
//  Created by Alerk on 2/27/17.
//  Copyright © 2017 Alerk. All rights reserved.
//

#include <stdio.h>
#include "FFMpegRaw.h"
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

FFMpegRaw* FFMpegRaw::instance = NULL;
void (*FFMpegRaw::callback)(<#void *#>) = NULL;

FFMpegRaw* FFMpegRaw::getInstance() {
    if(FFMpegRaw::instance==NULL) {
        FFMpegRaw::instance = new FFMpegRaw();
    }
    return FFMpegRaw::instance;
}

FFMpegInterface* getFFMpegWorker() {
    return FFMpegRaw::getInstance();
}

void releaseWorker() {
    
}

int FFMpegRaw::concatenateVideo(char* inputFiles[], int numOfInputFiles, char* outputFile,
                                int width, int height, int codec, void* (callbackFunc)(void*)) {
    
    AVFormatContext *inFormatCtx;
    AVStream        *inVideoStream = NULL;
    AVStream        *inAudioStream = NULL;
    
    AVFormatContext *outFormatCtx;
    AVStream        *outVideoStream = NULL;
    AVStream        *outAudioStream = NULL;
    
    
    AVCodecParameters   *pCodecCtxOrig = NULL;
    AVCodecContext      *pCodecCtx = NULL;
    AVCodec             *pCodec = NULL;
    AVCodecContext	    *codecCtx = NULL;
    
    int inAudioStreamIndex = -1;
    int inVideoStreamIndex = -1;
    
    int i, j, k;
    
    callback = callbackFunc;
    
    //Prepare output file: widthxheight, AV_PMT_PIX_YUV420P
    avformat_alloc_output_context2(&o_fmt_ctx, NULL, NULL, outputFile);
    o_video_stream = avformat_new_stream(o_fmt_ctx, NULL);
    {
        AVCodecParameters *c;
        c = o_video_stream->codecpar;
        
    }
    avio_open(&o_fmt_ctx->pb, outputFile, AVIO_FLAG_WRITE);
    //--- write header ---//
    avformat_write_header(o_fmt_ctx, NULL);
    
    
    //--- write content ---//
    int64_t videoLastPts = 0;
    int64_t videoLastDts = 0;
    int64_t audioLastPts = 0;
    int64_t audioLastDts = 0;
    for(i=0;i<numOfInputFiles; i++) {
        //Open inputFiles
        inFormatCtx = openInputFile(inputFiles[i]);
        if(inFormatCtx==NULL) {
            fprintf(stderr, "could not open file %s\n", inputFiles[i]);
            //return;
            goto end;
        }
        int videoStreamIndex = -1;
        int audioStreamIndex = -1;
        
        for (unsigned i = 0; i < inFormatCtx->nb_streams; i++)
        {
            if (inFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                videoStreamIndex = i;
                inVideoStream = inFormatCtx->streams[i];
            }
            else if (inFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                audioStreamIndex = i;
                inAudioStream = inFormatCtx->streams[i];
            }
            else
            {
                printf("unknown\n");
            }
        }
        if (inVideoStream == NULL)
        {
            fprintf(stderr, "Didn't find any video stream\n");
            //return;
            goto end;
        }
        int64_t videoPts = 0;
        int64_t videoDts = 0;
        int64_t audioPts = 0;
        int64_t audioDts = 0;
        
        //convert to certain output format: YUV420P, widthxheight
        for(;;) {
            AVPacket inPacket;
            av_init_packet(&inPacket);
            inPacket.size = 0;
            inPacket.data = NULL;
            if (av_read_frame(inFormatCtx, &inPacket) < 0) {
                fprintf(stderr, "could not read frame\n");
                break; // << Exit this loop when can no longer read frame
            }
            
            inPacket.flags |= AV_PKT_FLAG_KEY;
            //Write to outputFile
            //TODO: Convert to output format ---->>> February 28th, 2017
            
            if (inPacket.stream_index == videoStreamIndex){
                
            } else if (inPacket.stream_index == audioStreamIndex) {
                
            } else {
                printf("unknown\n");
            }
            
            av_interleaved_write_frame(outFormatCtx, &inPacket);
            //End of TODO <<<----
            
            av_packet_unref(&inPacket);
        }
        // set index stream
        videoLastPts += videoPts;
        videoLastDts += videoDts;
        audioLastPts += audioPts;
        audioLastDts += audioDts;
        
        if (inFormatCtx != NULL) {
            if (inFormatCtx->pb) {
                av_freep(&inFormatCtx->pb->buffer);
                av_freep(&inFormatCtx->pb);
            }
            avformat_close_input(&inFormatCtx);
        }
        
        if(callback!=NULL) {
            (*callback)((i+1)*100/numOfInputFiles);
        }
    }
    
    //------------ write trailer --------------//
    av_write_trailer(outFormatCtx);
    
    // free
    for (int i = 0; i < outFormatCtx->nb_streams; i++)
    {
        avcodec_parameters_free(&outFormatCtx->streams[i]->codecpar);
        av_freep(&outFormatCtx->streams[i]);
    }
    avio_close(outFormatCtx->pb);
    av_free(outFormatCtx);
    
end:
    /* note: the internal buffer could have changed, and be != avio_ctx_buffer */
    if (inFormatCtx != NULL) {
        if (inFormatCtx->pb) {
            av_freep(&inFormatCtx->pb->buffer);
            av_freep(&inFormatCtx->pb);
        }
        avformat_close_input(&inFormatCtx);
    }
    
    return 1;
    
}
int FFMpegRaw::overlayVideo(char* inputFile, char* effect, int width, int height, int codec,
                            void* (callbackFunc)(void*)) {
    callback = callbackFunc;
    if(callback!=NULL) {
        (*callback)(50);
    }
    return 1;
}
