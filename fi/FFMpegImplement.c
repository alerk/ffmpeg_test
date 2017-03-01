//
//  FFMpegRaw.cpp
//  ffmpeg_test
//
//  Created by Alerk on 2/27/17.
//  Copyright © 2017 Alerk. All rights reserved.
//

#include <stdio.h>
#include "FFMpegInterface.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>


// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

AVFormatContext* openInputFile(const char* input_filename)
{
    AVFormatContext *fmt_ctx = NULL;
    int ret = 0;

    av_register_all();

    //--------------- open file -----------------------//
    ret = avformat_open_input(&fmt_ctx, input_filename, NULL, NULL);
    if (ret < 0) {
        printf("Could not open input\n");
        return NULL;
    }

    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        printf("Could not find stream information\n");
        return NULL;
    }

    av_dump_format(fmt_ctx, 0, input_filename, 0);

    return fmt_ctx;
}

void (*callback)(int) = NULL;

int concatenateVideo(char* inputFiles[], int numOfInputFiles,
    char* outputFile, int width, int height, int codec,
    void (*callbackFunc)(int)) {

    AVFormatContext *inFormatCtx = NULL;
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

    int isInitedOutput = 0;

    callback = callbackFunc;

    av_register_all();

    //--- write content ---//
    int64_t videoLastPts = 0;
    int64_t videoLastDts = 0;
    int64_t audioLastPts = 0;
    int64_t audioLastDts = 0;
    for(i=0;i<numOfInputFiles; i++) {
        //Open inputFiles
        printf("Openning %s for read\n", inputFiles[i]);
        int ret = avformat_open_input(&inFormatCtx, inputFiles[i], NULL, NULL);
        if(ret < 0) {
            printf("Couldn't open input %s\n", inputFiles[i]);
            goto end;
        }
        ret = avformat_find_stream_info(inFormatCtx, NULL);
        if(ret < 0) {
            printf("Couldn't find stream info\n");
            goto end;
        }
        av_dump_format(inFormatCtx, 0, inputFiles[i], 0);

        //if(inFormatCtx==NULL) {
        //    fprintf(stderr, "could not open file %s\n", inputFiles[i]);
            //return;
        //    goto end;
        //}
        //------//
        //int videoStreamIndex = -1;
        //int audioStreamIndex = -1;

        for (unsigned i = 0; i < inFormatCtx->nb_streams; i++)
        {
            if (inVideoStreamIndex==-1 &&
                inFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                inVideoStreamIndex = i;
                inVideoStream = inFormatCtx->streams[i];
                printf("inVideoStreamIndex = %d\n", i);
            }

            if (inAudioStreamIndex==-1 &&
                inFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                inAudioStreamIndex = i;
                inAudioStream = inFormatCtx->streams[i];
                printf("inAudioStreamIndex = %d\n", i);
            }

        }
        if (inVideoStream == NULL) {
            fprintf(stderr, "Didn't find any video stream\n");
            //return;
            goto end;
        }

        if(inAudioStream == NULL) {
            fprintf(stderr, "Didn't find any audio stream\n");
            //return;
            goto end;
        }

        //Output format relate to first input file
        if(!isInitedOutput) {
            isInitedOutput = 1;
            //Prepare output file: widthxheight, AV_PMT_PIX_YUV420P
            avformat_alloc_output_context2(&outFormatCtx, NULL, NULL, outputFile);
            printf("Init outAudioStream\n");
            outAudioStream = avformat_new_stream(outFormatCtx, NULL);
            {
                AVCodecParameters *c;
                c = outAudioStream->codecpar;
                //avcodec_parameters_copy(c, inAudioStream->codecpar);
                c->bit_rate = inAudioStream->codecpar->bit_rate;
                c->codec_id = inAudioStream->codecpar->codec_id;
                c->codec_type = inAudioStream->codecpar->codec_type;
                c->bits_per_coded_sample = inAudioStream->codecpar->bits_per_coded_sample;
                c->bits_per_raw_sample = inAudioStream->codecpar->bits_per_raw_sample;
                c->block_align = inAudioStream->codecpar->block_align;
                c->channels = inAudioStream->codecpar->channels;
                c->video_delay = inAudioStream->codecpar->video_delay;
                c->block_align = inAudioStream->codecpar->block_align;
                c->channel_layout = inAudioStream->codecpar->channel_layout;
                c->sample_rate = inAudioStream->codecpar->sample_rate;
                c->frame_size = inAudioStream->codecpar->frame_size;
                
                
                c->extradata = inAudioStream->codecpar->extradata;
                c->extradata_size = inAudioStream->codecpar->extradata_size;
                c->format = inAudioStream->codecpar->format;
                c->chroma_location = inAudioStream->codecpar->chroma_location;
                c->color_primaries = inAudioStream->codecpar->color_primaries;
            }
            
            printf("Init outVideoStream\n");
            outVideoStream = avformat_new_stream(outFormatCtx, NULL);
            {
                AVCodecParameters *c;
                c = outVideoStream->codecpar;
                avcodec_parameters_copy(c, inVideoStream->codecpar);
                // TODO: prepare outFormatCtx
                //c->codec_id = AV_CODEC_ID_YUV4;
                //c->width = width;
                //c->height = height;
                c->bit_rate = inVideoStream->codecpar->bit_rate;
                c->codec_id = inVideoStream->codecpar->codec_id;
                c->codec_type = inVideoStream->codecpar->codec_type;
                //c->sample_aspect_ratio.num = inVideoStream->time_base.num;
                //c->sample_aspect_ratio.den = inVideoStream->time_base.den;
                c->width = inVideoStream->codecpar->width;
                c->height = inVideoStream->codecpar->height;

                c->bits_per_coded_sample = inVideoStream->codecpar->bits_per_coded_sample;
                c->bits_per_raw_sample = inVideoStream->codecpar->bits_per_raw_sample;

                c->video_delay = inVideoStream->codecpar->video_delay;

                c->extradata = inVideoStream->codecpar->extradata;
                c->extradata_size = inVideoStream->codecpar->extradata_size;
                c->field_order = inVideoStream->codecpar->field_order;
                c->format = inVideoStream->codecpar->format;
                c->chroma_location = inVideoStream->codecpar->chroma_location;
                c->color_primaries = inVideoStream->codecpar->color_primaries;
                c->color_range = inVideoStream->codecpar->color_range;
                c->color_space = inVideoStream->codecpar->color_space;
                c->color_trc = inVideoStream->codecpar->color_trc;
                c->profile = inVideoStream->codecpar->profile;
                c->level = inVideoStream->codecpar->level;

            }

            

            printf("Open file %s\n", outputFile);
            avio_open(&outFormatCtx->pb, outputFile, AVIO_FLAG_WRITE);
            //--- write header ---//
            printf("Write header\n");
            avformat_write_header(outFormatCtx, NULL);
        }
        int64_t videoPts = 0;
        int64_t videoDts = 0;
        int64_t audioPts = 0;
        int64_t audioDts = 0;

        //convert to certain output format: YUV420P, widthxheight
        printf("Looping through frames of %s\n", inputFiles[i]);
        for(;;) {
            AVPacket inPacket;
            av_init_packet(&inPacket);
            inPacket.size = 0;
            inPacket.data = NULL;
            if (av_read_frame(inFormatCtx, &inPacket) < 0) {
                fprintf(stderr, "could not read frame of %s\n", inputFiles[i]);
                break; // << Exit this loop when can no longer read frame
            }

            inPacket.flags |= AV_PKT_FLAG_KEY;
            //Write to outputFile
            //TODO: Convert to output format ---->>> February 28th, 2017

            if (inPacket.stream_index == inVideoStreamIndex){
                videoPts = inPacket.pts;
                inPacket.pts = videoPts + videoLastPts;
                videoDts = inPacket.dts;
                inPacket.dts = videoDts + videoLastDts;
                inPacket.stream_index = inVideoStreamIndex;

                if (inPacket.pts != AV_NOPTS_VALUE)
                {
                    inPacket.pts = av_rescale_q(inPacket.pts,
                        inVideoStream->time_base, outVideoStream->time_base);
                }

                if (inPacket.dts != AV_NOPTS_VALUE)
                {
                    inPacket.dts = av_rescale_q(inPacket.dts,
                        inVideoStream->time_base, outVideoStream->time_base);
                }
            } else if (inPacket.stream_index == inAudioStreamIndex) {
                audioPts = inPacket.pts;
                inPacket.pts = audioPts + audioLastPts;
                audioDts = inPacket.dts;
                inPacket.dts = audioDts + audioLastDts;
                inPacket.stream_index = inAudioStreamIndex;
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
int overlayVideo(char* inputFile, char* effect,
    int width, int height, int codec, void (*callbackFunc)(int)) {

    callback = callbackFunc;
    if(callback!=NULL) {
        (*callback)(50);
    }
    return 1;
}
