# Multi-Role TOE

## 目标

1. 支持多Role APP的TCP-Offload-Engine

## Note
1. ICMP Server 模块只计算ICMP checksum，不计算Ipv4 checksum

### 支持多IP所做的修改
1. Hash Table中的Key需要完整记录四元组，而不是之前的三元组，所以keySize=96

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