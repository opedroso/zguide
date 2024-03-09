//  kvsimple class - key-value message class for example applications

#include "kvsimple.hpp"
#include <iostream>
#include <optional>
#include <sstream>
namespace {
std::optional<zmq::message_t> receive_message(zmq::socket_t &socket) {
  zmq::message_t message(0);
  try {
    if (!socket.recv(message, zmq::recv_flags::none)) {
      return {};
    }
  } catch (zmq::error_t error) {
    std::cerr << "E: " << error.what() << std::endl;
    return {};
  }
  return message;
}
} // namespace

kvmsg::kvmsg(std::string key, int64_t sequence, ustring body)
    : key_(key), body_(body), sequence_(sequence) {}

void kvmsg::clear() {
  key_ = {};
  body_ = {};
  sequence_ = {};
}
//  Reads key-value message from socket, returns new kvmsg instance.
std::optional<kvmsg> kvmsg::recv(zmq::socket_t &socket) {
  kvmsg msg;
  auto key_message = receive_message(socket);
  if (!key_message)
    return {};
  msg.set_key((char *)(*key_message).data());
  auto sequence_message = receive_message(socket);
  msg.set_sequence(*(int64_t *)(*sequence_message).data());
  if (!sequence_message)
    return {};
  auto body_message = receive_message(socket);
  if (!body_message)
    return {};
  msg.set_body((unsigned char *)(*body_message).data());
  return msg;
}

//  Send key-value message to socket; any empty frames are sent as such.
void kvmsg::send(zmq::socket_t &socket) {
  {
    zmq::message_t message;
    message.rebuild(key_.size());
    std::memcpy(message.data(), key_.c_str(), key_.size());
    socket.send(message, zmq::send_flags::sndmore);
  }
  {
    zmq::message_t message;
    message.rebuild(sizeof(sequence_));
    std::memcpy(message.data(), (void *)&sequence_, sizeof(sequence_));
    socket.send(message, zmq::send_flags::sndmore);
  }
  {
    zmq::message_t message;
    message.rebuild(body_.size());
    std::memcpy(message.data(), body_.c_str(), body_.size());
    socket.send(message, zmq::send_flags::none);
  }
}

//  Return key from last read message, if any, else NULL
std::string kvmsg::key() const { return key_; }
//  Return sequence nbr from last read message, if any
int64_t kvmsg::sequence() const { return sequence_; }
//  Return body from last read message, if any, else NULL
ustring kvmsg::body() const { return body_; }
//  Return body size from last read message, if any, else zero
size_t kvmsg::size() const { return body_.size(); }
//  Set message key as provided
void kvmsg::set_key(std::string key) { key_ = key; }
//  Set message sequence number
void kvmsg::set_sequence(int64_t sequence) { sequence_ = sequence; }
//  Set message body
void kvmsg::set_body(ustring body) { body_ = body; }

std::string kvmsg::to_string() {
  std::stringstream ss;
  ss << "key=" << key_ << ",sequence=" << sequence_;
  int is_text = 1;
  for (size_t i = 0; i < body_.size(); ++i) {
    if (body[i] < 32 || data[i] > 127){
      is_text = 0;
      break;
    }
  }
  return {};
}
//  Dump message to stderr, for debugging and tracing

//  Runs self test of class
int kvmsg::test(int verbose) { return 0; }

