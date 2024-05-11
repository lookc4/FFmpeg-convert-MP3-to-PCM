#include <stdio.h>

#define __STDC_CONSTANT_MACROS
// 该宏确保在C++程序中使用FFmpeg库时的兼容性

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}
// 因为ffmpeg使用C语言写的，在C++中使用需要声明extern "C"确保正确引用

int main(int argc, char* argv[])
{
    AVFormatContext *pFormatCtx;                            // 用于存储多媒体文件的格式上下文信息
    int audioindex;                                         // 用于存储音频流的索引
    AVCodecContext *pCodecCtx;                              // 编解码器上下文，存储音频流的编解码信息
    const AVCodec *pCodec;                                  // 编解码器结构体，表示找到的音频编解码器
    AVFrame *pFrame;                                        // 用于存储原始帧数据
    AVPacket *packet;                                       // 用于存储读取到的数据包
    struct SwrContext *swr_ctx;                             // 音频重采样上下文，用于转换音频格式
    AVCodecParameters *pCodecPar;                           // 用于存储音视频流的编解码参数，例如编码器类型、比特率等信息
    char filepath[] = "input.mp3";                          // 输入文件路径
    char out_filepath[] = "output.pcm";                     // 输出文件路径
    FILE *out_file;                                         // 输出文件指针

    // 打开输入文件并获取其格式上下文
    pFormatCtx = avformat_alloc_context();                  // 分配并初始化一个AVFormatContext结构
    if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0) {  // 打开输入文件并填充pFormatCtx
        printf("Couldn't open input stream.\n");
        return -1;
    }

    // 查找多媒体文件的流信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {  // 读取多媒体文件的信息，填充pFormatCtx的流信息
        printf("Couldn't find stream information.\n");
        return -1;
    }

    // 查找音频流的索引
    audioindex = -1;
    for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++) {          // 遍历所有流，查找音频流索引
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioindex = i;
            break;
        }
    }
    if (audioindex == -1) {
        printf("Didn't find an audio stream.\n");
        return -1;
    }

    // 获取音频流的编解码参数
    pCodecPar = pFormatCtx->streams[audioindex]->codecpar;

    // 找到相应的编解码器
    pCodec = avcodec_find_decoder(pCodecPar->codec_id);     // 找到对应的编解码器
    if (pCodec == NULL) {
        printf("Codec not found.\n");
        return -1;
    }

    // 分配并初始化编解码器上下文
    pCodecCtx = avcodec_alloc_context3(pCodec);             // 分配并初始化编解码器上下文
    if (avcodec_parameters_to_context(pCodecCtx, pCodecPar) < 0) { // 将编解码参数复制到上下文
        printf("Failed to copy codec parameters to codec context.\n");
        return -1;
    }

    // 打开编解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {       // 打开编解码器
        printf("Could not open codec.\n");
        return -1;
    }

    // 获取音频的输入和输出通道布局
    AVChannelLayout in_ch_layout = AV_CHANNEL_LAYOUT_STEREO; // 输入通道布局
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO; // 输出通道布局


    if (pCodecCtx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
        av_channel_layout_default(&in_ch_layout, 2); 
    } else {
        av_channel_layout_copy(&in_ch_layout, &pCodecCtx->ch_layout);
    }

    // 初始化SwrContext用于重采样
    swr_ctx = swr_alloc();                                  // 分配并初始化音频重采样上下文
    av_opt_set_chlayout(swr_ctx, "in_chlayout", &in_ch_layout, 0); // 设置输入通道布局
    av_opt_set_chlayout(swr_ctx, "out_chlayout", &out_ch_layout, 0); // 设置输出通道布局
    av_opt_set_int(swr_ctx, "in_sample_rate", pCodecCtx->sample_rate, 0); // 设置输入采样率
    av_opt_set_int(swr_ctx, "out_sample_rate", 44100, 0);  // 设置输出采样率
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", pCodecCtx->sample_fmt, 0); // 设置输入采样格式
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0); // 设置输出采样格式
    swr_init(swr_ctx);                                                         // 初始化音频重采样上下文

    // 分配帧和包结构
    pFrame = av_frame_alloc();                     // 分配并初始化一个AVFrame结构用于存储原始音频帧数据
    packet = av_packet_alloc();                    // 分配并初始化一个AVPacket结构用于存储读取到的数据包

    // 打开输出文件
    out_file = fopen(out_filepath, "wb");          // 以写模式打开输出文件
    if (!out_file) {
        printf("Could not open output file.\n");
        return -1;
    }

    // 开始解码音频帧并进行格式转换
    while (av_read_frame(pFormatCtx, packet) >= 0) {                       // 逐个读取数据包
        if (packet->stream_index == audioindex) {
            int ret = avcodec_send_packet(pCodecCtx, packet);              // 将数据包发送到编解码器上下文
            if (ret < 0) {
                printf("Error sending a packet for decoding: %d\n", ret);
                continue;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(pCodecCtx, pFrame);            // 从解码器中接收解码后的帧
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    printf("Error during decoding: %d\n", ret);
                    return -1;
                }

                // 重采样并写入输出文件
                uint8_t *out_buf[2];                                       // 用于存储重采样后的音频数据
                int out_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, pCodecCtx->sample_rate) + pFrame->nb_samples, 44100, pCodecCtx->sample_rate, AV_ROUND_UP); // 计算输出样本数量
                int out_linesize;
                av_samples_alloc(out_buf, &out_linesize, 2, out_nb_samples, AV_SAMPLE_FMT_S16, 0); // 为输出缓冲区分配内存
                out_nb_samples = swr_convert(swr_ctx, out_buf, out_nb_samples, (const uint8_t **)pFrame->data, pFrame->nb_samples); // 将原始帧转换为输出格式
                fwrite(out_buf[0], 1, out_nb_samples * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * 2, out_file); // 写入输出文件
                av_freep(&out_buf[0]);                                     // 释放输出缓冲区内存
            }
        }
        av_packet_unref(packet);                                           // 释放数据包
    }

    // 清理资源
    swr_free(&swr_ctx);                                                     // 释放音频重采样上下文
    fclose(out_file);                                                       // 关闭输出文件
    av_frame_free(&pFrame);                                                 // 释放pFrame结构
    av_packet_free(&packet);                                                // 释放packet结构
    avcodec_free_context(&pCodecCtx);                                       // 释放编解码器上下文
    avformat_close_input(&pFormatCtx);                                      // 关闭输入文件并释放pFormatCtx结构

    av_channel_layout_uninit(&in_ch_layout);                                // 释放输入通道布局
    av_channel_layout_uninit(&out_ch_layout);                               // 释放输出通道布局

    return 0;
}
