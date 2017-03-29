#include <libavcodec/avcodec.h>

#include <libavformat/avformat.h>

#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>

#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>

#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#ifdef __MINGW32__
#undef main /* Prevents SDL from overriding main() */
#endif

#include <stdio.h>
#include <assert.h>

#include "ffmpeg_function.h"

// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000

#define DEFAULT_VIDEO_OUT_CODEC             AV_CODEC_ID_H264
#define DEFAULT_VIDEO_OUT_PIX_FORMAT        AV_PIX_FMT_YUV420P
#define DEFAULT_VIDEO_OUT_FRAME_RATE        30
#define DEFAULT_VIDEO_OUT_WIDTH             640
#define DEFAULT_VIDEO_OUT_HEIGHT            480
#define DEFAULT_VIDEO_OUT_RATE              3200000

#define DEFAULT_AUDIO_IN_SAMPLE_RATE       44100
#define DEFAULT_AUDIO_IN_SAMPLE_FMT        AV_SAMPLE_FMT_S16P
#define DEFAULT_AUDIO_IN_CHANNELS          2
#define DEFAULT_AUDIO_IN_CHANNEL_LAYOUT    AV_CH_LAYOUT_STEREO

#define DEFAULT_AUDIO_OUT_SAMPLE_RATE       44100
#define DEFAULT_AUDIO_OUT_SAMPLE_FMT        AV_SAMPLE_FMT_S16
#define DEFAULT_AUDIO_OUT_CHANNELS          2
#define DEFAULT_AUDIO_OUT_CHANNEL_LAYOUT    AV_CH_LAYOUT_STEREO
#define DEFAULT_AUDIO_OUT_CODEC             AV_CODEC_ID_AAC
#define AUDIO_OUT_EXPANSION_BUFFER          1024

#define MAX_COLOR_DIFF  450 //150 for each channel

//#define DEBUG_DISPLAY_SCREEN

// static AVFormatContext *ifmt_ctx;
// static AVFormatContext *ofmt_ctx;
typedef struct FilteringContext {
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;
} FilteringContext;
static FilteringContext *filter_ctx;

// static int64_t video_last_pts= 0, video_last_dts= 0, audio_last_pts= 0, audio_last_dts= 0;
// static int64_t video_pts= 0, video_dts= 0, audio_pts= 0, audio_dts= 0;
//
// unsigned int displayWidth = 0, displayHeight = 0;
// int o_video_st_id = -1, o_audio_st_id = -1;
// int i_video_st_id = -1, i_audio_st_id = -1;

