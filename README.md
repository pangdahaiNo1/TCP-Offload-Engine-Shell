# Multi-Role TOE

## 目标

1. 支持多Role APP的TCP-Offload-Engine

## Note
1. ICMP Server 模块只计算ICMP checksum，不计算Ipv4 checksum
2. TOE模拟的时候，第一个包的SEQ number的值位于`hls/toe_connfig.hpp`文件的`INIT_SEQ_NUMBER`

### 支持多IP所做的修改
1. Hash Table中的Key需要完整记录四元组，而不是之前的三元组，所以keySize=96
2. CamTuple类型使用FourTuple
3. Session Lookup Controller中操作的key为四元组
4. MockCam中模拟的KV Map，其Key为四元组
5. PortTable应该记录每个IP分配的Port，但也可以保持现状，即所有端口均不同
6. Tx app intf中App打开TCP连接的请求中，应该携带该App的IP地址
7. Net APP中的my_ip_addr应该接入GPIO，以此来使得用户可以自己配置该应用的IP地址


## 附录
### 1. 编码风格
参考[Google 开源项目风格指南-命名规范](https://zh-google-styleguide.readthedocs.io/en/latest/google-cpp-styleguide/naming/#macro-names)
1. 常量： 前缀k，混合大小写 
2. 宏定义：全大写
3. 枚举值：全大写
4. 文件：小写，配合下划线
5. 函数/类：每个单词首字母大写，没有下划线
6. 变量：全小写，配合下划线

7. 模块顶层：前缀文件名相同，全小写，后面加`_top`后缀。模块顶层函数只定义模块接口的`pragma`

### 2. TcpDump 常用命令
1. 读入PCAP后过滤
```bash
tcpdump -r arp.pcap -w arp_out.pcap ether dst 00:0a:35:02:9d:e5 or ff:ff:ff:ff:ff:ff
```