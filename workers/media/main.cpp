// Linux-only isolated media worker. No gameplay, database or network APIs.
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/log.h>
}
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string_view>

namespace {
constexpr std::int64_t MaxPixels = 33554432;
constexpr std::int64_t MaxSeconds = 86400;
constexpr int Strict = AV_EF_CRCCHECK | AV_EF_BITSTREAM | AV_EF_BUFFER | AV_EF_EXPLODE;
constexpr const char* Input = "/input";

struct Format {
    AVFormatContext* value = nullptr;
    ~Format() { if (value) avformat_close_input(&value); }
};
struct Codec {
    AVCodecContext* value = nullptr;
    ~Codec() { avcodec_free_context(&value); }
};
struct Packet {
    AVPacket* value = av_packet_alloc();
    ~Packet() { av_packet_free(&value); }
};
struct Frame {
    AVFrame* value = av_frame_alloc();
    ~Frame() { av_frame_free(&value); }
};

bool Header(std::string_view format) {
    std::ifstream file(Input, std::ios::binary | std::ios::ate);
    const auto size = file.tellg();
    const bool audio = format == "wav" || format == "mp3";
    if (size < 12 || size > (audio ? 256LL : 24LL) * 1024 * 1024) return false;
    file.seekg(0);
    std::array<unsigned char, 12> head{}, tail{};
    if (!file.read(reinterpret_cast<char*>(head.data()), head.size())) return false;
    if (format == "wav") {
        // Only ordinary RIFF/WAVE, not RF64 or streaming placeholder sizes.
        const auto bytes = static_cast<std::uint32_t>(head[4]) |
            (static_cast<std::uint32_t>(head[5]) << 8) |
            (static_cast<std::uint32_t>(head[6]) << 16) |
            (static_cast<std::uint32_t>(head[7]) << 24);
        return std::string_view(reinterpret_cast<const char*>(head.data()), 4) == "RIFF" &&
            std::string_view(reinterpret_cast<const char*>(head.data()+8), 4) == "WAVE" &&
            static_cast<std::int64_t>(bytes) + 8 == static_cast<std::int64_t>(size);
    }
    if (format == "mp3") return true; // forced demuxer and codec, no extension probing
    if (format == "png") {
        const std::array<unsigned char,8> signature{137,80,78,71,13,10,26,10};
        for (std::size_t i=0; i<signature.size(); ++i) if (head[i]!=signature[i]) return false;
        file.seekg(-12, std::ios::end);
        return bool(file.read(reinterpret_cast<char*>(tail.data()),tail.size())) &&
            tail == std::array<unsigned char,12>{0,0,0,0,'I','E','N','D',0xae,0x42,0x60,0x82};
    }
    file.seekg(-2, std::ios::end);
    return head[0]==0xff && head[1]==0xd8 && head[2]==0xff &&
        bool(file.read(reinterpret_cast<char*>(tail.data()),2)) && tail[0]==0xff && tail[1]==0xd9;
}

bool AllowedCodec(std::string_view format, AVCodecID id) {
    if (format=="mp3") return id==AV_CODEC_ID_MP3;
    if (format=="png") return id==AV_CODEC_ID_PNG;
    if (format=="jpeg") return id==AV_CODEC_ID_MJPEG;
    return id==AV_CODEC_ID_PCM_U8 || id==AV_CODEC_ID_PCM_S16LE ||
        id==AV_CODEC_ID_PCM_S24LE || id==AV_CODEC_ID_PCM_S32LE || id==AV_CODEC_ID_PCM_F32LE;
}

struct Decoded {
    bool audio = false;
    std::int64_t samples = 0;
    int rate = 0, channels = 0, width = 0, height = 0, images = 0;
    bool Add(const AVFrame& f) {
        if ((f.flags & AV_FRAME_FLAG_CORRUPT) || f.decode_error_flags) return false;
        if (audio) {
            const auto n = f.ch_layout.nb_channels;
            if (f.nb_samples <= 0 || f.sample_rate < 8000 || f.sample_rate > 192000 ||
                n < 1 || n > 2) return false;
            if (!rate) { rate = f.sample_rate; channels = n; }
            if (rate != f.sample_rate || channels != n ||
                f.nb_samples > MaxSeconds * rate - samples) return false;
            samples += f.nb_samples;
            return true;
        }
        if (++images != 1 || f.width < 1 || f.width > 8192 || f.height < 1 ||
            f.height > 8192 || static_cast<std::int64_t>(f.width)*f.height > MaxPixels) return false;
        width = f.width; height = f.height;
        return true;
    }
};

bool Decode(std::string_view kind, Decoded& result) {
    if (!Header(kind)) return false;
    result.audio = kind=="wav" || kind=="mp3";
    const auto* demux = av_find_input_format(kind=="png" ? "png_pipe" :
        kind=="jpeg" ? "jpeg_pipe" : kind=="wav" ? "wav" : "mp3");
    if (!demux) return false;
    Format input;
    input.value = avformat_alloc_context();
    if (!input.value) return false;
    input.value->max_streams = 1;
    input.value->error_recognition = Strict;
    AVDictionary* options = nullptr;
    if (av_dict_set(&options, "protocol_whitelist", "file", 0)<0) {
        av_dict_free(&options); return false;
    }
    const int opened = avformat_open_input(&input.value, Input, demux, &options);
    const bool unused = av_dict_count(options)!=0;
    av_dict_free(&options);
    if (opened<0 || unused || input.value->nb_streams!=1) return false;
    const auto* params = input.value->streams[0]->codecpar;
    if (params->codec_type != (result.audio ? AVMEDIA_TYPE_AUDIO : AVMEDIA_TYPE_VIDEO) ||
        !AllowedCodec(kind,params->codec_id)) return false;
    const auto* decoder = avcodec_find_decoder(params->codec_id);
    if (!decoder) return false;
    Codec codec;
    codec.value = avcodec_alloc_context3(decoder);
    if (!codec.value || avcodec_parameters_to_context(codec.value,params)<0) return false;
    codec.value->thread_count = 1;
    codec.value->err_recognition = Strict;
    codec.value->max_pixels = MaxPixels;
    if (avcodec_open2(codec.value,decoder,nullptr)<0) return false;
    Packet packet;
    Frame frame;
    if (!packet.value || !frame.value) return false;
    const auto drain = [&](bool flushing) {
        while (true) {
            const int ret = avcodec_receive_frame(codec.value,frame.value);
            if (ret==AVERROR(EAGAIN)) return !flushing;
            if (ret==AVERROR_EOF) return flushing;
            if (ret<0 || !result.Add(*frame.value)) return false;
            av_frame_unref(frame.value);
        }
    };
    int ret = 0;
    while ((ret=av_read_frame(input.value,packet.value))>=0) {
        if (packet.value->stream_index!=0 || (packet.value->flags & AV_PKT_FLAG_CORRUPT) ||
            avcodec_send_packet(codec.value,packet.value)<0 || !drain(false)) return false;
        av_packet_unref(packet.value);
    }
    if (ret!=AVERROR_EOF || (input.value->pb && input.value->pb->error<0) ||
        avcodec_send_packet(codec.value,nullptr)<0 || !drain(true)) return false;
    return result.audio ? result.samples>0 : result.images==1;
}
}

int main(int argc,char** argv) {
    if (argc!=2) return 64;
    const std::string_view kind=argv[1];
    if (kind!="wav" && kind!="mp3" && kind!="png" && kind!="jpeg") return 64;
    av_log_set_level(AV_LOG_ERROR);
    Decoded result;
    if (!Decode(kind,result)) {
        std::cout<<"{\"protocolVersion\":1,\"ok\":false}\n";
        return 2;
    }
    std::cout<<"{\"protocolVersion\":1,\"ok\":true,\"format\":\""<<kind<<"\"";
    if (result.audio)
        std::cout<<",\"sampleFrames\":"<<result.samples<<",\"sampleRate\":"<<result.rate
                 <<",\"channels\":"<<result.channels;
    else std::cout<<",\"width\":"<<result.width<<",\"height\":"<<result.height;
    std::cout<<"}\n";
}
