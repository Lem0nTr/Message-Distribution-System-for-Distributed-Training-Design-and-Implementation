#Message-Distribution-System-for-Distributed-Training-Design-and-Implementation
这是一个本科的毕业设计项目，課題名字是 面向分布式训练的消息分发系统。系统使用QT建立了跨平台计的通信节点，满足了跨平台、跨集群的场景的消息分发需求。内部包含了一套分层联邦学习的训练架构。包含中心服务器、边缘服务器以及客户端三种节点。同时在不同节点之间使用QT建立一个高效的消息分发系统，中心服务器与边缘服务器之间使用了Socket进行沟通，边缘服务器与客户端之间使用了MQTT进行沟通。
关于项目中的ip设置以及模型的设置和数据集的设置都是写死在代码中的，所以最好在部署之前更改一下这些地方。
关于MQTT的使用，代码中是连接了EMQX服务器，利用EMQX服务器搭建的MQTT集群。

最后测试的环境：
qt版本：qt6.5.3
操作系统：Ubuntu24.04.2
浏览器：Microsoft Edge 136.0.3240.50
模型：YOLOv8
浏览器所用系统：Windows	11家庭共享版
CPU：AMD Ryzen 7 7745HX with Radeon Graphics 
显卡：4060Laptop
RAM：16.0 GB
