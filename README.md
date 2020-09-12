# dnet

ÏµÍ³ÍøÂçÅäÖÃ
ulimit -n 102400
sysctl net.ipv4.ip_local_port_range="15000 61000"
sysctl net.ipv4.tcp_fin_timeout=30
sysctl net.ipv4.tcp_tw_reuse=1
sysctl net.core.somaxconn=10240
