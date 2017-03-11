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
#define DEFAULT_AUDIO_OUT_SAMPLE_FMT        AV_SAMPLE_FMT_FLTP
#define DEFAULT_AUDIO_OUT_CHANNELS          2
#define DEFAULT_AUDIO_OUT_CHANNEL_LAYOUT    AV_CH_LAYOUT_STEREO
#define DEFAULT_AUDIO_OUT_CODEC             AV_CODEC_ID_AAC
#define AUDIO_OUT_EXPANSION_BUFFER          1024

//#define DEBUG_DISPLAY_SCREEN

static AVFormatContext *ifmt_ctx;
static AVFormatContext *ofmt_ctx;
typedef struct FilteringContext {
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;
} FilteringContext;
static FilteringContext *filter_ctx;

static int64_t video_last_pts= 0, video_last_dts= 0, audio_last_pts= 0, audio_last_dts= 0;
static int64_t video_pts= 0, video_dts= 0, audio_pts= 0, audio_dts= 0;

unsigned int displayWidth = 0, displayHeight = 0;
int o_video_st_id = -1, o_audio_st_id = -1;
int i_video_st_id = -1, i_audio_st_id = -1;

static int open_input_file(const char *filename)
{
    int ret;
    unsigned int i;

    ifmt_ctx = NULL;
    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

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
                i_audio_st_id = i;
            } else {
                i_video_st_id = i;
            }
        }
    }

    av_dump_format(ifmt_ctx, 0, filename, 0);
    return 0;
}

static int open_output_file(const char *filename)
{
    AVStream *out_stream;
    AVStream *in_stream;
    AVOutputFormat *fmt;
    AVCodecContext *dec_ctx, *enc_ctx;
    AVCodec *encoder;
    int ret;
    unsigned int i;

    ofmt_ctx = NULL;
    fmt = NULL;
    fmt = av_guess_format(NULL, filename, NULL);
    if(!fmt) {
        printf("Could not deduce output format from file extension: using MOV.\n");
        fmt = av_guess_format("mov", NULL, NULL);
    }
    if(!fmt) {
        fprintf(stderr, "Couldn't find output format for %s\n", filename);
        exit(1);
    }
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
    if (!ofmt_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }
    ofmt_ctx->oformat = fmt;

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        //http://stackoverflow.com/questions/29348443/cannot-run-transcode-c-from-ffmpeg-examples
        //moved to @new_stream
        // out_stream = avformat_new_stream(ofmt_ctx, NULL);
        // if (!out_stream) {
        //     av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
        //     return AVERROR_UNKNOWN;
        // }
        //====> also not worked!

        in_stream = ifmt_ctx->streams[i];
        dec_ctx = in_stream->codec;


        if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
            || dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            /* in this example, we choose transcoding to same codec */
            encoder = avcodec_find_encoder(dec_ctx->codec_id);
            if (!encoder) {
                av_log(NULL, AV_LOG_FATAL, "Neccessary encoder not found\n");
                return AVERROR_INVALIDDATA;
            }
            //@new_stream
            //out_stream = avformat_new_stream(ofmt_ctx, NULL);
            out_stream = avformat_new_stream(ofmt_ctx, encoder);
            if (!out_stream) {
                av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
                return AVERROR_UNKNOWN;
            }
            enc_ctx = out_stream->codec;
            enc_ctx->codec_type = dec_ctx->codec_type;
            /* In this example, we transcode to same properties (picture size,
             * sample rate etc.). These properties can be changed for output
             * streams easily using filters */
            if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                o_video_st_id = i;
    //            enc_ctx->qmin = 3;
    //            enc_ctx->qmax = 30;
    //            enc_ctx->qcompress = 1;

                displayHeight = enc_ctx->height = dec_ctx->height;
                displayWidth = enc_ctx->width  = dec_ctx->width;
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
                if(ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
                    enc_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
                }
            } else {
                o_audio_st_id = i;
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
                enc_ctx->sample_fmt = dec_ctx->sample_fmt;

                enc_ctx->time_base.den = enc_ctx->sample_rate;// dec_ctx->time_base.den;
                enc_ctx->time_base.num = 1;//dec_ctx->time_base.num;

                out_stream->time_base.den = enc_ctx->sample_rate;//dec_ctx->time_base.den;
                out_stream->time_base.num = 1;//dec_ctx->time_base.num;
                printf("[AUDIO] dec_ctx->frame_size: %d; sample_rate: %d; channels: %d;channel_layout: %d\n",
                       dec_ctx->frame_size, dec_ctx->sample_rate, dec_ctx->channels, dec_ctx->channel_layout);
            }

            /* Third parameter can be used to pass settings to encoder */
            ret = avcodec_open2(enc_ctx, encoder, NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR,
                       "Cannot open %s encoder for stream #%u\n",
                       (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)?"video":"audio",
                       i);
                return ret;
            }
        } else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN) {
            av_log(NULL, AV_LOG_FATAL,
                   "Elementary stream #%d is of unknown type, cannot proceed\n", i);
            return AVERROR_INVALIDDATA;
        } else {
            /* if this stream must be remuxed */
            // ret = avcodec_copy_context(ofmt_ctx->streams[i]->codec,
            //                            ifmt_ctx->streams[i]->codec);
            // if (ret < 0) {
            //     av_log(NULL, AV_LOG_ERROR, "Copying stream context failed\n");
            //     return ret;
            // }
        }

        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    }
    printf("Dump output format\n");
    av_dump_format(ofmt_ctx, 0, filename, 1);

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", filename);
            return ret;
        }
    }

    /* init muxer, write output file header */
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }

    return 0;
}

