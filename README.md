
[![Powered][1]][2]  [![MIT licensed][3]][4]  [![Build Status][5]][6]

[1]: https://img.shields.io/badge/KCP-Powered-blue.svg
[2]: https://github.com/skywind3000/kcp

[3]: https://img.shields.io/badge/license-MIT-blue.svg
[4]: LICENSE

[5]: https://travis-ci.org/lalawue/m_kcptun.svg?branch=master
[6]: https://travis-ci.org/lalawue/m_kcptun


# About

m_kcptun was a cross platform secure TCP tunnel with M:N multiplexing base on [KCP](https://github.com/skywind3000/kcp) and [m_net](https://github.com/lalawue/m_net).

Support Linux/MacOS/FreeBSD/Windows





# Features

- support RC4 crypto
- support TCP multiplexing
- support KCP tunning, simplify with fast mode
- little memory footsprint
- using kqueue/epoll/select underlying





# QuickStart

Download precompiled [Release](https://github.com/lalawue/m_kcptun/releases).





# Compile & Running


1. in MacOS/Linux, first compile the source
```
# git clone https://github.com/lalawue/m_kcptun.git
# cd m_kcptun
# git submodule update --init --recursive
# make release
```

in Windows, using VS2017 under vc dir, the .vcxproj just ready for client side.

in FreeBSD, using gmake.



2. run remote & local

```
# ./remote_kcp.out -t "TARGET_TCP_IP:6783" -l ":3234" -fast 3 -key "65423187" # in server
# ./local_kcp.out -t "KCP_SERVER_IP:3234" -l ":6782" -fast 3 -key "65423187"  # in local
```

it will establish a **TCP - UDP(KCP) - TCP** conneciton like this:

> Local -> **Client (tcp_in:6782, udp_out:ANY) -> Server (udp_in:3234, tcp_out:ANY)** -> Remote (tcp_in:6783)