static int open_input_file(const char *filename, AVFormatContext **ifmt_ctx_ptr,
     int *i_audio_st_id, int *i_video_st_id)
{
    int ret;
    unsigned int i;

    AVFormatContext *ifmt_ctx;

    if ((ret = avformat_open_input(ifmt_ctx_ptr, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
    ifmt_ctx = *ifmt_ctx_ptr;

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *stream;
        AVCodecContext *codec_ctx;
        stream = ifmt_ctx->streams[i];
        codec_ctx = stream->codec;
        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
            || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            /* Open decoder */
            ret = avcodec_open2(codec_ctx,
                                avcodec_find_decoder(codec_ctx->codec_id), NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return ret;
            }
            if(codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
                *i_audio_st_id = i;
            } else {
                *i_video_st_id = i;
            }
        }
    }

    av_dump_format(ifmt_ctx, 0, filename, 0);
    return 1;
}

static int open_output_file(const char *filename, AVFormatContext *ifmt_ctx,
    AVFormatContext **ofmt_ctx_ptr, int *o_audio_st_id, int *o_video_st_id)
{
    AVStream *out_stream;
    AVStream *in_stream;
    AVOutputFormat *fmt;
    AVCodecContext *dec_ctx, *enc_ctx;
    AVCodec *encoder;
    int ret;
    unsigned int i;

    AVFormatContext *ofmt_ctx;
    fmt = NULL;
    fmt = av_guess_format(NULL, filename, NULL);
    if(!fmt)
    {
        printf("Could not deduce output format from file extension: using MOV.\n");
        fmt = av_guess_format("mov", NULL, NULL);
    }
    if(!fmt)
    {
        fprintf(stderr, "Couldn't find output format for %s\n", filename);
        exit(1);
    }
    avformat_alloc_output_context2(ofmt_ctx_ptr, NULL, NULL, filename);
    ofmt_ctx = *ofmt_ctx_ptr;
    if (!ofmt_ctx)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }
    ofmt_ctx->oformat = fmt;

    for (i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        in_stream = ifmt_ctx->streams[i];
        dec_ctx = in_stream->codec;

        if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
            || dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            /* in this example, we choose transcoding to same codec */
            encoder = avcodec_find_encoder(dec_ctx->codec_id);
            if (!encoder)
            {
                av_log(NULL, AV_LOG_FATAL, "Neccessary encoder not found\n");
                return AVERROR_INVALIDDATA;
            }
            //@new_stream
            //out_stream = avformat_new_stream(ofmt_ctx, NULL);
            out_stream = avformat_new_stream(ofmt_ctx, encoder);
            if (!out_stream)
            {
                av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
                return AVERROR_UNKNOWN;
            }
            enc_ctx = out_stream->codec;
            enc_ctx->codec_type = dec_ctx->codec_type;
            /* In this example, we transcode to same properties (picture size,
             * sample rate etc.). These properties can be changed for output
             * streams easily using filters */
            if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                *o_video_st_id = i;
    //            enc_ctx->qmin = 3;
    //            enc_ctx->qmax = 30;
    //            enc_ctx->qcompress = 1;

                enc_ctx->height = dec_ctx->height;
                enc_ctx->width  = dec_ctx->width;
                enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
                /* take first format from list of supported formats */
                enc_ctx->pix_fmt        = DEFAULT_VIDEO_OUT_PIX_FORMAT;
                /* video time_base can be set to whatever is handy and supported by encoder */
                enc_ctx->codec_id       = DEFAULT_VIDEO_OUT_CODEC;
                enc_ctx->framerate      = (AVRational){1, DEFAULT_VIDEO_OUT_FRAME_RATE};
                enc_ctx->bit_rate       = DEFAULT_VIDEO_OUT_RATE;
                enc_ctx->max_b_frames   = dec_ctx->max_b_frames;
                enc_ctx->gop_size       = dec_ctx->gop_size;
                //enc_ctx->auto_code_id = 0;
                //enc_ctx->stream_index = -1;

                enc_ctx->time_base.den = DEFAULT_VIDEO_OUT_FRAME_RATE;//dec_ctx->time_base.den;
                enc_ctx->time_base.num = 1;//dec_ctx->time_base.num;

                printf("[VIDEO #%d] gop_size = %d; den = %d; num = %d\n",
                    i, enc_ctx->gop_size, enc_ctx->time_base.den,
                    enc_ctx->time_base.num);

                out_stream->time_base.den = DEFAULT_VIDEO_OUT_FRAME_RATE;//dec_ctx->time_base.den;//frame rate
                out_stream->time_base.num = 1;//dec_ctx->time_base.num;
                if(enc_ctx->codec_id == AV_CODEC_ID_H264)
                {
                    enc_ctx->thread_count = 8;
                    av_opt_set(enc_ctx->priv_data, "preset", "veryfast", 0);
                    av_opt_set(enc_ctx->priv_data, "profile", "high", 0);
                    av_opt_set(enc_ctx->priv_data, "level", "4.1", 0);
                }
                if(enc_ctx->codec_id == AV_CODEC_ID_MPEG2VIDEO)
                {
                    enc_ctx->max_b_frames = 2;
                }
                if(enc_ctx->codec_id == AV_CODEC_ID_MPEG1VIDEO)
                {
                    enc_ctx->mb_decision = 2;
                }
                if(ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                {
                    enc_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
                }
            }
            else
            {
                *o_audio_st_id = i;
                //dec_ctx = ifmt_ctx->streams[i_audio_st_id]->codec;
                enc_ctx->codec_id = dec_ctx->codec_id;

                /* put sample parameters */
                //enc_ctx->bit_rate = dec_ctx->bit_rate;
                enc_ctx->sample_rate = dec_ctx->sample_rate;
                //enc_ctx->frame_size = DEFdec_ctx->frame_size;
                //enc_ctx->channels = 2;

                enc_ctx->channel_layout = DEFAULT_AUDIO_OUT_CHANNEL_LAYOUT;
                // dec_ctx->channel_layout;
                enc_ctx->channels = DEFAULT_AUDIO_OUT_CHANNELS;
                /* take first format from list of supported formats */
                enc_ctx->sample_fmt = DEFAULT_AUDIO_OUT_SAMPLE_FMT;// dec_ctx->sample_fmt;

                enc_ctx->time_base.den = enc_ctx->sample_rate;// dec_ctx->time_base.den;
                enc_ctx->time_base.num = 1;//dec_ctx->time_base.num;

                out_stream->time_base.den = enc_ctx->sample_rate;//dec_ctx->time_base.den;
                out_stream->time_base.num = 1;//dec_ctx->time_base.num;
                printf("[AUDIO] dec_ctx->frame_size: %d; sample_rate: %d; channels: %d;channel_layout: %d\n",
                       dec_ctx->frame_size, dec_ctx->sample_rate, dec_ctx->channels, dec_ctx->channel_layout);
            }

            /* Third parameter can be used to pass settings to encoder */
            ret = avcodec_open2(enc_ctx, encoder, NULL);
            if (ret < 0)
            {
                av_log(NULL, AV_LOG_ERROR,
                       "Cannot open %s encoder for stream #%u\n",
                       (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)?"video":"audio",
                       i);
                return ret;
            }
        }
        else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN)
        {
            av_log(NULL, AV_LOG_FATAL,
                   "Elementary stream #%d is of unknown type, cannot proceed\n", i);
            return AVERROR_INVALIDDATA;
        }
        else
        {
            /* if this stream must be remuxed */
            // ret = avcodec_copy_context(ofmt_ctx->streams[i]->codec,
            //                            ifmt_ctx->streams[i]->codec);
            // if (ret < 0) {
            //     av_log(NULL, AV_LOG_ERROR, "Copying stream context failed\n");
            //     return ret;
            // }
        }

        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        {
            enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
    }
    printf("Dump output format\n");
    av_dump_format(ofmt_ctx, 0, filename, 1);

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", filename);
            return ret;
        }
    }

    /* init muxer, write output file header */
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }

    return 0;
}

static int add_audio_stream(AVFormatContext *ofmt_ctx, AVCodecContext *dec_ctx,
    int *o_audio_st_id, int64_t *audio_last_pts, int last_sample_rate)
{
    int ret = -1;
    //Create new audio output stream
    AVCodec *encoder = avcodec_find_encoder(dec_ctx->codec_id);
    if (!encoder)
    {
        av_log(NULL, AV_LOG_FATAL, "Neccessary encoder not found\n");
        return AVERROR_INVALIDDATA;
    }
    AVStream *out_stream = avformat_new_stream(ofmt_ctx, encoder);
    if (!out_stream)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
        return AVERROR_UNKNOWN;
    }
    AVCodecContext *enc_ctx = out_stream->codec;
    enc_ctx->codec_type = dec_ctx->codec_type;
    *o_audio_st_id = ofmt_ctx->nb_streams-1;//keep track of old o_audio_st_id
    enc_ctx->codec_id = dec_ctx->codec_id;
    enc_ctx->sample_rate = dec_ctx->sample_rate;
    enc_ctx->channel_layout = DEFAULT_AUDIO_OUT_CHANNEL_LAYOUT;
    enc_ctx->channels = DEFAULT_AUDIO_OUT_CHANNELS;
    enc_ctx->sample_fmt = dec_ctx->sample_fmt;
    enc_ctx->time_base.den = enc_ctx->sample_rate;
    enc_ctx->time_base.num = 1;

    out_stream->time_base.den = enc_ctx->sample_rate;
    out_stream->time_base.num = 1;

    if(*audio_last_pts!=0)
    {
        //Rescale to new sample_rate
        *audio_last_pts = av_rescale_rnd(*audio_last_pts, dec_ctx->sample_rate, last_sample_rate, AV_ROUND_UP);
    }
    ret = avcodec_open2(enc_ctx, encoder, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream #%d\n", *o_audio_st_id);
        return ret;
    }
    return 1;
}