static int init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx,
                       AVCodecContext *enc_ctx, const char *filter_spec)
{
    char args[512];
    int ret = 0;
    AVFilter *buffersrc = NULL;
    AVFilter *buffersink = NULL;
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterGraph *filter_graph = avfilter_graph_alloc();

    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        buffersrc = avfilter_get_by_name("buffer");
        buffersink = avfilter_get_by_name("buffersink");
        if (!buffersrc || !buffersink) {
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        snprintf(args, sizeof(args),
                 "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                 dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
                 dec_ctx->time_base.num, dec_ctx->time_base.den,
                 dec_ctx->sample_aspect_ratio.num,
                 dec_ctx->sample_aspect_ratio.den);

        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                           args, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
            goto end;
        }

        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                           NULL, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "pix_fmts",
                             (uint8_t*)&enc_ctx->pix_fmt, sizeof(enc_ctx->pix_fmt),
                             AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
            goto end;
        }
    } else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        buffersrc = avfilter_get_by_name("abuffer");
        buffersink = avfilter_get_by_name("abuffersink");
        if (!buffersrc || !buffersink) {
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        if (!dec_ctx->channel_layout)
            dec_ctx->channel_layout =
            av_get_default_channel_layout(dec_ctx->channels);
        snprintf(args, sizeof(args),
                 "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
                 dec_ctx->time_base.num, dec_ctx->time_base.den, dec_ctx->sample_rate,
                 av_get_sample_fmt_name(dec_ctx->sample_fmt),
                 dec_ctx->channel_layout);
        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                           args, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
            goto end;
        }

        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                           NULL, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
                             (uint8_t*)&enc_ctx->sample_fmt, sizeof(enc_ctx->sample_fmt),
                             AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "channel_layouts",
                             (uint8_t*)&enc_ctx->channel_layout,
                             sizeof(enc_ctx->channel_layout), AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
                             (uint8_t*)&enc_ctx->sample_rate, sizeof(enc_ctx->sample_rate),
                             AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
            goto end;
        }
    } else {
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if (!outputs->name || !inputs->name) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec,
                                        &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

    /* Fill FilteringContext */
    fctx->buffersrc_ctx = buffersrc_ctx;
    fctx->buffersink_ctx = buffersink_ctx;
    fctx->filter_graph = filter_graph;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

static int init_filters(void)
{
    const char *filter_spec;
    unsigned int i;
    int ret;
    filter_ctx = av_malloc_array(ifmt_ctx->nb_streams, sizeof(*filter_ctx));
    if (!filter_ctx)
        return AVERROR(ENOMEM);

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        filter_ctx[i].buffersrc_ctx  = NULL;
        filter_ctx[i].buffersink_ctx = NULL;
        filter_ctx[i].filter_graph   = NULL;
        if (!(ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO
              || ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO))
            continue;


        if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            filter_spec = "null"; /* passthrough (dummy) filter for video */
        else
            filter_spec = "anull"; /* passthrough (dummy) filter for audio */
        ret = init_filter(&filter_ctx[i], ifmt_ctx->streams[i]->codec,
                          ofmt_ctx->streams[i]->codec, filter_spec);
        if (ret)
            return ret;
    }
    return 0;
}

