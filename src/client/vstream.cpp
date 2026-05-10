#include <boost/lockfree/spsc_queue.hpp>
#include <chrono>
#include <iostream>
#include <syncstream>

#include <portaudiocpp/AutoSystem.hxx>
#include <portaudiocpp/CppFunCallbackStream.hxx>
#include <portaudiocpp/Device.hxx>
#include <portaudiocpp/StreamParameters.hxx>

#include "client.hpp"

namespace {
template <bool remote = false>
std::string getIPv4(const containers::Socket& sfd) noexcept {
  char buf[INET_ADDRSTRLEN];
  in_addr addr;
  addr.s_addr = sfd.ip<remote>();

  if (inet_ntop(AF_INET, &addr, buf, sizeof(buf)) == NULL) {
    return std::string("printIPv4: error inet_ntop");
  }
  return std::string(std::format("{}/{}", buf, ntohs(sfd.port<remote>())));
}
}  // namespace

using namespace client;

VoiceStream::VoiceStream() {
  sockaddr_in sin;
  std::memset(&sin, 0, sizeof(sockaddr_in));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = 0;

  sfd_.create(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
  sfd_.bind(reinterpret_cast<sockaddr*>(&sin), sizeof(sin));

  epoll_event ev;
  ev.events = EPOLLIN | EPOLLOUT | EPOLLERR;
  ep_.insert(sfd_, ev);
}

void VoiceStream::run(in_addr_t ip, in_port_t port) {
  if (jt_.joinable())
    throw std::runtime_error("voiceStream: stream already is running");
  sockaddr_in sin;
  std::memset(&sin, 0, sizeof(sockaddr_in));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = ip;
  sin.sin_port = port;
  sfd_.connect(reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
  jt_ = std::jthread([&](std::stop_token stok) { runLoop(stok); });
}

void VoiceStream::stop() {
  if (jt_.joinable()) {
    jt_.request_stop();
    jt_.join();
  }
}

bool VoiceStream::joinable() const noexcept {
  return jt_.joinable();
}

void VoiceStream::runLoop(std::stop_token stok) {
  constexpr auto numChannels = 1;
  constexpr auto format = portaudio::FLOAT32;
  constexpr auto sampleRate = 44100;
  constexpr auto framesPerBuffer = 192;
  constexpr auto multFactor = 10;

  using Queue = boost::lockfree::spsc_queue<
      float,
      boost::lockfree::capacity<framesPerBuffer * numChannels * multFactor>>;
  struct UserData {
    int numChannels{};
    float inVolume{1}, outVolume{1};
    std::atomic_bool &inDev, &outDev;
    Queue in{}, out{};
  };

  try {
    portaudio::AutoSystem autoSystem;
    portaudio::System& system = portaudio::System::instance();

    portaudio::Device& outputDevice = system.defaultOutputDevice();
    portaudio::Device& inputDevice = system.defaultInputDevice();

    portaudio::DirectionSpecificStreamParameters outParams{
        outputDevice,
        numChannels,
        format,
        true,
        outputDevice.defaultLowOutputLatency(),
        nullptr};
    portaudio::DirectionSpecificStreamParameters inParams{
        inputDevice,
        numChannels,
        format,
        true,
        inputDevice.defaultHighInputLatency(),
        nullptr};
    portaudio::StreamParameters params{inParams, outParams, sampleRate,
                                       framesPerBuffer,
                                       paClipOff | paDitherOff};

    UserData qs{numChannels, 7, 1, inDev_, outDev_};

    auto callback =
        [](const void* inputBuffer, void* outputBuffer, unsigned long frames,
           [[maybe_unused]] const PaStreamCallbackTimeInfo* timeInfo,
           [[maybe_unused]] PaStreamCallbackFlags statusFlags,
           void* userData) -> int {
      auto in = static_cast<const float*>(inputBuffer);
      auto out = static_cast<float*>(outputBuffer);
      auto ud = static_cast<UserData*>(userData);

      auto inVolume = ud->inVolume, outVolume = ud->outVolume;
      auto &qin = ud->in, &qout = ud->out;

      auto size = frames * ud->numChannels;

      float frame;
      if (inputBuffer && ud->inDev) {
        for (unsigned long i = 0; i < size; ++i) {
          frame = in[i] * inVolume;
          if (frame >= -0.000001 && frame <= 0.000001)
            continue;
          qin.push(frame);
        }
      }
      if (outputBuffer && ud->outDev) {
        for (unsigned long i = 0; i < size; ++i)
          if (qout.pop(frame))
            out[i] = frame * outVolume;
          else
            out[i] = 0.0f;
      }
      return paContinue;
    };

    portaudio::FunCallbackStream stream{params, callback, &qs};
    std::string data;
    epoll_event ev;
    float recv[framesPerBuffer * numChannels * multFactor];

    constexpr auto callFreq = double(framesPerBuffer) / sampleRate;

    stream.start();
    while (!stok.stop_requested()) {
      if (ep_.wait(&ev, 1, 1)) {
        if (ev.events & EPOLLIN) {
          data = sfd_.recv();
          size_t result = 0;
          do {
            auto bytes = result * sizeof(float);
            auto min = std::min(data.length() - bytes, sizeof(recv));
            std::memcpy(recv, data.data() + bytes, min);
            result += qs.out.push(recv, min / sizeof(float));
            if (!qs.out.write_available())
              std::this_thread::yield();
          } while (result * sizeof(float) != data.length());
        }
        if (ev.events & EPOLLOUT) {
          size_t size;
          if ((size = qs.in.pop(recv, sizeof(recv) / sizeof(float)))) {
            size_t result = 0;
            constexpr size_t MAX_SEND_FLOAT = 1458 / sizeof(float);
            do {
              auto count = sfd_.send(
                  recv + result, std::min(size - result, MAX_SEND_FLOAT), 0);
              result += count;
            } while (result != size);
          }
        }
        if (ev.events & EPOLLERR) {
          std::cout << "voiceStream (loop): epoll return EPOLLERR\n";
          break;
        }
      }
      std::this_thread::sleep_for(std::chrono::duration<double>(callFreq));
    }
  } catch (const std::exception& e) {
    std::osyncstream(std::cerr) << e.what() << '\n';
  }
}
