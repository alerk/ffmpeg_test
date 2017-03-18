#include <stdio.h>
#include "ffmpeg_function.h"

static void callbackHandle(int arg)
{
    printf("Progress: %d %% - Completed\n", arg);
    return;
}

int main(int argc, char *argv[])
{
    int quit = 0;
    int selection = -1;
    int nb_files = -1, i=0;
    char *input_files[5];
    char output_file[20];
    for(i=0;i<5;i++)
    {
        input_files[i]=(char *)malloc(20*sizeof(char));
    }
    /* Menu: 1. Concat 2. Overlay 3. Quit */
    do
    {
        selection = -1;
        printf("1. Concat\n2. Overlay\n3. Quit\nSelection: ");
        scanf("%d", &selection);
        switch(selection)
        {
            case 1:
            {
                // printf("nb_files: ");
                // scanf("%d", &nb_files);
                // if(nb_files<0)
                // {
                //     nb_files = 2;
                // }
                // for(i=0;i<nb_files;i++)
                // {
                //     printf("File %d: ",i);
                //     scanf("%s", input_files[i]);
                // }
                // printf("Output: ");
                // scanf("%s", output_file);
                nb_files = 2;
                strcpy(input_files[0], "f1.MOV");
                strcpy(input_files[1], "vid11.MOV");
                strcpy(output_file, "f_o.MOV");
                FX_concat(nb_files, input_files, output_file, &callbackHandle);
                printf("Job done! Choose next task:\n");
            }
            break;
            case 2:
            {
                strcpy(input_files[0], "f1.MOV");
                strcpy(input_files[1], "03s_017.mp4");
                strcpy(output_file, "f_overlay.MOV");
                float alpha = 0.1f;
                // for(int t=1;t<10;t++)
                // {
                //     sprintf(output_file, "f_overlay_%d.MOV",t*10);
                //
                // }
                FX_overlay(input_files[0], input_files[1], output_file, alpha);
                printf("Job overlay done! Choose next task:\n");
            }
            break;
            case 3:
                quit = 1;
            break;
            default:
                printf("Invalid option (%d)\n", selection);
                break;
        }
    /* If 1:
        Accept number of input, input1, input2, ..., output from console */

    /* If 2:
        Quit */

    } while (quit!=1);
    for(i=0;i<5;i++)
    {
        free(input_files[i]);
    }

    // if(argc<3)
    // {
    //     fprintf(stderr, "Please call with enough param\n");
    //     fprintf(stderr, "inputFile1 inputFile2 outputFile");
    //     return -1;
    // }
    // char *inputFiles[] = {argv[1], argv[2]};
    // int nb_files = argc - 2;
    // // params: inputFiles, outputFile, width, height, codec = 0, callbackHandle;
    // FX_concat(nb_files, inputFiles, argv[3], &callbackHandle);

    printf("Program ended!\n");
    return 0;
}
#if 0
    int ret;
    AVPacket packet = { .data = NULL, .size = 0 };
    AVFrame *frame = NULL;
    enum AVMediaType type;
    unsigned int stream_index;
    unsigned int i;
    int got_frame;
    int (*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);
    int (*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *);

    SDL_Window *screen;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_Event       event;

    int64_t videoLastPts = 0;
    int64_t videoLastDts = 0;
    int64_t audioLastPts = 0;
    int64_t audioLastDts = 0;
    char* inputFile;
    char* outputFile;
    av_register_all();
    avfilter_register_all();
    avcodec_register_all();
    //avformat_register_all();


    if(argc < 3) {
        fprintf(stderr, "Usage: %s <input1> ... <output>\n", argv[0]);
        exit(1);
    } else {
        for(i=1;i<argc;i++) {
            printf("File %d: %s\n", i, argv[i]);
        }
    }
    /* create resampler context */
    // swr_ctx = swr_alloc();
    // if (!swr_ctx) {
    //     fprintf(stderr, "Could not allocate resampler context\n");
    //     ret = AVERROR(ENOMEM);
    //     goto end;
    // }

    //foreach inputFile
    for(i=1;i<argc-1;i++) {
        struct SwsContext *sws_ctx = NULL;
        struct SwrContext *swr_ctx = NULL;
        int is_need_rescale = 0, isNeedResample = 0;
        int video_buffer_pts = 0;
        int last_dec_nb_samples=0;
        AVCodecContext *dec_ctx=NULL, *enc_ctx=NULL;
        if ((ret = open_input_file(argv[i])) < 0){
            fprintf(stderr, "Couldn't open file %s\n", argv[i]);
            goto end;
        }
        if(i==1) {
            //Init output based on first input1
            outputFile = argv[argc-1];
            if ((ret = open_output_file(outputFile)) < 0){
                fprintf(stderr, "Couldn't open file %s\n", outputFile);
                goto end;
            }

            if ((ret = init_filters()) < 0) {
                goto end;
            }
#ifdef DEBUG_DISPLAY_SCREEN
            // Make a screen to put our video
            screen = SDL_CreateWindow("FFmpeg Tutorial", SDL_WINDOWPOS_UNDEFINED,
                                      SDL_WINDOWPOS_UNDEFINED, displayWidth, displayHeight, 0);
            if(!screen) {
                fprintf(stderr, "SDL: could not set video mode - exiting\n");
                exit(1);
            }

            renderer = SDL_CreateRenderer(screen, -1, 0);
            if(!renderer) {
                fprintf(stderr, "SDL: couldn't create renderer - exiting\n");
                exit(1);
            }

            // Allocate a place to put our YUV image on that screen
            texture = SDL_CreateTexture(renderer,
                                        SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING,
                                        displayWidth, displayHeight);
            if(!texture) {
                fprintf(stderr, "SDL: could not create texture - exiting\n");
                exit(1);
            }
#endif
        }
        //check if we need scalling
        for(stream_index = 0; stream_index < ifmt_ctx->nb_streams; stream_index++){
            dec_ctx = ifmt_ctx->streams[stream_index]->codec;
            if(dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                enc_ctx = ofmt_ctx->streams[o_video_st_id]->codec;
                    sws_ctx = sws_getContext(dec_ctx->width, dec_ctx->height,
                                             dec_ctx->pix_fmt,
                                             displayWidth, displayHeight,
                                             enc_ctx->pix_fmt,
                                             SWS_BILINEAR, NULL, NULL, NULL);
                    is_need_rescale = 1;
                    printf("Need rescale\n");
            } else if(dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
                isNeedResample = 1;
                enc_ctx = ofmt_ctx->streams[o_audio_st_id]->codec;
                swr_ctx = swr_alloc();
                if (!swr_ctx) {
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
                if ((ret = swr_init(swr_ctx)) < 0) {
                    fprintf(stderr, "Failed to initialize the resampling context\n");
                    goto end;
                }

                //swr_ctx
            } else {
                printf("AVMEDIA_TYPE_UNKNOWN\n");
            }
        }

        while(av_read_frame(ifmt_ctx, &packet)>=0) {
            //if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0)
            //    break;

            stream_index = packet.stream_index;
            type = ifmt_ctx->streams[stream_index]->codec->codec_type;
            //printf("Demuxer gave frame of stream_index %u\n", stream_index);
            if (filter_ctx[stream_index].filter_graph) {
            // if(1) {
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

                if (got_frame) {
                    printf("Got frame of decode\n");
                    frame->pts = av_frame_get_best_effort_timestamp(frame);
                    if(type==AVMEDIA_TYPE_VIDEO){
                        printf("\n[VIDEO] ");
                        dec_ctx = ifmt_ctx->streams[stream_index]->codec;
                        enc_ctx = ofmt_ctx->streams[o_video_st_id]->codec;
                        //Recalculate the pts time stamp
                        printf("Primary pts: %lld/old_video_pts: %lld\t", frame->pts, video_pts);
                        //do the resize here
                        if(is_need_rescale) {
                            AVFrame *scale_frame = av_frame_alloc();
                            scale_frame->width = displayWidth;
                            scale_frame->height = displayHeight;
                            scale_frame->format = ofmt_ctx->streams[o_video_st_id]->codec->pix_fmt;
                            ret = av_frame_get_buffer(scale_frame, 32);
                            if(ret<0) {
                                fprintf(stderr, "Failed to alloc scale_frame buffer\n");
                                break;
                            }
                            sws_scale(sws_ctx, (uint8_t const * const *)frame->data,
                                      frame->linesize, 0, dec_ctx->height,
                                      scale_frame->data, scale_frame->linesize);
#ifdef DEBUG_DISPLAY_SCREEN
                            SDL_UpdateYUVTexture(texture, NULL,
                                                 scale_frame->data[0], scale_frame->linesize[0],
                                                 scale_frame->data[1], scale_frame->linesize[1],
                                                 scale_frame->data[2], scale_frame->linesize[2]);
                            SDL_RenderCopy(renderer, texture, NULL, NULL);
                            SDL_RenderPresent(renderer);
#endif
                            // if(frame->pts<video_pts) {
                            //     frame->pts = video_pts+1;
                            // }
                            // video_pts = frame->pts;
                            //scale_frame->pts = frame->pts + video_last_pts;
                            if(frame->pts + video_buffer_pts < 0) {
                                //In case of frame->pts < 0
                                video_buffer_pts = -(frame->pts+video_buffer_pts);
                            }

                            scale_frame->pts = av_rescale_q(frame->pts,
                                dec_ctx->time_base,enc_ctx->time_base)+video_last_pts;
                            printf("current_pts: %lld\n", scale_frame->pts);
                            video_pts=scale_frame->pts;
                            ret = encode_write_frame(scale_frame, stream_index, NULL);
                            av_frame_free(&scale_frame);
                        } else {
#ifdef DEBUG_DISPLAY_SCREEN
                            SDL_UpdateYUVTexture(texture, NULL,
                                                 frame->data[0], frame->linesize[0],
                                                 frame->data[1], frame->linesize[1],
                                                 frame->data[2], frame->linesize[2]);
                            SDL_RenderCopy(renderer, texture, NULL, NULL);
                            SDL_RenderPresent(renderer);
#endif
                            video_pts = frame->pts;
                            frame->pts = video_pts+video_last_pts;
                            printf("current_pts: %lld\n", frame->pts);
                            ret = encode_write_frame(frame, stream_index, NULL);
                        }
                        //ret = filter_encode_write_frame(frame, stream_index);
                    } else if(type==AVMEDIA_TYPE_AUDIO) {
                        enc_ctx = ofmt_ctx->streams[o_audio_st_id]->codec;
                        int nb_samples = 0,dst_nb_samples=0,max_dst_nb_samples=0;
                        printf("\n[AUDIO] ");
                        printf("Primary pts: %lld/old_audio_pts: %lld\tnb_samples=%d\n",
                               frame->pts, audio_pts, frame->nb_samples);
                        dec_ctx->channels = av_get_channel_layout_nb_channels(dec_ctx->channel_layout);
                        if(dec_ctx->channels==0) {
                            dec_ctx->channels = DEFAULT_AUDIO_IN_CHANNELS;
                        }
                        enc_ctx->channels = av_get_channel_layout_nb_channels(enc_ctx->channel_layout);
                        if(enc_ctx->channels == 0) {
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

                        if(audio_frame) {
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
                        } else {
                            fprintf(stderr, "Cannot alloc output audio stream %s\n", inputFile);
                        }
                    }
                    av_frame_free(&frame);

                    if (ret < 0) {
                        goto end;
                    }

                } else {
                    av_frame_free(&frame);
                    //printf("Didn't get frame of decode\n");
                }
            }
            av_packet_unref(&packet);
        }//---> end of while(av_read_frame(ifmt_ctx, &packet)>=0)
        //update pts and dts
        audio_last_pts += (audio_pts+1);
        printf("[AUDIO] Update audio_last_pts = %lld\n", audio_last_pts);
        // audio_last_dts += audio_dts;
        video_last_pts += (video_pts+1);
        printf("[VIDEO] Update video_last_pts = %lld\n", video_last_pts);
        // video_last_dts += video_dts;

        /*--- Close the input file ---*/
        close_input();
        /*--- free video scalling context ---*/
        if(sws_ctx) {
            sws_freeContext(sws_ctx);
        }
        /*--- free audio resampling context ---*/
        if(swr_ctx) {
            swr_free(&swr_ctx);
        }
    }//---> end of for each input file;
    /* flush filters and encoders */
    for (i = 0; i < ofmt_ctx->nb_streams; i++) {
        /* flush filter */
        if (!filter_ctx[i].filter_graph)
            continue;
        ret = filter_encode_write_frame(NULL, i);
        if (ret < 0) {
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
    printf("unref &packet\n");
    // if(&packet) {
    //     av_packet_unref(&packet);
    // }
    printf("av_frame_free\n");
    if(frame){
        av_frame_free(&frame);
    }
    //close_input(); => moved to every file
    close_output();

    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));

#ifdef DEBUG_DISPLAY_SCREEN
    SDL_PollEvent(&event);
    switch(event.type) {
        case SDL_QUIT:
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(screen);
            SDL_CloseAudio();

            SDL_Quit();
            exit(0);
            break;
        default:
            break;
    }
#endif

    return ret ? 1 : 0;
}
#endif
