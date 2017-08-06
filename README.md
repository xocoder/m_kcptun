
# About

m_kcptun was a cross platform TCP tunnel base on [kcp](https://github.com/skywind3000/kcp) and [m_net](https://github.com/lalawue/m_net).

Support MacOS/Linux, Windows will be support soon.





# Features

- support kcp tunning, and fast
- little memory footsprint
- using kqueue/epoll/select underlining





# Usage

command below:

```
Client: ./local_kcp.out -r "KCP_SERVER_IP:3234" -l ":6782"
Server: ./remote_kcp.out -t "TARGET_TCP_IP:6782" -l ":3234"
```

will establish a **TCP - UDP(KCP) - TCP** conneciton like this:

> Local -> **Client (tcp_in:6782, udp_out:3234) -> Server (udp_in:3234, tcp_out:6782)** -> Remote (tcp_in:6782) 





# Todo

- internal protocol to indicate stream open/close between local/remote tcp connection (reset tcp conneciton)
- support Windows
- provide crypto option
