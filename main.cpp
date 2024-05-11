#include <stdio.h>

#define __STDC_CONSTANT_MACROS
// �ú�ȷ����C++������ʹ��FFmpeg��ʱ�ļ�����

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}
// ��Ϊffmpegʹ��C����д�ģ���C++��ʹ����Ҫ����extern "C"ȷ����ȷ����

int main(int argc, char* argv[])
{
    AVFormatContext *pFormatCtx;                            // ���ڴ洢��ý���ļ��ĸ�ʽ��������Ϣ
    int audioindex;                                         // ���ڴ洢��Ƶ��������
    AVCodecContext *pCodecCtx;                              // ������������ģ��洢��Ƶ���ı������Ϣ
    const AVCodec *pCodec;                                  // ��������ṹ�壬��ʾ�ҵ�����Ƶ�������
    AVFrame *pFrame;                                        // ���ڴ洢ԭʼ֡����
    AVPacket *packet;                                       // ���ڴ洢��ȡ�������ݰ�
    struct SwrContext *swr_ctx;                             // ��Ƶ�ز��������ģ�����ת����Ƶ��ʽ
    AVCodecParameters *pCodecPar;                           // ���ڴ洢����Ƶ���ı���������������������͡������ʵ���Ϣ
    char filepath[] = "input.mp3";                          // �����ļ�·��
    char out_filepath[] = "output.pcm";                     // ����ļ�·��
    FILE *out_file;                                         // ����ļ�ָ��

    // �������ļ�����ȡ���ʽ������
    pFormatCtx = avformat_alloc_context();                  // ���䲢��ʼ��һ��AVFormatContext�ṹ
    if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0) {  // �������ļ������pFormatCtx
        printf("Couldn't open input stream.\n");
        return -1;
    }

    // ���Ҷ�ý���ļ�������Ϣ
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {  // ��ȡ��ý���ļ�����Ϣ�����pFormatCtx������Ϣ
        printf("Couldn't find stream information.\n");
        return -1;
    }

    // ������Ƶ��������
    audioindex = -1;
    for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++) {          // ������������������Ƶ������
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioindex = i;
            break;
        }
    }
    if (audioindex == -1) {
        printf("Didn't find an audio stream.\n");
        return -1;
    }

    // ��ȡ��Ƶ���ı�������
    pCodecPar = pFormatCtx->streams[audioindex]->codecpar;

    // �ҵ���Ӧ�ı������
    pCodec = avcodec_find_decoder(pCodecPar->codec_id);     // �ҵ���Ӧ�ı������
    if (pCodec == NULL) {
        printf("Codec not found.\n");
        return -1;
    }

    // ���䲢��ʼ���������������
    pCodecCtx = avcodec_alloc_context3(pCodec);             // ���䲢��ʼ���������������
    if (avcodec_parameters_to_context(pCodecCtx, pCodecPar) < 0) { // �������������Ƶ�������
        printf("Failed to copy codec parameters to codec context.\n");
        return -1;
    }

    // �򿪱������
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {       // �򿪱������
        printf("Could not open codec.\n");
        return -1;
    }

    // ��ȡ��Ƶ����������ͨ������
    AVChannelLayout in_ch_layout = AV_CHANNEL_LAYOUT_STEREO; // ����ͨ������
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO; // ���ͨ������


    if (pCodecCtx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
        av_channel_layout_default(&in_ch_layout, 2); 
    } else {
        av_channel_layout_copy(&in_ch_layout, &pCodecCtx->ch_layout);
    }

    // ��ʼ��SwrContext�����ز���
    swr_ctx = swr_alloc();                                  // ���䲢��ʼ����Ƶ�ز���������
    av_opt_set_chlayout(swr_ctx, "in_chlayout", &in_ch_layout, 0); // ��������ͨ������
    av_opt_set_chlayout(swr_ctx, "out_chlayout", &out_ch_layout, 0); // �������ͨ������
    av_opt_set_int(swr_ctx, "in_sample_rate", pCodecCtx->sample_rate, 0); // �������������
    av_opt_set_int(swr_ctx, "out_sample_rate", 44100, 0);  // �������������
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", pCodecCtx->sample_fmt, 0); // �������������ʽ
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0); // �������������ʽ
    swr_init(swr_ctx);                                                         // ��ʼ����Ƶ�ز���������

    // ����֡�Ͱ��ṹ
    pFrame = av_frame_alloc();                     // ���䲢��ʼ��һ��AVFrame�ṹ���ڴ洢ԭʼ��Ƶ֡����
    packet = av_packet_alloc();                    // ���䲢��ʼ��һ��AVPacket�ṹ���ڴ洢��ȡ�������ݰ�

    // ������ļ�
    out_file = fopen(out_filepath, "wb");          // ��дģʽ������ļ�
    if (!out_file) {
        printf("Could not open output file.\n");
        return -1;
    }

    // ��ʼ������Ƶ֡�����и�ʽת��
    while (av_read_frame(pFormatCtx, packet) >= 0) {                       // �����ȡ���ݰ�
        if (packet->stream_index == audioindex) {
            int ret = avcodec_send_packet(pCodecCtx, packet);              // �����ݰ����͵��������������
            if (ret < 0) {
                printf("Error sending a packet for decoding: %d\n", ret);
                continue;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(pCodecCtx, pFrame);            // �ӽ������н��ս�����֡
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    printf("Error during decoding: %d\n", ret);
                    return -1;
                }

                // �ز�����д������ļ�
                uint8_t *out_buf[2];                                       // ���ڴ洢�ز��������Ƶ����
                int out_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, pCodecCtx->sample_rate) + pFrame->nb_samples, 44100, pCodecCtx->sample_rate, AV_ROUND_UP); // ���������������
                int out_linesize;
                av_samples_alloc(out_buf, &out_linesize, 2, out_nb_samples, AV_SAMPLE_FMT_S16, 0); // Ϊ��������������ڴ�
                out_nb_samples = swr_convert(swr_ctx, out_buf, out_nb_samples, (const uint8_t **)pFrame->data, pFrame->nb_samples); // ��ԭʼ֡ת��Ϊ�����ʽ
                fwrite(out_buf[0], 1, out_nb_samples * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * 2, out_file); // д������ļ�
                av_freep(&out_buf[0]);                                     // �ͷ�����������ڴ�
            }
        }
        av_packet_unref(packet);                                           // �ͷ����ݰ�
    }

    // ������Դ
    swr_free(&swr_ctx);                                                     // �ͷ���Ƶ�ز���������
    fclose(out_file);                                                       // �ر�����ļ�
    av_frame_free(&pFrame);                                                 // �ͷ�pFrame�ṹ
    av_packet_free(&packet);                                                // �ͷ�packet�ṹ
    avcodec_free_context(&pCodecCtx);                                       // �ͷű������������
    avformat_close_input(&pFormatCtx);                                      // �ر������ļ����ͷ�pFormatCtx�ṹ

    av_channel_layout_uninit(&in_ch_layout);                                // �ͷ�����ͨ������
    av_channel_layout_uninit(&out_ch_layout);                               // �ͷ����ͨ������

    return 0;
}
