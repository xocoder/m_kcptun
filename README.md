
[![Powered][1]][2]  [![MIT licensed][3]][4]  [![Build Status][5]][6]

[1]: https://img.shields.io/badge/KCP-Powered-blue.svg
[2]: https://github.com/skywind3000/kcp

[3]: https://img.shields.io/badge/license-MIT-blue.svg
[4]: LICENSE

[5]: https://travis-ci.org/lalawue/m_kcptun.svg?branch=master
[6]: https://travis-ci.org/lalawue/m_kcptun



# DEPRECATE

suggest [BBR](https://github.com/google/bbr) congestion control algorithm instead.



# About

m_kcptun was a cross platform secure TCP tunnel with M:N multiplexing base on [KCP](https://github.com/skywind3000/kcp), [m_net](https://github.com/lalawue/m_net) and [m_foundation](https://github.com/lalawue/m_foundation).

Support Linux/MacOS/FreeBSD/Windows





# Features

- support RC4 crypto
- support TCP multiplexing
- support KCP tunning, simplify with fast mode
- support Reed-Solomon erasure codes, default RS(11, 2)
- support multiple UDP transport path
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
# ./local_kcp.out  -l 'TCP_LOCAL_IP:6782' -r 'KCP_SERVER_IP:3234' -fast 3 -key '65423187'  # in local
# ./remote_kcp.out -l 'KCP_SERVER_IP:3234' -r 'TCP_REMOTE_IP:6783' -fast 3 -key '65423187' # in server
```

it will establish a **TCP <-> UDP(KCP) <-> TCP** conneciton like this:

> Local <-> **Client (tcp_in:6782, udp_out:ANY) <-> Server (udp_in:3234, tcp_out:ANY)** <-> Remote (tcp_in:6783)
