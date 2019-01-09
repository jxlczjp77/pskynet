# pskynet
skynet框架C++移植版

最近研究了一下云风的skynet框架，光看代码总觉得不是很明白，想调试一下linux又没有用着顺手的工具，再加上对lua不是太熟，所以花了几天时间用C++在windows下移植了框架部分。

得益于skynet框架设计的简洁，移植工作并没有太大障碍。限于对skynet的理解有限，部分逻辑按照自己的想法写了，希望有兴趣的同学可以一起看看。

## 项目依赖

```
1、boost。 读写锁，自旋锁，无锁队列
2、libuv。 网络部分用libuv替换
3、lua5.3官方版。 Debug模式下默认使用lua5.3官方版本，方便内存泄漏检测
4、vld。 windows下的内存泄漏检测库
```
所有依赖项除了boost以外，都包含在工程目录里面了。boost路径请在vs2013的全局配置中配置好。

## 编译

目前仅在windows下使用vs2013编译，要支持其他平台和编译器应该也不难。

Debug采用MDd方式，对应boost编译配置：
b2 runtime-link=shared link=static toolset=msvc-12.0 variant=debug

Relese模式采用MT方式，对应boost编译配置：
b2 runtime-link=static link=static toolset=msvc-12.0 variant=release

## 测试

1：启动skynet服务
```
cd build_win32\Win32\Debug
pskynet ../../../examples/config
```
注意：Ctrl + C可以退出服务。由于还不明白skynet的退出机制是怎么处理的，所以加了这部分处理逻辑，仅简单的关闭所有IO句柄，退出定时器和监控线程，等待工作线程处理完剩余消息后退出。

2：启动测试客户端
```
lua ../../../examples/client.lua
```
## skynet相关链接

* Read Wiki https://github.com/cloudwu/skynet/wiki
* The FAQ in wiki https://github.com/cloudwu/skynet/wiki/FAQ
