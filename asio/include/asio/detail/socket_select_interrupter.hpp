//
// socket_select_interrupter.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2005 Christopher M. Kohlhoff (chris@kohlhoff.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_SOCKET_SELECT_INTERRUPTER_HPP
#define ASIO_DETAIL_SOCKET_SELECT_INTERRUPTER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/push_options.hpp"

#include "asio/error.hpp"
#include "asio/detail/socket_holder.hpp"
#include "asio/detail/socket_ops.hpp"
#include "asio/detail/socket_types.hpp"

namespace asio {
namespace detail {

class socket_select_interrupter
{
public:
  // Constructor.
  socket_select_interrupter()
  {
    socket_holder acceptor(socket_ops::socket(AF_INET, SOCK_STREAM,
          IPPROTO_TCP));
    if (acceptor.get() == invalid_socket)
      throw asio::error(socket_ops::get_error());

    int opt = 1;
    socket_ops::setsockopt(acceptor.get(), SOL_SOCKET, SO_REUSEADDR, &opt,
        sizeof(opt));

    inet_addr_v4_type addr;
    socket_addr_len_type addr_len = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = 0;
    if (socket_ops::bind(acceptor.get(), (const socket_addr_type*)&addr,
          addr_len) == socket_error_retval)
      throw asio::error(socket_ops::get_error());

    if (getsockname(acceptor.get(), (socket_addr_type*)&addr, &addr_len)
        == socket_error_retval)
      throw asio::error(socket_ops::get_error());

    if (socket_ops::listen(acceptor.get(), SOMAXCONN) == socket_error_retval)
      throw asio::error(socket_ops::get_error());

    socket_holder client(socket_ops::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (client.get() == invalid_socket)
      throw asio::error(socket_ops::get_error());

    if (socket_ops::connect(client.get(), (const socket_addr_type*)&addr,
          addr_len) == socket_error_retval)
      throw asio::error(socket_ops::get_error());

    socket_holder server(socket_ops::accept(acceptor.get(), 0, 0));
    if (server.get() == invalid_socket)
      throw asio::error(socket_ops::get_error());
    
    ioctl_arg_type non_blocking = 1;
    if (socket_ops::ioctl(client.get(), FIONBIO, &non_blocking))
      throw asio::error(socket_ops::get_error());

    opt = 1;
    socket_ops::setsockopt(client.get(), IPPROTO_TCP, TCP_NODELAY, &opt,
        sizeof(opt));

    non_blocking = 1;
    if (socket_ops::ioctl(server.get(), FIONBIO, &non_blocking))
      throw asio::error(socket_ops::get_error());

    opt = 1;
    socket_ops::setsockopt(server.get(), IPPROTO_TCP, TCP_NODELAY, &opt,
        sizeof(opt));

    read_descriptor_ = server.release();
    write_descriptor_ = client.release();
  }

  // Destructor.
  ~socket_select_interrupter()
  {
    if (read_descriptor_ != invalid_socket)
      socket_ops::close(read_descriptor_);
    if (write_descriptor_ != invalid_socket)
      socket_ops::close(write_descriptor_);
  }

  // Interrupt the select call.
  void interrupt()
  {
    char byte = 0;
    socket_ops::send(write_descriptor_, &byte, 1, 0);
  }

  // Reset the select interrupt. Returns true if the call was interrupted.
  bool reset()
  {
    char data[1024];
    int bytes_read = socket_ops::recv(read_descriptor_, data, sizeof(data), 0);
    bool was_interrupted = (bytes_read > 0);
    while (bytes_read == sizeof(data))
      bytes_read = socket_ops::recv(read_descriptor_, data, sizeof(data), 0);
    return was_interrupted;
  }

  // Get the read descriptor to be passed to select.
  socket_type read_descriptor() const
  {
    return read_descriptor_;
  }

private:
  // The read end of a connection used to interrupt the select call. This file
  // descriptor is passed to select such that when it is time to stop, a single
  // byte will be written on the other end of the connection and this
  // descriptor will become readable.
  socket_type read_descriptor_;

  // The write end of a connection used to interrupt the select call. A single
  // byte may be written to this to wake up the select which is waiting for the
  // other end to become readable.
  socket_type write_descriptor_;
};

} // namespace detail
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_DETAIL_SOCKET_SELECT_INTERRUPTER_HPP
