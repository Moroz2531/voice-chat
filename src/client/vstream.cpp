#include <boost/lockfree/spsc_queue.hpp>
#include <chrono>
#include <iostream>
#include <syncstream>

#include <portaudiocpp/AutoSystem.hxx>
#include <portaudiocpp/CppFunCallbackStream.hxx>
#include <portaudiocpp/Device.hxx>
#include <portaudiocpp/StreamParameters.hxx>

#include "client.hpp"

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
  ev.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLET;
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
  constexpr auto framesPerBuffer = 256;
  constexpr auto multFactor = 4;

  struct UserData {
    using Queue = boost::lockfree::spsc_queue<
        float,
        boost::lockfree::capacity<framesPerBuffer * numChannels * multFactor>>;
    int numChannels{};
    double inVolume{}, outVolume{};
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
        false,
        outputDevice.defaultLowOutputLatency(),
        nullptr};
    portaudio::DirectionSpecificStreamParameters inParams{
        inputDevice,
        numChannels,
        format,
        false,
        inputDevice.defaultLowInputLatency(),
        nullptr};
    portaudio::StreamParameters params{inParams, outParams, sampleRate,
                                       framesPerBuffer, paNoFlag};

    UserData qs{numChannels};

    auto callback =
        [](const void* inputBuffer, void* outputBuffer,
           unsigned long framesPerBuffer,
           [[maybe_unused]] const PaStreamCallbackTimeInfo* timeInfo,
           [[maybe_unused]] PaStreamCallbackFlags statusFlags,
           void* userData) -> int {
      auto in = static_cast<const float*>(inputBuffer);
      auto out = static_cast<float*>(outputBuffer);
      auto ud = static_cast<UserData*>(userData);

      auto inVolume = ud->inVolume, outVolume = ud->outVolume;
      auto &qin = ud->in, &qout = ud->out;

      auto size = framesPerBuffer * ud->numChannels;

      for (unsigned long i = 0; i < size; ++i) {
        qin.push(in[i] * inVolume);
        out[i] = qout.pop(out[i]) ? out[i] * outVolume : 0;
      }
      return paContinue;
    };

    portaudio::FunCallbackStream stream{params, callback, &qs};
    std::string data;
    epoll_event ev;
    float recv[framesPerBuffer * numChannels * multFactor];

    constexpr auto callFreq = double(framesPerBuffer) / sampleRate;
    constexpr auto multFactorSleep = 2;
    constexpr auto timesleep = callFreq / multFactorSleep;

    stream.start();
    while (!stok.stop_requested()) {
      if (ep_.wait(&ev, 1, 0)) {
        if (ev.events & EPOLLIN) {
          data = sfd_.recv();
          std::memcpy(&recv, data.data(), data.length());
          qs.out.push(recv, data.length() / sizeof(float));
        }
        if (ev.events & EPOLLOUT) {
          size_t size;
          if ((size = qs.in.pop(recv, sizeof(recv) / sizeof(float))))
            sfd_.send(reinterpret_cast<char*>(recv), size, 0);
        }
        if (ev.events & EPOLLERR)
          throw std::runtime_error("voiceStream (loop): epoll return EPOLLERR");
      }
      std::this_thread::sleep_for(std::chrono::duration<double>(timesleep));
    }
  } catch (const std::exception& e) {
    std::osyncstream(std::cerr) << e.what() << '\n';
  }
}