static int encode_write_frame_flush(unsigned int stream_index, int *got_frame) {
    int ret;
    int got_frame_local;
    AVPacket enc_pkt;

    int64_t pts, dts;
    int (*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *);

    int type = ofmt_ctx->streams[stream_index]->codec->codec_type;
    if(type == AVMEDIA_TYPE_VIDEO) {
        enc_func = avcodec_encode_video2;
        // pts = video_pts++;
        // dts = video_dts++;
    } else if (type == AVMEDIA_TYPE_AUDIO) {
        enc_func = avcodec_encode_audio2;
        // pts = audio_pts++;
        // dts = audio_dts++;
    } else {
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
    ret = enc_func(ofmt_ctx->streams[stream_index]->codec, &enc_pkt,
                   NULL, got_frame);

    if (ret < 0)
        return ret;
    if (!(*got_frame))
        return 0;

    /* prepare packet for muxing */
    enc_pkt.stream_index = stream_index;
    av_packet_rescale_ts(&enc_pkt,
         ofmt_ctx->streams[stream_index]->codec->time_base,
         ofmt_ctx->streams[stream_index]->time_base);

    //enc_pkt.pts = av_rescale_q(enc_pkt.pts,
    //    ofmt_ctx->streams[stream_index]->codec->time_base,
    //    ofmt_ctx->streams[stream_index]->time_base);
    av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");
    /* mux encoded frame */
    ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
    return ret;
}

static int encode_write_frame(AVFrame *filt_frame, unsigned int i_stream_index,
                              int *got_frame) {
    int ret;
    int got_frame_local;
    int o_stream_idx = -1;
    AVPacket enc_pkt;
    int64_t pts, dts, last_pts, last_dts;
    int (*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *);
    int type = ifmt_ctx->streams[i_stream_index]->codec->codec_type;

    if(type == AVMEDIA_TYPE_VIDEO) {
        enc_func = avcodec_encode_video2;
        o_stream_idx = o_video_st_id;
        // pts = video_pts++;
        // last_pts = video_last_pts;
        // dts = video_dts++;
        // last_dts = video_last_dts;
    } else if (type == AVMEDIA_TYPE_AUDIO) {
        enc_func = avcodec_encode_audio2;
        o_stream_idx = o_audio_st_id;
        // pts = audio_pts++;
        // last_pts = audio_last_pts;
        // dts = audio_dts++;
        // last_dts = audio_last_dts;
    } else {
        printf("Unknown\n");
        return 0;
    }

    if (!got_frame)
        got_frame = &got_frame_local;

    av_log(NULL, AV_LOG_INFO, "Encoding frame\n");
    /* encode filtered frame */
    enc_pkt.data = NULL;
    enc_pkt.size = 0;
    av_init_packet(&enc_pkt);
    ret = enc_func(ofmt_ctx->streams[o_stream_idx]->codec, &enc_pkt,
                   filt_frame, got_frame);
    //av_frame_free(&filt_frame);
    if (ret < 0)
        return ret;
    if (!(*got_frame))
        return 0;

    /* prepare packet for muxing */
    enc_pkt.stream_index = o_stream_idx;
    av_packet_rescale_ts(&enc_pkt,
           ofmt_ctx->streams[o_stream_idx]->codec->time_base,
           ofmt_ctx->streams[o_stream_idx]->time_base);
    //enc_pkt.pts = av_rescale_q(enc_pkt.pts,
    //             ofmt_ctx->streams[o_stream_idx]->codec->time_base,
    //             ofmt_ctx->streams[o_stream_idx]->time_base);

    printf("Muxing %s frame pts = %lld dts = %lld\n",
           (type == AVMEDIA_TYPE_VIDEO)?"video":"audio", enc_pkt.pts, enc_pkt.dts);
    /* mux encoded frame */
    ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
    return ret;
}

// static int encode_write_frame(AVFrame *filt_frame, unsigned int stream_index,
//     int *got_frame) {
//     int ret;
//     int got_frame_local;
//     AVPacket enc_pkt;
//     int (*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *) =
//         (ifmt_ctx->streams[stream_index]->codec->codec_type ==
//          AVMEDIA_TYPE_VIDEO) ? avcodec_encode_video2 : avcodec_encode_audio2;
//
//     if (!got_frame)
//         got_frame = &got_frame_local;
//
//     av_log(NULL, AV_LOG_INFO, "Encoding frame\n");
//     /* encode filtered frame */
//     enc_pkt.data = NULL;
//     enc_pkt.size = 0;
//     av_init_packet(&enc_pkt);
//     ret = enc_func(ofmt_ctx->streams[stream_index]->codec, &enc_pkt,
//             filt_frame, got_frame);
//     av_frame_free(&filt_frame);
//     if (ret < 0)
//         return ret;
//     if (!(*got_frame))
//         return 0;
//
//     /* prepare packet for muxing */
//     enc_pkt.stream_index = stream_index;
//     av_packet_rescale_ts(&enc_pkt,
//                          ofmt_ctx->streams[stream_index]->codec->time_base,
//                          ofmt_ctx->streams[stream_index]->time_base);
//
//     av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");
//     /* mux encoded frame */
//     ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
//     return ret;
// }

static int filter_encode_write_frame(AVFrame *frame, unsigned int stream_index)
{
    int ret;
    AVFrame *filt_frame;

    av_log(NULL, AV_LOG_INFO, "Pushing decoded frame to filters\n");
    /* push the decoded frame into the filtergraph */
    ret = av_buffersrc_add_frame_flags(filter_ctx[stream_index].buffersrc_ctx,
                                       frame, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
        return ret;
    }

    /* pull filtered frames from the filtergraph */
    while (1) {
        filt_frame = av_frame_alloc();
        if (!filt_frame) {
            ret = AVERROR(ENOMEM);
            break;
        }
        av_log(NULL, AV_LOG_INFO, "Pulling filtered frame from filters\n");
        ret = av_buffersink_get_frame(filter_ctx[stream_index].buffersink_ctx,
                                      filt_frame);
        if (ret < 0) {
            /* if no more frames for output - returns AVERROR(EAGAIN)
             * if flushed and no more frames for output - returns AVERROR_EOF
             * rewrite retcode to 0 to show it as normal procedure completion
             */
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                ret = 0;
            av_frame_free(&filt_frame);
            break;
        }

        filt_frame->pict_type = AV_PICTURE_TYPE_NONE;
        ret = encode_write_frame(filt_frame, stream_index, NULL);
        if (ret < 0)
            break;
    }

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
    if (nb_samples) {
        ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) {
            fprintf(stderr, "Error allocating an audio buffer\n");
            exit(1);
        }
    }
    return frame;
}

