# Multi-Role TOE

## 目标

1. 支持多Role APP的TCP-Offload-Engine


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