int main() { return 0; }
//
////  zhash_free_fn callback helper that does the low level destruction:
// void kvmsg_free(void *ptr) {
//   if (ptr) {
//     kvmsg_t *self = (kvmsg_t *)ptr;
//     //  Destroy message frames if any
//     int frame_nbr;
//     for (frame_nbr = 0; frame_nbr < kvmsg_FRAMES; frame_nbr++)
//       if (self->present[frame_nbr])
//         zmq_msg_close(&self->frame[frame_nbr]);
//
//     //  Free object itself
//     free(self);
//   }
// }
//
////  Destructor
// void kvmsg_destroy(kvmsg_t **self_p) {
//   assert(self_p);
//   if (*self_p) {
//     kvmsg_free(*self_p);
//     *self_p = NULL;
//   }
// }
//
////  .split recv method
////  This method reads a key-value message from socket, and returns a new
////  {{kvmsg}} instance:
//
// kvmsg_t *kvmsg_recv(void *socket) {
//  assert(socket);
//  kvmsg_t *self = kvmsg_new(0);
//
//  //  Read all frames off the wire, reject if bogus
//  int frame_nbr;
//  for (frame_nbr = 0; frame_nbr < kvmsg_FRAMES; frame_nbr++) {
//    if (self->present[frame_nbr])
//      zmq_msg_close(&self->frame[frame_nbr]);
//    zmq_msg_init(&self->frame[frame_nbr]);
//    self->present[frame_nbr] = 1;
//    if (zmq_msg_recv(&self->frame[frame_nbr], socket, 0) == -1) {
//      kvmsg_destroy(&self);
//      break;
//    }
//    //  Verify multipart framing
//    int rcvmore = (frame_nbr < kvmsg_FRAMES - 1) ? 1 : 0;
//    if (zsocket_rcvmore(socket) != rcvmore) {
//      kvmsg_destroy(&self);
//      break;
//    }
//  }
//  return self;
//}
//
////  .split send method
////  This method sends a multiframe key-value message to a socket:
//
// void kvmsg_send(kvmsg_t *self, void *socket) {
//  assert(self);
//  assert(socket);
//
//  int frame_nbr;
//  for (frame_nbr = 0; frame_nbr < kvmsg_FRAMES; frame_nbr++) {
//    zmq_msg_t copy;
//    zmq_msg_init(&copy);
//    if (self->present[frame_nbr])
//      zmq_msg_copy(&copy, &self->frame[frame_nbr]);
//    zmq_msg_send(&copy, socket,
//                 (frame_nbr < kvmsg_FRAMES - 1) ? ZMQ_SNDMORE : 0);
//    zmq_msg_close(&copy);
//  }
//}
//
////  .split key methods
////  These methods let the caller get and set the message key, as a
////  fixed string and as a printf formatted string:
//
// char *kvmsg_key(kvmsg_t *self) {
//  assert(self);
//  if (self->present[FRAME_KEY]) {
//    if (!*self->key) {
//      size_t size = zmq_msg_size(&self->frame[FRAME_KEY]);
//      if (size > kvmsg_KEY_MAX)
//        size = KVMSG_KEY_MAX;
//      memcpy(self->key, zmq_msg_data(&self->frame[FRAME_KEY]), size);
//      self->key[size] = 0;
//    }
//    return self->key;
//  } else
//    return NULL;
//}
//
// void kvmsg_set_key(kvmsg_t *self, char *key) {
//  assert(self);
//  zmq_msg_t *msg = &self->frame[FRAME_KEY];
//  if (self->present[FRAME_KEY])
//    zmq_msg_close(msg);
//  zmq_msg_init_size(msg, strlen(key));
//  memcpy(zmq_msg_data(msg), key, strlen(key));
//  self->present[FRAME_KEY] = 1;
//}
//
// void kvmsg_fmt_key(kvmsg_t *self, char *format, ...) {
//  char value[KVMSG_KEY_MAX + 1];
//  va_list args;
//
//  assert(self);
//  va_start(args, format);
//  vsnprintf(value, KVMSG_KEY_MAX, format, args);
//  va_end(args);
//  kvmsg_set_key(self, value);
//}
//
////  .split sequence methods
////  These two methods let the caller get and set the message sequence number:
//
// int64_t kvmsg_sequence(kvmsg_t *self) {
//  assert(self);
//  if (self->present[FRAME_SEQ]) {
//    assert(zmq_msg_size(&self->frame[FRAME_SEQ]) == 8);
//    byte *source = zmq_msg_data(&self->frame[FRAME_SEQ]);
//    int64_t sequence =
//        ((int64_t)(source[0]) << 56) + ((int64_t)(source[1]) << 48) +
//        ((int64_t)(source[2]) << 40) + ((int64_t)(source[3]) << 32) +
//        ((int64_t)(source[4]) << 24) + ((int64_t)(source[5]) << 16) +
//        ((int64_t)(source[6]) << 8) + (int64_t)(source[7]);
//    return sequence;
//  } else
//    return 0;
//}
//
// void kvmsg_set_sequence(kvmsg_t *self, int64_t sequence) {
//  assert(self);
//  zmq_msg_t *msg = &self->frame[FRAME_SEQ];
//  if (self->present[FRAME_SEQ])
//    zmq_msg_close(msg);
//  zmq_msg_init_size(msg, 8);
//
//  byte *source = zmq_msg_data(msg);
//  source[0] = (byte)((sequence >> 56) & 255);
//  source[1] = (byte)((sequence >> 48) & 255);
//  source[2] = (byte)((sequence >> 40) & 255);
//  source[3] = (byte)((sequence >> 32) & 255);
//  source[4] = (byte)((sequence >> 24) & 255);
//  source[5] = (byte)((sequence >> 16) & 255);
//  source[6] = (byte)((sequence >> 8) & 255);
//  source[7] = (byte)((sequence)&255);
//
//  self->present[FRAME_SEQ] = 1;
//}
//
////  .split message body methods
////  These methods let the caller get and set the message body as a
////  fixed string and as a printf formatted string:
//
// byte *kvmsg_body(kvmsg_t *self) {
//  assert(self);
//  if (self->present[FRAME_BODY])
//    return (byte *)zmq_msg_data(&self->frame[FRAME_BODY]);
//  else
//    return NULL;
//}
//
// void kvmsg_set_body(kvmsg_t *self, byte *body, size_t size) {
//  assert(self);
//  zmq_msg_t *msg = &self->frame[FRAME_BODY];
//  if (self->present[FRAME_BODY])
//    zmq_msg_close(msg);
//  self->present[FRAME_BODY] = 1;
//  zmq_msg_init_size(msg, size);
//  memcpy(zmq_msg_data(msg), body, size);
//}
//
// void kvmsg_fmt_body(kvmsg_t *self, char *format, ...) {
//  char value[255 + 1];
//  va_list args;
//
//  assert(self);
//  va_start(args, format);
//  vsnprintf(value, 255, format, args);
//  va_end(args);
//  kvmsg_set_body(self, (byte *)value, strlen(value));
//}
//
////  .split size method
////  This method returns the body size of the most recently read message,
////  if any exists:
//
// size_t kvmsg_size(kvmsg_t *self) {
//  assert(self);
//  if (self->present[FRAME_BODY])
//    return zmq_msg_size(&self->frame[FRAME_BODY]);
//  else
//    return 0;
//}
//
////  .split store method
////  This method stores the key-value message into a hash map, unless
////  the key and value are both null. It nullifies the {{kvmsg}} reference
////  so that the object is owned by the hash map, not the caller:
//
// void kvmsg_store(kvmsg_t **self_p, zhash_t *hash) {
//  assert(self_p);
//  if (*self_p) {
//    kvmsg_t *self = *self_p;
//    assert(self);
//    if (self->present[FRAME_KEY] && self->present[FRAME_BODY]) {
//      zhash_update(hash, kvmsg_key(self), self);
//      zhash_freefn(hash, kvmsg_key(self), kvmsg_free);
//    }
//    *self_p = NULL;
//  }
//}
//
////  .split dump method
////  This method prints the key-value message to stderr for
////  debugging and tracing:
//
// void kvmsg_dump(kvmsg_t *self) {
//  if (self) {
//    if (!self) {
//      fprintf(stderr, "NULL");
//      return;
//    }
//    size_t size = kvmsg_size(self);
//    byte *body = kvmsg_body(self);
//    fprintf(stderr, "[seq:%" PRId64 "]", kvmsg_sequence(self));
//    fprintf(stderr, "[key:%s]", kvmsg_key(self));
//    fprintf(stderr, "[size:%zd] ", size);
//    int char_nbr;
//    for (char_nbr = 0; char_nbr < size; char_nbr++)
//      fprintf(stderr, "%02X", body[char_nbr]);
//    fprintf(stderr, "\n");
//  } else
//    fprintf(stderr, "NULL message\n");
//}
//
////  .split test method
////  It's good practice to have a self-test method that tests the class; this
////  also shows how it's used in applications:
//
// int kvmsg_test(int verbose) {
//  kvmsg_t *kvmsg;
//
//  printf(" * kvmsg: ");
//
//  //  Prepare our context and sockets
//  zctx_t *ctx = zctx_new();
//  void *output = zsocket_new(ctx, ZMQ_DEALER);
//  int rc = zmq_bind(output, "ipc://kvmsg_selftest.ipc");
//  assert(rc == 0);
//  void *input = zsocket_new(ctx, ZMQ_DEALER);
//  rc = zmq_connect(input, "ipc://kvmsg_selftest.ipc");
//  assert(rc == 0);
//
//  zhash_t *kvmap = zhash_new();
//
//  //  Test send and receive of simple message
//  kvmsg = kvmsg_new(1);
//  kvmsg_set_key(kvmsg, "key");
//  kvmsg_set_body(kvmsg, (byte *)"body", 4);
//  if (verbose)
//    kvmsg_dump(kvmsg);
//  kvmsg_send(kvmsg, output);
//  kvmsg_store(&kvmsg, kvmap);
//
//  kvmsg = kvmsg_recv(input);
//  if (verbose)
//    kvmsg_dump(kvmsg);
//  assert(streq(kvmsg_key(kvmsg), "key"));
//  kvmsg_store(&kvmsg, kvmap);
//
//  //  Shutdown and destroy all objects
//  zhash_destroy(&kvmap);
//  zctx_destroy(&ctx);
//
//  printf("OK\n");
//  return 0;
//}
//