static int get_format_from_sample_fmt(const char **fmt, enum AVSampleFormat sample_fmt)
{
    int i;
    struct sample_fmt_entry {
        enum AVSampleFormat sample_fmt; const char *fmt_be, *fmt_le;
    } sample_fmt_entries[] = {
        { AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
        { AV_SAMPLE_FMT_S16, "s16be", "s16le" },
        { AV_SAMPLE_FMT_S32, "s32be", "s32le" },
        { AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
        { AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
    };
    *fmt = NULL;
    for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
        struct sample_fmt_entry *entry = &sample_fmt_entries[i];
        if (sample_fmt == entry->sample_fmt) {
            *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
            return 0;
        }
    }
    fprintf(stderr,
            "Sample format %s not supported as output format\n",
            av_get_sample_fmt_name(sample_fmt));
    return AVERROR(EINVAL);
}

static int flush_encoder(unsigned int stream_index)
{
    int ret;
    int got_frame;

    if (!(ofmt_ctx->streams[stream_index]->codec->codec->capabilities &
          AV_CODEC_CAP_DELAY))
        return 0;

    while (1) {
        av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);
        ret = encode_write_frame_flush(stream_index, &got_frame);
        if (ret < 0)
            break;
        if (!got_frame)
            return 0;
    }
    return ret;
}

int close_input() {
    if(ifmt_ctx) {
        for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
            avcodec_close(ifmt_ctx->streams[i]->codec);
        }
        avformat_close_input(&ifmt_ctx);
    }
    return 1;
}

