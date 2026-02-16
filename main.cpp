
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include <FLAC/all.h>
#include <gme/gme.h>

namespace fs = std::filesystem;

std::string pad(std::string value) {
    unsigned size = 8;
    std::string result = value;
    while (result.size() < size) {
        result = "0" + result;
    }
    return result;
}

class Audio {
    protected:
        std::vector<int> m_raw;
        unsigned m_sampleRate;
        unsigned m_totalChannel;
    protected:
        unsigned m_track;
        std::string m_title;
    public:
        Audio(std::vector<int> raw, unsigned sampleRate, unsigned totalChannel) : m_raw(raw), m_sampleRate(sampleRate), m_totalChannel(totalChannel) {
        }

        ~Audio() {
        }

        void SetTrack(unsigned track) {
            m_track = track;
        }

        void SetTitle(std::string title) {
            m_title = title;
        }

        void save(std::string filename) const {
            // Step 1. Prepare FLAC audio stream
            std::vector<uint8_t> flac;
            int sampleCount = m_raw.size() / m_totalChannel;

            std::string write = "TITLE=" + m_title;

            FLAC__StreamMetadata_VorbisComment_Entry key{(uint32_t)write.size(), (uint8_t*)write.data()};
            FLAC__StreamMetadata* meta = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
            FLAC__metadata_object_vorbiscomment_append_comment(meta, key, true);

            FLAC__StreamEncoder* encoder = FLAC__stream_encoder_new();
            FLAC__stream_encoder_set_verify(encoder, true);
            FLAC__stream_encoder_set_compression_level(encoder, 5);
            FLAC__stream_encoder_set_channels(encoder, m_totalChannel);
            FLAC__stream_encoder_set_bits_per_sample(encoder, 16);
            FLAC__stream_encoder_set_sample_rate(encoder, m_sampleRate);
            FLAC__stream_encoder_set_total_samples_estimate(encoder, sampleCount);
            FLAC__stream_encoder_set_metadata(encoder, &meta, 1);
            FLAC__stream_encoder_init_stream(encoder,
                [](const FLAC__StreamEncoder*, const FLAC__byte buffer[], size_t bytes, unsigned, unsigned, void* client_data) -> FLAC__StreamEncoderWriteStatus {
                    std::vector<uint8_t>* result = (std::vector<uint8_t>*)client_data;
                    copy(buffer, buffer + bytes, back_inserter(*result));
                    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
                }, nullptr, nullptr, nullptr, &flac);
            FLAC__stream_encoder_process_interleaved(encoder, m_raw.data(), sampleCount);
            FLAC__stream_encoder_finish(encoder);
            FLAC__stream_encoder_delete(encoder);
            FLAC__metadata_object_delete(meta);

            // Step 2. Save output
            std::ofstream out;
            out.open(filename, std::ios::binary | std::ios::out);
            out.write((char*)flac.data(), flac.size());
        }

};

class AudioTranscoder {
    protected:
        Music_Emu* emu = nullptr;
        unsigned kSampleRate = 44100;
        unsigned kTotalChannel = 2;
        unsigned kRefreshRate = 60;
        unsigned kSampleCount = kSampleRate / kRefreshRate;
    public:
        AudioTranscoder(std::string filename) {
            gme_open_file(filename.c_str(), &emu, kSampleRate);
            gme_set_stereo_depth(emu, 0.2);
        }

        ~AudioTranscoder() {
            gme_delete(emu);
        }

        unsigned count() const {
            return gme_track_count(emu);
        }

        void info(unsigned track) const {
            gme_info_t* info;
            gme_err_t error;

            error = gme_track_info(emu, &info, 0);
            if (error) {
                std::cerr << "error getting track info" << error << std::endl;
                return;
            }

            std::cout << "system: " << info->system << std::endl;
            std::cout << "game: " << info->game << std::endl;
            std::cout << "song: " << info->song << std::endl;
            std::cout << "author: " << info->author << std::endl;

            std::cout << "length: " << info->length << " msec." << std::endl;
            std::cout << "intro-length: " << info->intro_length << " msec." << std::endl;
            std::cout << "loop-length: " << info->loop_length << " msec." << std::endl;
            std::cout << "play-length: " << info->play_length << " msec." << std::endl;

            gme_free_info(info);
        }

        std::shared_ptr<Audio> convert(unsigned track) {
            info(track);
            static int limit = 3 * 60 * kTotalChannel * kSampleRate;
            std::vector<short> buf(kSampleCount * kTotalChannel, 0);
            std::vector<int> res;
            gme_start_track(emu, track);
            while (res.size() < limit && !gme_track_ended(emu)) {
                gme_play(emu, buf.size(), buf.data());
                copy(buf.begin(), buf.end(), back_inserter(res));
            }
            auto audio = std::make_shared<Audio>(res, kSampleRate, kTotalChannel);
            audio->SetTrack(track);
            audio->SetTitle(std::to_string(track + 1));
            return audio;
        }

};

std::string makeTrackFilename(unsigned track) {
    return pad(std::to_string(track)) + ".flac";
}

int main(int argc, char* argv[]) {

    if (argc != 2) {
        std::cout << "usage: nsf-ripper [input]" << std::endl;
        return 0;
    }

    auto coder = std::make_shared<AudioTranscoder>(argv[1]);
    unsigned total = coder->count();

    fs::path nsf(argv[1]);
    std::filesystem::create_directory(nsf.root_path() / nsf.stem());
    for (unsigned n = 0; n < total; ++n) {

        std::cout << "Transcoding track [" << n + 1 << "/" << total << "]..." << std::flush;

        auto audio = coder->convert(n);

        std::string name = makeTrackFilename(n + 1); // Hm?
        std::string filename = nsf.root_path() / nsf.stem() / name;
        audio->save(filename);

        std::cout << "done." << std::endl;
    }

    return 0;
}
