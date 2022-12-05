#ifndef ENCODETHREAD_H
#define ENCODETHREAD_H

#include <QThread>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

typedef struct {
    AVChannelLayout channel_layout;
    AVSampleFormat fmt;
    int bytesPerSample;
    int sample_rate = 0;
    const char *file;
} AudioEncodeSpec;

class EncodeThread : public QThread
{
    Q_OBJECT
private:
    void run() override;
public:
    EncodeThread(QObject *parent);
    ~EncodeThread();
signals:

};

#endif // ENCODETHREAD_H
