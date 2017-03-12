int FX_concat(int nb_files, char **input_files, char *output_file, void (*callback)(int));

/*

VARIABLES to be reset/refactored

static AVFormatContext *ifmt_ctx;
static AVFormatContext *ofmt_ctx;
static int64_t video_last_pts= 0, video_last_dts= 0, audio_last_pts= 0, audio_last_dts= 0;
static int64_t video_pts= 0, video_dts= 0, audio_pts= 0, audio_dts= 0;

unsigned int displayWidth = 0, displayHeight = 0;
int o_video_st_id = -1, o_audio_st_id = -1;
int i_video_st_id = -1, i_audio_st_id = -1;

FUNCTIONS to be refactored

static int open_input_file(const char *filename);
    static int open_input_file(const char *filename, AVFormatContext **ifmt_ctx, int *i_audio_st_id, int *i_video_st_id);

static int open_output_file(const char *filename);
    static int open_output_file(const char *filename, AVFormatContext *ifmt_ctx, AVFormatContext **ofmt_ctx, int *o_audio_st_id, int *o_video_st_id);

static int encode_write_frame(AVFrame *filt_frame, unsigned int i_stream_index, int *got_frame);
    int encode_write_frame(AVFormatContext *ofmt_ctx, int type, AVFrame *filt_frame,
        unsigned int i_stream_index, unsigned int o_stream_index, int *got_frame);

static int encode_write_frame_flush(unsigned int stream_index, int *got_frame);
    int encode_write_frame(AVFormatContext *ofmt_ctx, int type,
    unsigned int o_stream_index, int *got_frame);

static int flush_encoder(unsigned int stream_index);
    int flush_encoder(AVFormatContext **ofmt_ctx, unsigned int stream_index);

int close_input() ;
    int close_input(AVFormatContext **ifmt_ctx);

int close_output() ;
    int close_output(AVFormatContext **ofmt_ctx);
*/