static int encode_write_frame_flush(AVFormatContext *ofmt_ctx,
    unsigned int o_stream_index, int *got_frame)
{
    int ret;
    int got_frame_local;
    AVPacket enc_pkt;

    int64_t pts, dts;
    int (*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *);

    int type = ofmt_ctx->streams[o_stream_index]->codec->codec_type;
    if(type == AVMEDIA_TYPE_VIDEO)
    {
        enc_func = avcodec_encode_video2;
    }
    else if (type == AVMEDIA_TYPE_AUDIO)
    {
        enc_func = avcodec_encode_audio2;
    }
    else
    {
        printf("Unknown\n");
        return 0;
    }

    if (!got_frame)
        got_frame = &got_frame_local;

    av_log(NULL, AV_LOG_INFO, "Encoding frame for flushing\n");
    /* encode filtered frame */
    enc_pkt.data = NULL;
    enc_pkt.size = 0;
    av_init_packet(&enc_pkt);
    ret = enc_func(ofmt_ctx->streams[o_stream_index]->codec, &enc_pkt,
                   NULL, got_frame);

    if (ret < 0)
        return ret;
    if (!(*got_frame))
        return 0;

    /* prepare packet for muxing */
    enc_pkt.stream_index = o_stream_index;
    av_packet_rescale_ts(&enc_pkt,
         ofmt_ctx->streams[o_stream_index]->codec->time_base,
         ofmt_ctx->streams[o_stream_index]->time_base);

    av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");
    /* mux encoded frame */
    ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
    return ret;
}

static int encode_write_frame(AVFormatContext *ofmt_ctx, int type,
    AVFrame *filt_frame, unsigned int i_stream_index,
    unsigned int o_stream_index, int *got_frame)
{
    int ret=-1;
    int got_frame_local=-1;

    AVPacket enc_pkt;
    int64_t pts, dts, last_pts, last_dts;
    int (*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *);
    //int type = ifmt_ctx->streams[i_stream_index]->codec->codec_type;

    if(type == AVMEDIA_TYPE_VIDEO) {
        enc_func = avcodec_encode_video2;

    } else if (type == AVMEDIA_TYPE_AUDIO) {
        enc_func = avcodec_encode_audio2;

    } else {
        av_log(NULL, AV_LOG_INFO, "Unknown\n");
        return 0;
    }

    if (!got_frame)
        got_frame = &got_frame_local;

    //av_log(NULL, AV_LOG_INFO, "Encoding frame\n");
    /* encode filtered frame */
    enc_pkt.data = NULL;
    enc_pkt.size = 0;
    av_init_packet(&enc_pkt);
    ret = enc_func(ofmt_ctx->streams[o_stream_index]->codec, &enc_pkt,
                   filt_frame, got_frame);
    //av_frame_free(&filt_frame);
    if (ret < 0)
        return ret;
    if (!(*got_frame))
        return 0;

    /* prepare packet for muxing */
    enc_pkt.stream_index = o_stream_index;
    av_packet_rescale_ts(&enc_pkt,
           ofmt_ctx->streams[o_stream_index]->codec->time_base,
           ofmt_ctx->streams[o_stream_index]->time_base);

    // av_log(NULL, AV_LOG_INFO, "Muxing %s frame pts = %lld dts = %lld\n",
    //        (type == AVMEDIA_TYPE_VIDEO)?"video":"audio", enc_pkt.pts, enc_pkt.dts);
    /* mux encoded frame */
    ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
    return ret;
}

/* audio output */
static AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
                                  uint64_t channel_layout,
                                  int sample_rate, int nb_samples)
{
    AVFrame *frame = av_frame_alloc();
    int ret;
    if (!frame) {
        fprintf(stderr, "Error allocating an audio frame\n");
        exit(1);
    }
    frame->format = sample_fmt;
    frame->channel_layout = channel_layout;
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;
    // if (nb_samples) {
    //     ret = av_frame_get_buffer(frame, 0);
    //     if (ret < 0) {
    //         fprintf(stderr, "Error allocating an audio buffer\n");
    //         exit(1);
    //     }
    // }
    return frame;
}

static int flush_encoder(AVFormatContext **ofmt_ctx_ptr, unsigned int stream_index)
{
    int ret;
    int got_frame;
    AVFormatContext *ofmt_ctx;
    ofmt_ctx = *ofmt_ctx_ptr;

    if (!(ofmt_ctx->streams[stream_index]->codec->codec->capabilities &
          AV_CODEC_CAP_DELAY))
        return 0;

    while (1)
    {
        av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);
        ret = encode_write_frame_flush(ofmt_ctx, stream_index, &got_frame);
        if (ret < 0)
        {
            break;
        }
        if (!got_frame)
        {
            return 0;
        }
    }
    return ret;
}

int close_input(AVFormatContext **ifmt_ctx_ptr)
{
    AVFormatContext *ifmt_ctx;
    ifmt_ctx = *ifmt_ctx_ptr;
    if(ifmt_ctx) {
        for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
            avcodec_close(ifmt_ctx->streams[i]->codec);
        }
        avformat_close_input(ifmt_ctx_ptr);
    }
    return 1;
}

int close_output(AVFormatContext **ofmt_ctx_ptr)
{
    AVFormatContext *ofmt_ctx;
    ofmt_ctx = *ofmt_ctx_ptr;
    if(ofmt_ctx)
    {
        for(int i=0;i<ofmt_ctx->nb_streams;i++)
        {
            if (ofmt_ctx->streams[i] && ofmt_ctx->streams[i]->codec)
            {
                avcodec_close(ofmt_ctx->streams[i]->codec);
            }
            // if (filter_ctx && filter_ctx[i].filter_graph)
            // {
            //     avfilter_graph_free(&filter_ctx[i].filter_graph);
            // }
        }
    }
    //av_free(filter_ctx);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        avio_closep(&ofmt_ctx->pb);
    }
    avformat_free_context(ofmt_ctx);
    return 1;
}

