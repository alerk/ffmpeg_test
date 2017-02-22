//
//  FFmpegManager.c
//  FFmpegDemoTND
//
//  Created by Le Bac Duong on 2/21/17.
//  Copyright © 2017 Eastgate Software Ltd. All rights reserved.
//

#include "FFMpegManager.h"
extern "C"
{
#include <libavutil/file.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

/////

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
        //goto end;
    }
    
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        printf("Could not find stream information\n");
		return NULL;
       // goto end;
    }
    
    av_dump_format(fmt_ctx, 0, input_filename, 0);
    
    //avformat_close_input(&fmt_ctx);
    ///* note: the internal buffer could have changed, and be != avio_ctx_buffer */
    //if (avio_ctx) {
    //	av_freep(&avio_ctx->buffer);
    //	av_freep(&avio_ctx);
    //}
    //av_file_unmap(buffer, buffer_size);
    
    //if (ret < 0) {
    //	cout << "Error occurred" << endl;
    //	return fmt_ctx;
    //}
    //av_file_unmap(buffer, buffer_size);
    return fmt_ctx;
}

void concatenateVideos(char** inputFiles, char*  outputFile, int numberFile)
{
    AVFormatContext *i_fmt_ctx;
    AVStream        *i_video_stream = NULL;
    AVStream        *i_audio_stream = NULL;
    
    AVFormatContext *o_fmt_ctx;
    AVStream        *o_video_stream = NULL;
    AVStream        *o_audio_stream = NULL;
    
    
    AVCodecParameters   *pCodecCtxOrig = NULL;
    AVCodecContext      *pCodecCtx = NULL;
    AVCodec             *pCodec = NULL;
    AVCodecContext	    *codecCtx = NULL;
    
    int i_audioStreamIndex = -1;
    int i_videoStreamIndex = -1;
    
    //NSString *path1 = [[NSBundle mainBundle] pathForResource:@"test1" ofType:@"mov"];
    const char *inputPath1 = inputFiles[0];//"test1.mov";//[path1 UTF8String];
    i_fmt_ctx = openInputFile(inputPath1);
    if (i_fmt_ctx == NULL)
    {
        printf("could not open output file\n");
        return;
    }
    
    int indexStream = 0;
    
    for (unsigned i = 0; i < i_fmt_ctx->nb_streams; i++)
    {
        
        if (i_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            i_videoStreamIndex = i;
            i_video_stream = i_fmt_ctx->streams[i];
        }
        else if (i_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            i_audioStreamIndex = i;
            i_audio_stream = i_fmt_ctx->streams[i];
        }
        else
        {
            printf("unknown\n");
        }
    }
    
    if (i_video_stream == NULL)
    {
        printf("didn't find any video stream\n");
		return;
        //goto end;
    }
    
    avformat_alloc_output_context2(&o_fmt_ctx, NULL, NULL, outputFile);
    
    
    // Xoá
    if (i_video_stream <= i_audio_stream)
    {
        // add stream video
        if (i_video_stream != NULL)
        {
            o_video_stream = avformat_new_stream(o_fmt_ctx, NULL);
            {
                AVCodecParameters *c;
                c = o_video_stream->codecpar;
                c->bit_rate = i_video_stream->codecpar->bit_rate;
                c->codec_id = i_video_stream->codecpar->codec_id;
                c->codec_type = i_video_stream->codecpar->codec_type;
                c->sample_aspect_ratio.num = i_video_stream->time_base.num;
                c->sample_aspect_ratio.den = i_video_stream->time_base.den;
                c->width = i_video_stream->codecpar->width;
                c->height = i_video_stream->codecpar->height;
                
                c->bits_per_coded_sample = i_video_stream->codecpar->bits_per_coded_sample;
                c->bits_per_raw_sample = i_video_stream->codecpar->bits_per_raw_sample;
                
                c->video_delay = i_video_stream->codecpar->video_delay;
                
                c->extradata = i_video_stream->codecpar->extradata;
                c->extradata_size = i_video_stream->codecpar->extradata_size;
                c->field_order = i_video_stream->codecpar->field_order;
                c->format = i_video_stream->codecpar->format;
                c->chroma_location = i_video_stream->codecpar->chroma_location;
                c->color_primaries = i_video_stream->codecpar->color_primaries;
                c->color_range = i_video_stream->codecpar->color_range;
                c->color_space = i_video_stream->codecpar->color_space;
                c->color_trc = i_video_stream->codecpar->color_trc;
                c->profile = i_video_stream->codecpar->profile;
                c->level = i_video_stream->codecpar->level;
            }
        }
        
        
        // add stream audio
        if (i_audio_stream != NULL)
        {
            o_audio_stream = avformat_new_stream(o_fmt_ctx, NULL);
            {
                AVCodecParameters *c;
                c = o_audio_stream->codecpar;
                c->bit_rate = i_audio_stream->codecpar->bit_rate;
                c->codec_id = i_audio_stream->codecpar->codec_id;
                c->codec_type = i_audio_stream->codecpar->codec_type;
                c->bits_per_coded_sample = i_audio_stream->codecpar->bits_per_coded_sample;
                c->bits_per_raw_sample = i_audio_stream->codecpar->bits_per_raw_sample;
                c->block_align = i_audio_stream->codecpar->block_align;
                c->channels = i_audio_stream->codecpar->channels;
                c->video_delay = i_audio_stream->codecpar->video_delay;
                c->block_align = i_audio_stream->codecpar->block_align;
                c->channel_layout = i_audio_stream->codecpar->channel_layout;
                c->sample_rate = i_audio_stream->codecpar->sample_rate;
                c->frame_size = i_audio_stream->codecpar->frame_size;
                
                
                c->extradata = i_audio_stream->codecpar->extradata;
                c->extradata_size = i_audio_stream->codecpar->extradata_size;
                c->format = i_audio_stream->codecpar->format;
                c->chroma_location = i_audio_stream->codecpar->chroma_location;
                c->color_primaries = i_audio_stream->codecpar->color_primaries;
            }
        }
    }
    else
    {
        // add stream audio
        if (i_audio_stream != NULL)
        {
            o_audio_stream = avformat_new_stream(o_fmt_ctx, NULL);
            {
                AVCodecParameters *c;
                c = o_audio_stream->codecpar;
                c->bit_rate = i_audio_stream->codecpar->bit_rate;
                c->codec_id = i_audio_stream->codecpar->codec_id;
                c->codec_type = i_audio_stream->codecpar->codec_type;
                c->bits_per_coded_sample = i_audio_stream->codecpar->bits_per_coded_sample;
                c->bits_per_raw_sample = i_audio_stream->codecpar->bits_per_raw_sample;
                c->block_align = i_audio_stream->codecpar->block_align;
                c->channels = i_audio_stream->codecpar->channels;
                c->video_delay = i_audio_stream->codecpar->video_delay;
                c->block_align = i_audio_stream->codecpar->block_align;
                c->channel_layout = i_audio_stream->codecpar->channel_layout;
                c->sample_rate = i_audio_stream->codecpar->sample_rate;
                c->frame_size = i_audio_stream->codecpar->frame_size;
                
                
                c->extradata = i_audio_stream->codecpar->extradata;
                c->extradata_size = i_audio_stream->codecpar->extradata_size;
                c->format = i_audio_stream->codecpar->format;
                c->chroma_location = i_audio_stream->codecpar->chroma_location;
                c->color_primaries = i_audio_stream->codecpar->color_primaries;
            }
        }
        
        // add stream video
        if (i_video_stream != NULL)
        {
            o_video_stream = avformat_new_stream(o_fmt_ctx, NULL);
            {
                AVCodecParameters *c;
                c = o_video_stream->codecpar;
                c->bit_rate = i_video_stream->codecpar->bit_rate;
                c->codec_id = i_video_stream->codecpar->codec_id;
                c->codec_type = i_video_stream->codecpar->codec_type;
                c->sample_aspect_ratio.num = i_video_stream->time_base.num;
                c->sample_aspect_ratio.den = i_video_stream->time_base.den;
                c->width = i_video_stream->codecpar->width;
                c->height = i_video_stream->codecpar->height;
                
                c->bits_per_coded_sample = i_video_stream->codecpar->bits_per_coded_sample;
                c->bits_per_raw_sample = i_video_stream->codecpar->bits_per_raw_sample;
                
                c->video_delay = i_video_stream->codecpar->video_delay;
                
                c->extradata = i_video_stream->codecpar->extradata;
                c->extradata_size = i_video_stream->codecpar->extradata_size;
                c->field_order = i_video_stream->codecpar->field_order;
                c->format = i_video_stream->codecpar->format;
                c->chroma_location = i_video_stream->codecpar->chroma_location;
                c->color_primaries = i_video_stream->codecpar->color_primaries;
                c->color_range = i_video_stream->codecpar->color_range;
                c->color_space = i_video_stream->codecpar->color_space;
                c->color_trc = i_video_stream->codecpar->color_trc;
                c->profile = i_video_stream->codecpar->profile;
                c->level = i_video_stream->codecpar->level;
            }
        }
    }
    
    avio_open(&o_fmt_ctx->pb, outputFile, AVIO_FLAG_WRITE);
    //////////////////////////////////////////// write header////////////////////////////////////////////////////////////////
    avformat_write_header(o_fmt_ctx, NULL);
    
    
    //////////////////////////////////////////// write content///////////////////////////////////////////////////////////////
    int64_t video_last_pts = 0;
    int64_t video_last_dts = 0;
    int64_t audio_last_pts = 0;
    int64_t audio_last_dts = 0;
    for (int i = 0; i < numberFile; i++)
    {
        i_fmt_ctx = NULL;
        if (i == 0)
        {
            //NSString *path1 = [[NSBundle mainBundle] pathForResource:@"test1" ofType:@"mov"];
            const char *inputPath1 = inputFiles[0];//"test1.mov";//[path1 UTF8String];
            i_fmt_ctx = openInputFile(inputPath1);
        }
        else
        {
            //NSString *path2 = [[NSBundle mainBundle] pathForResource:@"test2" ofType:@"mov"];
            const char *inputPath2 = inputFiles[1];//"test2.mov";//[path2 UTF8String];
            i_fmt_ctx = openInputFile(inputPath2);
        }
        //i_fmt_ctx = openInputFile(inputFiles[i]);
        if (i_fmt_ctx == NULL)
        {
            printf("could not open input file\n");
			return;
            //goto end;
        }
        
        int videoStreamIndex = -1;
        int audioStreamIndex = -1;
        i_video_stream = NULL;
        i_audio_stream = NULL;
        /* we only use first video stream of each input file */
        for (unsigned i = 0; i < i_fmt_ctx->nb_streams; i++)
        {
            if (i_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                videoStreamIndex = i;
                i_video_stream = i_fmt_ctx->streams[i];
            }
            else if (i_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                audioStreamIndex = i;
                i_audio_stream = i_fmt_ctx->streams[i];
            }
            else
            {
                printf("unknowne\n");
            }
        }
        
        
        int64_t videoPts = 0;
        int64_t videoDts = 0;
        int64_t audioPts = 0;
        int64_t audioDts = 0;
        while (1)
        {
            AVPacket i_pkt;
            av_init_packet(&i_pkt);
            i_pkt.size = 0;
            i_pkt.data = NULL;
            if (av_read_frame(i_fmt_ctx, &i_pkt) < 0)
            {
                printf("could not read frame\n");
                break;
            }
            
            /*
             * pts and dts should increase monotonically
             * pts should be >= dts
             */
            
            i_pkt.flags |= AV_PKT_FLAG_KEY;
            
            // set index stream
            if (i_pkt.stream_index == videoStreamIndex)
            {
                videoPts = i_pkt.pts;
                i_pkt.pts = videoPts + video_last_pts;
                videoDts = i_pkt.dts;
                i_pkt.dts = videoDts + video_last_dts;
                i_pkt.stream_index = i_videoStreamIndex;
                
                if (i_pkt.pts != AV_NOPTS_VALUE)
                {
                    i_pkt.pts = av_rescale_q(i_pkt.pts, o_video_stream->codecpar->sample_aspect_ratio, o_video_stream->time_base);
                }
                
                if (i_pkt.dts != AV_NOPTS_VALUE)
                {
                    i_pkt.dts = av_rescale_q(i_pkt.dts, o_video_stream->codecpar->sample_aspect_ratio, o_video_stream->time_base);
                }
            }
            else if (i_pkt.stream_index == audioStreamIndex)
            {
                audioPts = i_pkt.pts;
                i_pkt.pts = audioPts + audio_last_pts;
                audioDts = i_pkt.dts;
                i_pkt.dts = audioDts + audio_last_dts;
                i_pkt.stream_index = i_audioStreamIndex;
            }
            else
            {
                printf("unknown\n");
            }
            
            av_interleaved_write_frame(o_fmt_ctx, &i_pkt);
            av_packet_unref(&i_pkt);
        }
        
        video_last_pts += videoPts;
        video_last_dts += videoDts;
        audio_last_pts += audioPts;
        audio_last_dts += audioDts;
        
        
        if (i_fmt_ctx != NULL) {
            if (i_fmt_ctx->pb) {
                av_freep(&i_fmt_ctx->pb->buffer);
                av_freep(&i_fmt_ctx->pb);
            }
            avformat_close_input(&i_fmt_ctx);
        }
    }
    
    //------------ write trailer --------------//
    av_write_trailer(o_fmt_ctx);
    
    
    // free
    for (int i = 0; i < o_fmt_ctx->nb_streams; i++)
    {
        avcodec_parameters_free(&o_fmt_ctx->streams[i]->codecpar);
        av_freep(&o_fmt_ctx->streams[i]);
    }
    avio_close(o_fmt_ctx->pb);
    av_free(o_fmt_ctx);
    
//end:
    /* note: the internal buffer could have changed, and be != avio_ctx_buffer */
    if (i_fmt_ctx != NULL) {
        if (i_fmt_ctx->pb) {
            av_freep(&i_fmt_ctx->pb->buffer);
            av_freep(&i_fmt_ctx->pb);
        }
        avformat_close_input(&i_fmt_ctx);
    }
}
