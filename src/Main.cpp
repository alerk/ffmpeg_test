extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}
#include "../include/FFMpegManager.h"
#include <stdio.h>

// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif
int main(int argc, char* argv[])
{
    if(argc<3)
    {
        fprintf(stderr, "Please call with enough param\n");
        fprintf(stderr, "inputFile1 inputFile2 outputFile");
        return -1;
    }
    char *inputFiles[] = {argv[1], argv[2]};
    concatenateVideos(inputFiles, argv[3]);

    printf("Program ended!\n");    
    return 0;
}
#if 0
void saveFrame(AVFrame *pFrame, int width, int height, int iFrame);
int test_main(char* argv[])
{
    int i, videoStream;
    AVCodec *pCodec = NULL;
    AVFormatContext *pFormatCtx = NULL;
    AVFrame *pFrame;
    AVFrame *pFrameRGB;
    uint8_t *buffer = NULL;
    int numBytes;
    struct SwsContext *sws_ctx = NULL;
    int frameFinished;
    AVPacket packet;

    av_register_all();

    //Open file
    if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL)!=0)
    {
        fprintf(stderr, "Couldn't open file\n");
        return -1;
    }

    //Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL)<0)
    {
        fprintf(stderr, "Couldn't find stream information\n");
        return -1;
    }

    //Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, argv[1], 0);

    AVCodecContext *pCodecCtxOrig = NULL;
    AVCodecContext *pCodecCtx = NULL;
    //Find the first video stream
    videoStream = -1;
    for(i=0; i<pFormatCtx->nb_streams; i++)
    {
        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
        {
            videoStream = i;
            break;
        }
    }

    if(videoStream==-1)
    {
        fprintf(stderr, "Didn't find a video stream\n");
        return -1;
    }

    //Get a pointer to the codec context for the video stream
    pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;

    //Find the decoder for the video stream
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if(pCodec == NULL)
    {
        fprintf(stderr, "Unsupported codec\n");
        return -1;
    }

    //Copy context
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig)!=0) 
    {
        fprintf(stderr, "Couldn't copy codec context\n");
        return -1;
    }

    //Open codec
    if(avcodec_open2(pCodecCtx, pCodec, NULL)<0)
    {
        fprintf(stderr, "Couldn't open codec context\n");
        return -1;
    }

    //Allocate video frame
    pFrame = av_frame_alloc();

    //Stored in 24-bit RGB, convert frame from its native to RGB
    //Allocate an AVFrame struture
    pFrameRGB = av_frame_alloc();
    if(pFrameRGB == NULL)
    {
        fprintf(stderr, "Couldn't alloc frame structure\n");
        return -1;
    }

    //Place to store data while converting
    //av_malloc only wrapper of malloc, no protection for leak, double freeing...
    numBytes = avpicture_get_size(PIX_FMT_RGB24, pCodecCtx->width, 
        pCodecCtx->height);
    buffer = (uint8_t*)av_malloc(numBytes*sizeof(uint8_t));

    //Assign appropriate parts of buffer to image plans in pFrameRGB
    // pFrameRGB is an AVFrame, but AVFrame is a superset of AVPicture
    avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB24, 
        pCodecCtx->width, pCodecCtx->height);

    //Now we're ready to read from the stream!
    //Reading the data
    //init SWS context for software scaling;    
    sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
        pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
        PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);
    while(av_read_frame(pFormatCtx, &packet)>=0)
    {
        //Is this packet from the video stream
        if(packet.stream_index == videoStream)
        {
            //Decode video frame
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
            //Did we get a video frame?
            if(frameFinished)
            {
                sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data, pFrame->linesize,
                0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);

                //Save the frame to disk
                if(++i<=5)
                {
                    saveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);
                }
            }
        }
        //Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);
    }
    av_free(buffer);
    av_free(pFrameRGB);

    //Free the YUV frame
    av_free(pFrame);
    
    //Close the codecs
    avcodec_close(pCodecCtx);
    avcodec_close(pCodecCtxOrig);

    //Close the file video
    avformat_close_input(&pFormatCtx);

	printf("Program ended!\n");
	return 0;
}

void saveFrame(AVFrame *pFrame, int width, int height, int iFrame)
{
    FILE *pFile;
    char szFilename[32];
    int y;
    //Open file
    sprintf(szFilename, "frame%d.ppm", iFrame);
    pFile = fopen(szFilename, "wb");
    if(NULL == pFile)
    {
        fprintf(stderr, "Cannot open file %s\n", szFilename);
    }
    //Write header
    fprintf(pFile, "P6\n%d %d\n255\n", width, height);

    //Write the pixel data
    for(y=0; y<height; y++)
    {
        fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
    }
    //Close file
    fclose(pFile);
}
#endif