int FX_concat(int nb_files, char **input_files, char *output_file, void (*callback)(int))
{
    int ret;
    AVPacket packet = { .data = NULL, .size = 0 };
    AVFrame *frame = NULL;
    uint8_t **dst_data = NULL;
    int dst_linesize;
    enum AVMediaType type;
    unsigned int stream_index;
    unsigned int i;
    int got_frame;
    int (*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);
    int (*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *);

    AVFormatContext *ifmt_ctx=NULL;
    AVFormatContext *ofmt_ctx=NULL;
    int64_t video_last_pts= 0, video_last_dts= 0, audio_last_pts= 0, audio_last_dts= 0;
    int64_t video_pts= 0, video_dts= 0, audio_pts= 0, audio_dts= 0;
    int o_video_st_id = -1, o_audio_st_id = -1;
    int i_video_st_id = -1, i_audio_st_id = -1;

    av_register_all();
    avfilter_register_all();
    avcodec_register_all();

    for(i=0;i<nb_files;i++)
    {
        ifmt_ctx=NULL;
        struct SwsContext *sws_ctx = NULL;
        struct SwrContext *swr_ctx = NULL;
        int is_need_rescale = 0, is_need_resample = 0;
        int video_buffer_pts = 0, audio_buffer_pts = 0;
        int last_dec_nb_samples=0, last_sample_rate = 0;
        AVCodecContext *dec_ctx=NULL, *enc_ctx=NULL;
        audio_pts = 0;
        if ((ret = open_input_file(input_files[i], &ifmt_ctx,
            &i_audio_st_id, &i_video_st_id)) < 0)
        {
            fprintf(stderr, "Couldn't open file %s\n", input_files[i]);
            return -1;
        }
        if(0==i)
        {
            //Open output_file based on 1st input_file params
            if ((ret = open_output_file(output_file, ifmt_ctx, &ofmt_ctx,
                &o_audio_st_id, &o_video_st_id)) < 0)
            {
                fprintf(stderr, "Couldn't open file %s\n", output_file);
                return -1;
            }
            // if ((ret = init_filters()) < 0)
            // {
            //     return -1;
            // }
        }
        //check if we need scalling
        for(stream_index = 0; stream_index < ifmt_ctx->nb_streams; stream_index++)
        {
            dec_ctx = ifmt_ctx->streams[stream_index]->codec;
            if(dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                enc_ctx = ofmt_ctx->streams[o_video_st_id]->codec;
                sws_ctx = sws_getContext(dec_ctx->width, dec_ctx->height,
                                         dec_ctx->pix_fmt,
                                         enc_ctx->width, enc_ctx->height,
                                         enc_ctx->pix_fmt,
                                         SWS_BILINEAR, NULL, NULL, NULL);
                is_need_rescale = 1;
                printf("Need rescale\n");
            }
            else if(dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                is_need_resample = 1;
                enc_ctx = ofmt_ctx->streams[o_audio_st_id]->codec;
                // if(enc_ctx->sample_rate != dec_ctx->sample_rate)
                // {
                //     last_sample_rate = enc_ctx->sample_rate;
                //     //Create new audio_output_stream
                //     ret = add_audio_stream(ofmt_ctx, dec_ctx, &o_audio_st_id, &audio_last_pts, last_sample_rate);
                //     if(ret < 0)
                //     {
                //         printf("Failed on create new audio stream\n");
                //         return -1;
                //     }
                // }

                swr_ctx = swr_alloc();
                if (!swr_ctx)
                {
                    fprintf(stderr, "Could not allocate resampler context\n");
                    exit(1);
                }
                //set properties for swr_ctx
                av_opt_set_int(swr_ctx, "in_channel_layout",    dec_ctx->channel_layout, 0);
                av_opt_set_int(swr_ctx, "in_channel_count",   dec_ctx->channels,       0);
                av_opt_set_int(swr_ctx, "in_sample_rate",       dec_ctx->sample_rate, 0);
                av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", dec_ctx->sample_fmt, 0);

                av_opt_set_int(swr_ctx, "out_channel_layout",    enc_ctx->channel_layout, 0);
                av_opt_set_int(swr_ctx, "out_channel_count",  enc_ctx->channels,       0);
                av_opt_set_int(swr_ctx, "out_sample_rate",       enc_ctx->sample_rate, 0);
                av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", enc_ctx->sample_fmt, 0);

                /* initialize the resampling context */
                if ((ret = swr_init(swr_ctx)) < 0)
                {
                    fprintf(stderr, "Failed to initialize the resampling context\n");
                    goto end;
                }
            }
            else
            {
                printf("AVMEDIA_TYPE_UNKNOWN\n");
            }
        }/* end of scaling contexts */

        while(av_read_frame(ifmt_ctx, &packet)>=0)
        {
            stream_index = packet.stream_index;
            type = ifmt_ctx->streams[stream_index]->codec->codec_type;
            // if (filter_ctx[stream_index].filter_graph)
            // {
            if(type==AVMEDIA_TYPE_VIDEO || type==AVMEDIA_TYPE_AUDIO)
            {
                frame = av_frame_alloc();
                if (!frame) {
                    ret = AVERROR(ENOMEM);
                    break;
                }
                av_packet_rescale_ts(&packet,
                                     ifmt_ctx->streams[stream_index]->time_base,
                                     ifmt_ctx->streams[stream_index]->codec->time_base);
                /*get the packet, decode packet and encode to open_output_file */
                dec_func = (type == AVMEDIA_TYPE_VIDEO) ? avcodec_decode_video2 :
                    avcodec_decode_audio4;
                ret = dec_func(ifmt_ctx->streams[stream_index]->codec, frame,
                               &got_frame, &packet);

                if (ret < 0)
                {
                    av_frame_free(&frame);
                    fprintf(stderr, "Decoding failed\n");
                    break;
                }
                else
                {
                    //printf("Decode_function ok!\n");
                }

                if (got_frame)
                {
                    printf("Got frame of decode\n");
                    frame->pts = av_frame_get_best_effort_timestamp(frame);
                    if(type==AVMEDIA_TYPE_VIDEO)
                    {
                        printf("\n[VIDEO] ");
                        dec_ctx = ifmt_ctx->streams[stream_index]->codec;
                        enc_ctx = ofmt_ctx->streams[o_video_st_id]->codec;
                        //Recalculate the pts time stamp
                        printf("Primary pts: %lld/old_video_pts: %lld\t", frame->pts, video_pts);
                        //do the resize here
                        if(is_need_rescale)
                        {
                            AVFrame *scale_frame = av_frame_alloc();
                            scale_frame->width = enc_ctx->width;
                            scale_frame->height = enc_ctx->height;
                            scale_frame->format = enc_ctx->pix_fmt;
                            ret = av_frame_get_buffer(scale_frame, 32);
                            if(ret<0)
                            {
                                fprintf(stderr, "Failed to alloc scale_frame buffer\n");
                                break;
                            }
                            sws_scale(sws_ctx, (uint8_t const * const *)frame->data,
                                    frame->linesize, 0, dec_ctx->height,
                                    scale_frame->data, scale_frame->linesize);

                            //In case of frame->pts < 0
                            if(frame->pts + video_buffer_pts < 0)
                            {
                                video_buffer_pts = -(frame->pts+video_buffer_pts);
                            }

                            scale_frame->pts = av_rescale_q(frame->pts,
                                dec_ctx->time_base,enc_ctx->time_base)+video_last_pts;
                            if(scale_frame->pts<=video_pts)
                            {
                                scale_frame->pts++;
                            }
                            printf("current_pts: %lld\n", scale_frame->pts);
                            video_pts=scale_frame->pts;
                            ret = encode_write_frame(ofmt_ctx, type, scale_frame,
                                stream_index, o_video_st_id, NULL);
                            av_frame_free(&scale_frame);
                        }
                        else
                        {
                            video_pts = frame->pts;
                            frame->pts = video_pts+video_last_pts;
                            printf("current_pts: %lld\n", frame->pts);
                            ret = encode_write_frame(ofmt_ctx, type, frame,
                                stream_index, o_video_st_id, NULL);
                        }
                    }
                    else if(type==AVMEDIA_TYPE_AUDIO)
                    {
                        enc_ctx = ofmt_ctx->streams[o_audio_st_id]->codec;
                        int nb_samples = 0,dst_nb_samples=0,max_dst_nb_samples=0;
                        if(audio_buffer_pts+frame->pts < 0)
                        {
                            audio_buffer_pts = -(audio_buffer_pts+frame->pts);
                        }
                        frame->pts += audio_buffer_pts;

                        printf("\n[AUDIO] ");
                        printf("Primary pts: %lld/old_audio_pts: %lld\tnb_samples=%d\n",
                            frame->pts, audio_pts, frame->nb_samples);
                        dec_ctx = ifmt_ctx->streams[stream_index]->codec;

                        dec_ctx->channels = av_get_channel_layout_nb_channels(dec_ctx->channel_layout);
                        if(dec_ctx->channels==0)
                        {
                            dec_ctx->channels = DEFAULT_AUDIO_IN_CHANNELS;
                        }
                        enc_ctx->channels = av_get_channel_layout_nb_channels(enc_ctx->channel_layout);
                        if(enc_ctx->channels == 0)
                        {
                            enc_ctx->channels = DEFAULT_AUDIO_OUT_CHANNELS;
                        }
                        last_dec_nb_samples = nb_samples = frame->nb_samples;
                        //Directly write frame into added audio_stream
                        // frame->pts = audio_pts + audio_last_pts;
                        // audio_pts += frame->nb_samples;
                        // ret = encode_write_frame(ofmt_ctx, type, frame, stream_index, o_audio_st_id, NULL);

                        max_dst_nb_samples = dst_nb_samples =
                            av_rescale_rnd(nb_samples, enc_ctx->sample_rate,
                            dec_ctx->sample_rate, AV_ROUND_UP);
                        //Alloc a temp buffer
                        ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, enc_ctx->channels,
                            dst_nb_samples, enc_ctx->sample_fmt, 0);
                        //Resample to that temp buffer
                        int converted_nb_samples=0;
                        converted_nb_samples = ret = swr_convert(swr_ctx,
                            dst_data, dst_nb_samples,
                            (const uint8_t **)frame->data, frame->nb_samples);
                        //Create the audio_frame based on the converted samples
                        AVFrame *audio_frame = alloc_audio_frame(
                            enc_ctx->sample_fmt, enc_ctx->channel_layout,
                            enc_ctx->sample_rate, converted_nb_samples);
                        printf("converted_nb_samples: %d\n",converted_nb_samples);
                        //Copy the temp buffer to audio_frame
                        ret = av_frame_make_writable(audio_frame);
                        // int64_t dst_bufsize = av_samples_get_buffer_size(&dst_linesize,
                        //     enc_ctx->channels, converted_nb_samples, enc_ctx->sample_fmt, 1);
                        // memcpy(audio_frame->data[0], dst_data, dst_bufsize);
                        av_samples_fill_arrays(audio_frame->data, &dst_linesize,
                            dst_data[0], enc_ctx->channels, converted_nb_samples, enc_ctx->sample_fmt, 1);
                        //Encode the frame
                        audio_frame->pts = audio_pts + audio_last_pts;
                        audio_pts+=converted_nb_samples;//ret = nb_converted_samples
                        ret = encode_write_frame(ofmt_ctx, type, audio_frame,
                            stream_index, o_audio_st_id, NULL);
                        av_frame_free(&audio_frame);
                        //av_freep(&dst_data[0]);

                        // if(audio_frame)
                        // {
                        //     ret = av_frame_make_writable(audio_frame);
                        //     /*--- convert to destination audio sample ---*/
                        //
                        //     // if(dst_nb_samples!=max_dst_nb_samples)
                        //     // {
                        //     //     av_frame_free(&audio_frame);
                        //     //     audio_frame = alloc_audio_frame(
                        //     //         enc_ctx->sample_fmt, enc_ctx->channel_layout,
                        //     //         enc_ctx->sample_rate, dst_nb_samples);
                        //     // }
                        //     ret = swr_convert(swr_ctx,
                        //                       audio_frame->data, dst_nb_samples,
                        //                       (const uint8_t **)frame->data, frame->nb_samples);
                        //     printf("dst_nb_samples: %d \t converted_nb_samples: %d\n", dst_nb_samples, ret);
                        //     ret = swr_convert(swr_ctx,
                        //                     audio_frame->extended_data, dst_nb_samples,
                        //                     (const uint8_t **)frame->extended_data, frame->nb_samples);
                        //     if (ret < 0) {
                        //         fprintf(stderr, "Error while converting\n");
                        //         exit(1);
                        //     }
                        //     audio_frame->pts = audio_pts + audio_last_pts;
                        //     audio_pts+=ret;//ret = nb_converted_samples
                        //     ret = encode_write_frame(ofmt_ctx, type, audio_frame,
                        //         stream_index, o_audio_st_id, NULL);
                        //     av_frame_free(&audio_frame);
                        // }
                        // else
                        // {
                        //     fprintf(stderr, "Cannot alloc output audio stream %s\n", input_files[i]);
                        // }
                    }
                    av_frame_free(&frame);
                    if (ret < 0)
                    {
                        goto end;
                    }
                }
                else
                {
                    av_frame_free(&frame);
                }
            }
            av_packet_unref(&packet);
        }//---> end of while(av_read_frame(ifmt_ctx, &packet)>=0)

        //update pts and dts
        audio_last_pts += (audio_pts+1);
        printf("[AUDIO] Update audio_last_pts = %lld\n", audio_last_pts);
        video_last_pts += (video_pts+1);
        printf("[VIDEO] Update video_last_pts = %lld\n", video_last_pts);

        /*--- Close the input file ---*/
        close_input(&ifmt_ctx);
        /*--- free video scalling context ---*/
        if(sws_ctx)
        {
            sws_freeContext(sws_ctx);
        }
        /*--- free audio resampling context ---*/
        if(swr_ctx)
        {
            swr_free(&swr_ctx);
        }
        if(callback!=NULL)
        {
            (*callback)((i+1)*100/nb_files);
        }
    }/* end of for each input_file */

    /* flush filters and encoders */
    for (i = 0; i < ofmt_ctx->nb_streams; i++)
    {
        /* flush filter */
        // if (!filter_ctx[i].filter_graph)
        //     continue;
        // ret = filter_encode_write_frame(NULL, i);
        // if (ret < 0)
        // {
        //     av_log(NULL, AV_LOG_ERROR, "Flushing filter failed\n");
        //     goto end;
        // }

        /* flush encoder */
        ret = flush_encoder(&ofmt_ctx, i);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
            goto end;
        }
    }
    av_write_trailer(ofmt_ctx);

