#include "encodethread.h"
#include <QDebug>
#include <QFile>

#ifdef Q_OS_MAC
#define IN_FILE "/Users/caitou/Desktop/outxx.pcm"
#define OUT_FILE "/Users/caitou/Desktop/out.aac"
#else
#define IN_FILE "C:\\Workspaces\\in.pcm"
#define OUT_FILE "C:\\Workspaces\\out.pcm"
#endif

#define ERROR_BUF(res) \
    char errbuf[1024]; \
    av_strerror(res, errbuf, sizeof(errbuf)); \

EncodeThread::EncodeThread(QObject *parent) : QThread(parent) {
    connect(this, &QThread::finished, this, &QThread::deleteLater);
}

EncodeThread::~EncodeThread() {
    disconnect();
    requestInterruption();
    quit();
    wait();

    qDebug() << "EncodeThread::~EncodeThread()";
}

static int check_sample_fmt(const AVCodec *codec,
                            enum AVSampleFormat sample_fmt)
{
    const enum AVSampleFormat *p = codec->sample_fmts;

    while (*p != AV_SAMPLE_FMT_NONE) {
        qDebug() << "fmt: " << av_get_sample_fmt_name(*p);
        if (*p == sample_fmt)
            return 1;
        p++;
    }
    return 0;
}

int audio_encode(AVCodecContext *ctx, AVFrame *frame, AVPacket *packet, QFile &out) {
    /// 这里只需要 send 一次（全量）
    int res = avcodec_send_frame(ctx, frame);
    if (res < 0) {
        ERROR_BUF(res);
        qDebug() << "avcodec_send_frame err:" << errbuf;
        return res;
    }
    qDebug() << "avcodec_send_frame success";

    /* read all the available output packets (in general there may be any
         * number of them */
    /// 这里需要批量的 receive？
    while (true) {
        res = avcodec_receive_packet(ctx, packet);
        // EAGAIN: output is not available in the current state - user must try to send input
        // AVERROR_EOF: the encoder has been fully flushed, and there will be no more output packets
        // 退出函数，重新走 send 流程
        if (res == AVERROR(EAGAIN) || res == AVERROR_EOF) {
           return 0;
        } else if (res < 0) {
           ERROR_BUF(res);
           qDebug() << "avcodec_receive_packet error" << errbuf;
           return res;
        }
        qDebug() << "avcodec_receive_packet success packet size: " << packet->size;
        out.write((char *) packet->data, packet->size);

        // 释放资源
        av_packet_unref(packet);
    }
}

/// PCM => AAC
void EncodeThread::run() {
    AudioEncodeSpec inSpec;
    inSpec.sample_rate = 44100;
    inSpec.channel_layout = AV_CHANNEL_LAYOUT_STEREO;
    inSpec.fmt = AV_SAMPLE_FMT_S16;
    inSpec.file = IN_FILE;

    const AVCodec *codec = nullptr;
    AVCodecContext *ctx = nullptr;
    AVFrame *frame = nullptr;
    AVPacket *packet = nullptr;
    QFile pcm(IN_FILE);
    QFile aac(OUT_FILE);
    int res = 0;

    codec = avcodec_find_encoder_by_name("libfdk_aac");
    if (codec == nullptr) {
        qDebug() << "avcodec_find_encoder_by_name failure";
        return;
    }

    qDebug() << codec->name;

    if (!check_sample_fmt(codec, inSpec.fmt)) {
        qDebug() << "check_sample_fmt not support pcm fmt" << av_get_sample_fmt_name(inSpec.fmt);
        goto end;
    }

    ctx = avcodec_alloc_context3(codec);
    if (ctx == nullptr) {
        qDebug() << "avcodec_alloc_context3 failure";
        return;
    }

    /// 配置
    ctx->sample_rate = inSpec.sample_rate;
    ctx->ch_layout = inSpec.channel_layout;
    ctx->sample_fmt = inSpec.fmt;
    // 配置输出参数
    ctx->bit_rate = 32000; // 比特率
    ctx->profile = FF_PROFILE_AAC_HE_V2; // 规格
    if (avcodec_open2(ctx, codec, nullptr) < 0) {
        ERROR_BUF(res);
        qDebug() << "avcodec_open2 error" << errbuf;
        goto end;
    }

    if (pcm.open(QFile::ReadOnly) == 0) {
        qDebug() << "pcm open failure file: " << inSpec.file;
        goto end;
    }

    if (aac.open(QFile::WriteOnly) == 0) {
        qDebug() << "aac open failure file: " << OUT_FILE;
        goto end;
    }

    /* frame containing input raw audio */
    frame = av_frame_alloc();
    if (frame == nullptr) {
        qDebug() << "av_frame_alloc failure";
        goto end;
    }

    //  frame->nb_samples = 1024;
    // 样本帧数量（由frame_size决定）
    frame->nb_samples = ctx->frame_size;
    frame->ch_layout = inSpec.channel_layout;
    frame->format = inSpec.fmt;

    /* allocate the data buffers */
    /**
     * Allocate new buffer(s) for audio or video data.
     *
     * The following fields must be set on frame before calling this function:
     * - format (pixel format for video, sample format for audio)
     * - width and height for video
     * - nb_samples and ch_layout for audio
     *
     * This function will fill AVFrame.data and AVFrame.buf arrays and, if
     * necessary, allocate and fill AVFrame.extended_data and AVFrame.extended_buf.
     * For planar formats, one buffer will be allocated for each plane.
     *
     * @warning: if frame already has been allocated, calling this function will
     *           leak memory. In addition, undefined behavior can occur in certain
     *           cases.
     *
     * @param frame frame in which to store the new buffers.
     * @param align Required buffer size alignment. If equal to 0, alignment will be
     *              chosen automatically for the current CPU. It is highly
     *              recommended to pass 0 here unless you know what you are doing.
     *
     * @return 0 on success, a negative AVERROR on error.
     */
    res = av_frame_get_buffer(frame, 0);
    if (res < 0) {
        ERROR_BUF(res);
        qDebug() << "av_frame_get_buffer error" << errbuf;
        goto end;
    }

    packet = av_packet_alloc();
    if (packet == nullptr) {
        qDebug() << "av_packet_alloc failure";
        goto end;
    }

    while ((res == pcm.read((char *) frame->data[0],
                            frame->linesize[0])) > 0) {
        qDebug() << "linesize: " << frame->linesize[0];
        int ret = audio_encode(ctx, frame, packet, aac);
        if (ret < 0) {
            goto end;
        }
    }

    /*
     * It can be NULL, in which case it is considered a flush packet.
     * This signals the end of the stream. If the encoder
     * still has packets buffered, it will return them after this
     * call. Once flushing mode has been entered, additional flush
     * packets are ignored, and sending frames will return
     * AVERROR_EOF.
    */
    audio_encode(ctx, nullptr, packet, aac);
end:
    pcm.close();
    aac.close();
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&ctx);
}
