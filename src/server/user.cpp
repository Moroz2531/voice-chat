#include <cstring>

#include <user.hpp>

using namespace messenger;

bool BaseUser::send(std::span<const char> buf) {
  constexpr size_t MAX_DATA = 1400, REPEAT_SEND = 2;
  if (buf.size()) {
    if (tempbufsend.capacity() >= tempbufsend.size() + buf.size()) {
      if (tempbufsend.empty()) {
        auto bytes = sfd_->send(buf.data(), buf.size(), 0);
        if (bytes == buf.size())
          return true;
        else
          tempbufsend.write(buf.data() + bytes, buf.size() - bytes);
      } else
        tempbufsend.write(buf.data(), buf.size());
    } else
      return false;
  }
  for (int j = 0; j < REPEAT_SEND && tempbufsend.size(); ++j) {
    auto sendBytes = std::min(MAX_DATA, tempbufsend.size());
    auto bytes = sfd_->send(tempbufsend, sendBytes, 0);
    if (bytes != sendBytes) {
      std::memmove(tempbufsend, tempbufsend + bytes, sendBytes - bytes);
      tempbufsend.unsafe_erase_size(bytes);
    } else if (sendBytes == tempbufsend.size()) {
      tempbufsend.clear();
    } else {
      std::memmove(tempbufsend, tempbufsend + sendBytes,
                   tempbufsend.size() - sendBytes);
      tempbufsend.unsafe_erase_size(sendBytes);
    }
  }
  return true;
}

size_t BaseUser::recv(std::span<char> buf, char sep) {
  netsize_t bytes;
  size_t cap{0};
  while ((bytes = sfd_->recv(tempbufrecv + tempbufrecv.size(),
                             tempbufrecv.capacity() - tempbufrecv.size(), 0)) >
         0) {
    tempbufrecv.unsafe_add_size(bytes);
    if (!cap) {
      auto pos = static_cast<char*>(
          std::memchr(tempbufrecv, tempbufrecv.size(), bytes));
      if (pos == nullptr) {
        if (tempbufrecv.capacity() == tempbufrecv.size()) {
          sfd_->close();
          break;
        }
        continue;
      }
      auto tcap = static_cast<size_t>(pos - tempbufrecv);
      if (tcap > buf.size())
        break;
      cap = tcap;
      std::memcpy(buf.data(), tempbufrecv, cap);
      std::memmove(tempbufrecv, tempbufrecv + cap + 1, cap + 1);
    }
  }
  if (!cap && tempbufrecv.capacity() == tempbufrecv.size())
    sfd_->close();
  return cap;
}