end:
    if(frame)
    {
        av_frame_free(&frame);
    }
    close_output(&ofmt_ctx);

    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));
    }
    return 0;
}

typedef struct AVFrameList
{
    AVFrame *frame;
    struct AVFrameList *next_ptr;
} AVFrameList;
typedef struct AVFrameQueue
{
    AVFrameList *first, *last, *current;
    int nb_frames;
    int size;
} AVFrameQueue;
int init_queue(AVFrameQueue *q)
{
    q->last = q->first = NULL;
    q->current = q->first;
    q->nb_frames = 0;
    q->size = 0;
}
int put_frame_to_queue(AVFrameQueue *q, AVFrame *i_frame)
{
    AVFrameList *_frame = av_malloc(sizeof(AVFrameList));
    if(!_frame)
    {
        fprintf(stderr, "Cannot alloc AVFrameList\n");
        return 0;
    }
    (_frame->frame) = i_frame;
    _frame->next_ptr = NULL;
    if(!q->last) //queue is NULL, this is the 1st item
    {
        q->first = _frame;
        q->current = q->first;
    }
    else //Append
    {
        q->last->next_ptr = _frame;
    }
    q->last = _frame;
    q->nb_frames++;
    return 1;
}
/**
Get a frame at current and move current to next position
if (current->next_ptr == NULL) {current = first;}
*/
int get_frame_from_queue(AVFrameQueue *q, AVFrame **o_frame)
{
    if(!q->first) //queue NULL
    {
        fprintf(stderr, "Queue is empty\n");
        return 0;
    }
    //put the q->current->frame to output frame
    *o_frame = q->current->frame;
    if(q->current->next_ptr == NULL) //Already at the last pointer
    {
        q->current = q->first;
    }
    else
    {
        q->current = q->current->next_ptr;
    }
    return 1;
}

