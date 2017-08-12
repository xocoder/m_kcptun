
# About

m_kcptun was a cross platform TCP tunnel with M:N multiplexing base on [kcp](https://github.com/skywind3000/kcp) and [m_net](https://github.com/lalawue/m_net).

Support MacOS/Linux, Windows will be support soon.





# Features

- support RC4 crypto
- support TCP multiplexing
- support kcp tunning, simplify with fast mode
- little memory footsprint
- using kqueue/epoll/select underlying





# Usage

command below:

```
Client: ./local_kcp.out -r "KCP_SERVER_IP:3234" -l ":6782" -fast 3 -key "65423187"
Server: ./remote_kcp.out -t "TARGET_TCP_IP:6782" -l ":3234" -fast 3 -key "65423187"
```

will establish a **TCP - UDP(KCP) - TCP** conneciton like this:

> Local -> **Client (tcp_in:6782, udp_out:ANY) -> Server (udp_in:3234, tcp_out:ANY)** -> Remote (tcp_in:6782) 





# Todo

- support Windows