int close_output() {
    if(ofmt_ctx) {
        for(int i=0;i<ofmt_ctx->nb_streams;i++) {
            if (ofmt_ctx->streams[i] && ofmt_ctx->streams[i]->codec){
                avcodec_close(ofmt_ctx->streams[i]->codec);
            }
            if (filter_ctx && filter_ctx[i].filter_graph) {
                avfilter_graph_free(&filter_ctx[i].filter_graph);
            }
        }
    }
    av_free(filter_ctx);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
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
    enum AVMediaType type;
    unsigned int stream_index;
    unsigned int i;
    int got_frame;
    int (*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);
    int (*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *);

    av_register_all();
    avfilter_register_all();
    avcodec_register_all();

    for(i=0;i<nb_files;i++)
    {

        struct SwsContext *sws_ctx = NULL;
        struct SwrContext *swr_ctx = NULL;
        int is_need_rescale = 0, isNeedResample = 0;
        int video_buffer_pts = 0;
        int last_dec_nb_samples=0;
        AVCodecContext *dec_ctx=NULL, *enc_ctx=NULL;
        if ((ret = open_input_file(input_files[i])) < 0){
            fprintf(stderr, "Couldn't open file %s\n", input_files[i]);
            return -1;
        }
        if(0==i)
        {
            //Open output_file based on 1st input_file params
            if ((ret = open_output_file(output_file)) < 0)
            {
                fprintf(stderr, "Couldn't open file %s\n", output_file);
                return -1;
            }
            if ((ret = init_filters()) < 0)
            {
                return -1;
            }
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
                                         displayWidth, displayHeight,
                                         enc_ctx->pix_fmt,
                                         SWS_BILINEAR, NULL, NULL, NULL);
                is_need_rescale = 1;
                printf("Need rescale\n");
            }
            else if(dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                isNeedResample = 1;
                enc_ctx = ofmt_ctx->streams[o_audio_st_id]->codec;
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

                if (ret < 0) {
                    av_frame_free(&frame);
                    fprintf(stderr, "Decoding failed\n");
                    break;
                } else {
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
                            scale_frame->width = displayWidth;
                            scale_frame->height = displayHeight;
                            scale_frame->format = ofmt_ctx->streams[o_video_st_id]->codec->pix_fmt;
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
                            printf("current_pts: %lld\n", scale_frame->pts);
                            video_pts=scale_frame->pts;
                            ret = encode_write_frame(scale_frame, stream_index, NULL);
                            av_frame_free(&scale_frame);
                        }
                        else
                        {
                            video_pts = frame->pts;
                            frame->pts = video_pts+video_last_pts;
                            printf("current_pts: %lld\n", frame->pts);
                            ret = encode_write_frame(frame, stream_index, NULL);
                        }
                    }
                    else if(type==AVMEDIA_TYPE_AUDIO)
                    {
                        enc_ctx = ofmt_ctx->streams[o_audio_st_id]->codec;
                        int nb_samples = 0,dst_nb_samples=0,max_dst_nb_samples=0;
                        printf("\n[AUDIO] ");
                        printf("Primary pts: %lld/old_audio_pts: %lld\tnb_samples=%d\n",
                               frame->pts, audio_pts, frame->nb_samples);
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

                        dec_ctx = ifmt_ctx->streams[stream_index]->codec;
                        last_dec_nb_samples = nb_samples = frame->nb_samples;
                        max_dst_nb_samples = dst_nb_samples =
                            av_rescale_rnd(nb_samples, enc_ctx->sample_rate,
                            dec_ctx->sample_rate, AV_ROUND_UP);
                        dst_nb_samples = (int)av_rescale_rnd(
                            swr_get_delay(swr_ctx, dec_ctx->sample_rate) + frame->nb_samples,
                            enc_ctx->sample_rate, dec_ctx->sample_rate, AV_ROUND_UP);
                        AVFrame *audio_frame = alloc_audio_frame(
                            enc_ctx->sample_fmt, enc_ctx->channel_layout,
                            enc_ctx->sample_rate, dst_nb_samples);

                        if(audio_frame)
                        {
                            ret = av_frame_make_writable(audio_frame);
                            /*--- convert to destination audio sample ---*/
                            ret = swr_convert(swr_ctx,
                                              audio_frame->data, dst_nb_samples,
                                              (const uint8_t **)frame->data, frame->nb_samples);
                            if (ret < 0) {
                                fprintf(stderr, "Error while converting\n");
                                exit(1);
                            }
                            if(frame->pts<audio_pts) {
                                frame->pts = (audio_pts+last_dec_nb_samples);
                            }
                            audio_frame->pts = swr_next_pts(swr_ctx, frame->pts);
                            audio_pts=frame->pts;
                            ret = encode_write_frame(audio_frame, stream_index, NULL);
                            av_frame_free(&audio_frame);
                        }
                        else
                        {
                            fprintf(stderr, "Cannot alloc output audio stream %s\n", input_files[i]);
                        }
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
        close_input();
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
        if(callback!=NULL) {
            (*callback)((i+1)*100/nb_files);
        }
    }/* end of for each input_file */

    /* flush filters and encoders */
    for (i = 0; i < ofmt_ctx->nb_streams; i++)
    {
        /* flush filter */
        if (!filter_ctx[i].filter_graph)
            continue;
        ret = filter_encode_write_frame(NULL, i);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Flushing filter failed\n");
            goto end;
        }

        /* flush encoder */
        ret = flush_encoder(i);
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
    close_output();

    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));
    }
    return 0;
}