int open_effect_file(const char *effect_file, AVFrameQueue *e_a_frame, AVFrameQueue *e_v_frame,
    int width, int height, int pix_fmt)
{
    int stream_index, ret=0, got_frame;
    unsigned int i = 0;
    enum AVMediaType type;
    AVFormatContext *efmt_ctx=NULL;
    AVPacket packet = { .data = NULL, .size = 0 };
    AVFormatContext *ifmt_ctx;
    AVCodecContext *e_ctx;
    struct SwsContext *sws_ctx = NULL;
    int64_t video_pts = 0;

    int e_audio_st_id = -1, e_video_st_id = -1;

    if ((ret = avformat_open_input(&efmt_ctx, effect_file, NULL, NULL)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot open effect file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(efmt_ctx, NULL)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    for (i = 0; i < efmt_ctx->nb_streams; i++)
    {
        AVStream *stream;
        AVCodecContext *codec_ctx;
        stream = efmt_ctx->streams[i];
        codec_ctx = stream->codec;

        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
            || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            /* Open decoder */
            ret = avcodec_open2(codec_ctx,
                                avcodec_find_decoder(codec_ctx->codec_id), NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return ret;
            }
            if(codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                e_audio_st_id = i;
            } else {
                e_video_st_id = i;
                sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height,
                    codec_ctx->pix_fmt,
                    width, height,
                    pix_fmt,
                    SWS_BILINEAR, NULL, NULL, NULL);
            }
        }
    }

    av_dump_format(efmt_ctx, 0, effect_file, 0);
    struct AVRational o_timebase = (AVRational){1,DEFAULT_VIDEO_OUT_FRAME_RATE};

    while(av_read_frame(efmt_ctx, &packet)>=0)
    {
        stream_index = packet.stream_index;
        type = efmt_ctx->streams[stream_index]->codec->codec_type;

        if(type==AVMEDIA_TYPE_VIDEO)
        {
            AVFrame *frame = av_frame_alloc();
            if (!frame) {
                ret = AVERROR(ENOMEM);
                break;
            }
            av_packet_rescale_ts(&packet,
                efmt_ctx->streams[stream_index]->time_base,
                efmt_ctx->streams[stream_index]->codec->time_base);
            ret = avcodec_decode_video2(efmt_ctx->streams[stream_index]->codec, frame,
                &got_frame, &packet);
            if(ret < 0)
            {
                printf("Cannot decode effect video frame\n");
                return -1;
            }
            //Should convert to ofmt_ctx format & frame_rate
            if(got_frame)
            {
                e_ctx = efmt_ctx->streams[stream_index]->codec;
                frame->pts = av_frame_get_best_effort_timestamp(frame);
                AVFrame *video_frame = av_frame_alloc();
                if(!video_frame)
                {
                    ret = AVERROR(ENOMEM);
                    break;
                }
                video_frame->width = width;
                video_frame->height = height;
                video_frame->format = pix_fmt;
                ret = av_frame_get_buffer(video_frame, 32);
                if(ret<0)
                {
                    fprintf(stderr, "Failed to alloc video_frame buffer\n");
                    break;
                }
                sws_scale(sws_ctx, (uint8_t const * const *)frame->data,
                        frame->linesize, 0, e_ctx->height,
                        video_frame->data, video_frame->linesize);
                video_frame->pts = av_rescale_q(frame->pts, e_ctx->time_base, o_timebase);
                if(video_frame->pts<=video_pts)
                {
                    video_frame->pts++;
                }
                printf("current_pts: %lld\n", video_frame->pts);
                video_pts=video_frame->pts;
                //Put to queue
                put_frame_to_queue(e_v_frame, video_frame);
            }
            av_frame_free(&frame);
        }
        else if(type==AVMEDIA_TYPE_AUDIO)
        {
            printf("[EFFECT] Audio frame read\n");
        }
    }/*--- End of while(av_read_frame(efmt_ctx, &packet)>=0) ---*/
    close_input(&efmt_ctx);
    return 1;
}

int open_output_file_with_effect(const char *output_file,
    AVFormatContext *ifmt_ctx, AVFormatContext **ofmt_ctx,
    int *o_audio_st_id, int *o_audio_effect_st_id, int *o_video_st_id)
{
    //Temporarily no audio for effect_file
    open_output_file(output_file, ifmt_ctx, ofmt_ctx, o_audio_st_id, o_video_st_id);
    return 1;
}

//x = col = width, y = row = height;
int blend_pixel(AVFrame **dst, AVFrame *effect, int x, int y, float alpha)
{
    //because input ís YUV_420P = PLANAR, so the pixels are YYYY...UUUU...VVVV
    //Modified: input is now RGB24 = RGB RGB RGB ...
    //printf("[blend_pixel] Pos: %d x %d \n", x, y);
    int uv_x=x, uv_y=y;
    // uint8_t dst_r = (*dst)->data[0+(*dst)->linesize[0] * y + x];
    // uint8_t dst_g = (*dst)->data[0+(*dst)->linesize[0] * uv_y + uv_x];
    // uint8_t dst_b = (*dst)->data[0+(*dst)->linesize[0] * uv_y + uv_x];
    uint8_t dst_r = (*dst)->data[0][0 + ((*dst)->linesize[0] * y + 3*x)];
    uint8_t dst_g = (*dst)->data[0][1 + ((*dst)->linesize[0] * y + 3*x)];
    uint8_t dst_b = (*dst)->data[0][2 + ((*dst)->linesize[0] * y + 3*x)];

    uint8_t eff_r = effect->data[0][0 + (effect->linesize[0] * y + 3*x)];
    uint8_t eff_g = effect->data[0][1 + (effect->linesize[0] * y + 3*x)];
    uint8_t eff_b = effect->data[0][2 + (effect->linesize[0] * y + 3*x)];
    float difference = 1.0*(abs(dst_r-eff_r) + abs(dst_g-eff_g) + abs(dst_b-eff_b));
    float coeff = difference/MAX_COLOR_DIFF;
    coeff = (coeff>1)?1:coeff;
    // printf("[blend_pixel] Before: %u - %u - %u\t", dst_r, dst_g, dst_b);

    dst_r = (uint8_t)(dst_r * coeff + eff_r * (1-coeff));
    dst_g = (uint8_t)(dst_g * coeff + eff_g * (1-coeff));
    dst_b = (uint8_t)(dst_b * coeff + eff_b * (1-coeff));

    // printf("After: %u - %u - %u\n", dst_r, dst_g, dst_b);
    // if(sum_before != sum_after)
    // {
    //     printf("[blend_pixel] CHANGED!\n");
    // }

    (*dst)->data[0][0 + ((*dst)->linesize[0] * y + 3*x)] = dst_r;
    (*dst)->data[0][1 + ((*dst)->linesize[0] * y + 3*x)] = dst_g;
    (*dst)->data[0][2 + ((*dst)->linesize[0] * y + 3*x)] = dst_b;

    return 1;
}
int overlay_frame(AVFrame **frame, AVFrame *effect_frame, int width, int height, float alpha)
{
    int x=0, y=0, ret = 0;
    struct SwsContext *sws_yuv_rgb = NULL, *sws_rgb_yuv = NULL;
    AVFrame *rgb_frame = NULL, *rgb_effect = NULL;
    //Prepare the RGB frames
    rgb_frame = av_frame_alloc();
    rgb_frame->width = width;
    rgb_frame->height = height;
    rgb_frame->format = AV_PIX_FMT_RGB24;
    ret = av_frame_get_buffer(rgb_frame, 32);
    rgb_effect = av_frame_alloc();
    rgb_effect->width = width;
    rgb_effect->height = height;
    rgb_effect->format = AV_PIX_FMT_RGB24;
    ret = av_frame_get_buffer(rgb_effect, 32);

    sws_yuv_rgb = sws_getContext(width, height, (*frame)->format,
        width, height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, NULL, NULL, NULL);
    sws_rgb_yuv = sws_getContext(width, height,AV_PIX_FMT_RGB24,
        width, height, (*frame)->format,
        SWS_BILINEAR, NULL, NULL, NULL);
    //clone from YUV420P to RGB24
    ret = sws_scale(sws_yuv_rgb, (uint8_t const * const *)(*frame)->data,
            (*frame)->linesize, 0, height,
            rgb_frame->data, rgb_frame->linesize);
    if(ret<0)
    {
        fprintf(stderr,"cannot convert *frame to rgb_frame");
        return -1;
    }

    ret = sws_scale(sws_yuv_rgb, (uint8_t const * const *)effect_frame->data,
            effect_frame->linesize, 0, height,
            rgb_effect->data, rgb_effect->linesize);
    if(ret<0)
    {
        fprintf(stderr,"cannot convert effect_frame to rgb_effect");
        return -1;
    }

    for(y=0; y<height; y++)
    {
        for(x = 0; x < width; x++)
        {
            blend_pixel(&rgb_frame, rgb_effect, x, y, alpha);
        }
    }

    // clone back from RGB24 to YUV420P
    ret = sws_scale(sws_rgb_yuv, (uint8_t const * const *)rgb_frame->data,
            rgb_frame->linesize, 0, height,
            (*frame)->data, (*frame)->linesize);
    if(ret<0)
    {
        fprintf(stderr,"cannot convert rgb_frame back to frame");
        return -1;
    }
    av_frame_free(&rgb_frame);
    av_frame_free(&rgb_effect);
    sws_freeContext(sws_yuv_rgb);
    sws_freeContext(sws_rgb_yuv);

    return 1;
}
int FX_overlay(const char *input_file, const char *effect_file, const char *output_file, float alpha)
{
    /**
    - Read the inputfile, get the related param
     + Video (width, height, format, frame_rate)
     + Audio (sample_rate, sample_fmt, codec)

    - Read the effectFile, get the related params
     + Video (width, height, format, frame_rate) convert to inputFile (w, h, f, f_r)
     + Audio: add a audio stream for every effect audio stream
    */
    int ret;
    AVFormatContext *ifmt_ctx=NULL, *efmt_ctx=NULL, *ofmt_ctx=NULL;
    AVPacket packet = { .data = NULL, .size = 0 };
    AVFrame *frame = NULL;
    AVFrame **e_v_frame_buf, **e_a_frame_buf;
    AVCodecContext *dec_ctx,*enc_ctx;
    AVFrameQueue e_audio_queue, e_video_queue;
    enum AVMediaType type;
    unsigned int stream_index;
    unsigned int i;
    int got_frame;
    int (*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);
    int (*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *);
    int o_video_st_id=0, i_video_st_id = 0, o_audio_st_id=0, i_audio_st_id=0, o_audio_effect_st_id = 0;
    int video_buffer_pts=0, video_pts=0;
    struct SwsContext *sws_ctx;

    av_register_all();
    avfilter_register_all();
    avcodec_register_all();

    //Open input file and get the input_format_context, input_audio_stream_id, input_video_stream_id
    open_input_file(input_file, &ifmt_ctx, &i_audio_st_id, &i_video_st_id);
    dec_ctx = ifmt_ctx->streams[i_video_st_id]->codec;
    //Open effect file, put the audio frame to e_a_frame_buf, video frames to e_v_frame_buf
    init_queue(&e_audio_queue);
    init_queue(&e_video_queue);
    open_effect_file(effect_file, &e_audio_queue, &e_video_queue, dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt);
    // open_output_file_with_effect(output_file, ifmt_ctx, &ofmt_ctx,
    //     &o_audio_st_id, &o_audio_effect_st_id, &o_video_st_id);
    open_output_file(output_file, ifmt_ctx, &ofmt_ctx, &o_audio_st_id, &o_video_st_id);
    enc_ctx = ofmt_ctx->streams[o_video_st_id]->codec;
    sws_ctx = sws_getContext(dec_ctx->width, dec_ctx->height,
                             dec_ctx->pix_fmt,
                             enc_ctx->width, enc_ctx->height,
                             enc_ctx->pix_fmt,
                             SWS_BILINEAR, NULL, NULL, NULL);
    //Scan through input file frames
    printf("/*---- Overlay %f ----*/\n", alpha);
    while(av_read_frame(ifmt_ctx, &packet)>=0)
    {
        stream_index = packet.stream_index;
        type = ifmt_ctx->streams[stream_index]->codec->codec_type;

        if(type==AVMEDIA_TYPE_VIDEO)
        {
            //Decode the video packet to frame
            frame = av_frame_alloc();
            if (!frame) {
                ret = AVERROR(ENOMEM);
                break;
            }
            av_packet_rescale_ts(&packet,
                                 ifmt_ctx->streams[stream_index]->time_base,
                                 ifmt_ctx->streams[stream_index]->codec->time_base);
            /*get the packet, decode packet and encode to open_output_file */
            ret = avcodec_decode_video2(ifmt_ctx->streams[stream_index]->codec, frame,
                           &got_frame, &packet);
            if (ret < 0)
            {
                av_frame_free(&frame);
                fprintf(stderr, "Decoding failed\n");
                break;
            }
            else
            {
                //printf("Decode_function ok!\n");
            }

            if (got_frame)
            {
                //printf("Got frame of decode\n");
                frame->pts = av_frame_get_best_effort_timestamp(frame);
                //Overlay the frame
                AVFrame *effect_frame;
                ret = get_frame_from_queue(&e_video_queue, &effect_frame);
                if(ret<0)
                {
                    printf("Cannot get frame from video_queue\n");
                    return 0;
                }
                // dec_ctx = ifmt_ctx->streams[stream_index]->codec;
                // enc_ctx = ofmt_ctx->streams[o_video_st_id]->codec;
                AVFrame *scale_frame = av_frame_alloc();
                scale_frame->width = enc_ctx->width;
                scale_frame->height = enc_ctx->height;
                scale_frame->format = enc_ctx->pix_fmt;
                ret = av_frame_get_buffer(scale_frame, 32);
                if(ret<0)
                {
                    fprintf(stderr, "Failed to alloc scale_frame buffer\n");
                    break;
                }
                sws_scale(sws_ctx, (uint8_t const * const *)frame->data,
                        frame->linesize, 0, dec_ctx->height,
                        scale_frame->data, scale_frame->linesize);
                if(frame->pts + video_buffer_pts < 0)
                {
                    video_buffer_pts = -(frame->pts+video_buffer_pts);
                }

                scale_frame->pts = av_rescale_q(frame->pts,
                    dec_ctx->time_base,enc_ctx->time_base);
                if(scale_frame->pts<=video_pts)
                {
                    scale_frame->pts++;
                }
                video_pts=scale_frame->pts;

                ret = overlay_frame(&scale_frame, effect_frame, enc_ctx->width, enc_ctx->height, alpha);
                if(ret < 0)
                {
                    printf("Cannot do the overlay\n");
                    return 0;
                }
                effect_frame->pts = scale_frame->pts;

                //Encode to output video stream
                ret = encode_write_frame(ofmt_ctx, type, scale_frame,
                    stream_index, o_video_st_id, NULL);
                av_frame_free(&scale_frame);

            }
            av_frame_free(&frame);
        }
        else if(type==AVMEDIA_TYPE_AUDIO)
        {
            //Decode the audio packet to frame
            //Encode to output audio stream
            //printf("[AUDIO] Write packet\n");
            ret = av_interleaved_write_frame(ofmt_ctx, &packet);
            if(ret < 0)
            {
                printf("Cannot write packet\n");
                return -1;
            }
            /*--- assumed that no audio effect first ---*/
            //Get the effect audio frame from queue
            //Encode to output audio effect stream
        }
        else
        {
            //printf("AVMEDIA_TYPE_UNKNOWN\n");
        }
        av_packet_unref(&packet);
    }/*--- End of while(av_read_frame(ifmt_ctx, &packet)>=0) ---*/
    close_input(&ifmt_ctx);
    if(sws_ctx)
    {
        sws_freeContext(sws_ctx);
    }
    /* flush filters and encoders */
    for (i = 0; i < ofmt_ctx->nb_streams; i++)
    {
        /* flush encoder */
        ret = flush_encoder(&ofmt_ctx, i);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
        }
    }
    av_write_trailer(ofmt_ctx);
    close_output(&ofmt_ctx);
    return 0;
}
